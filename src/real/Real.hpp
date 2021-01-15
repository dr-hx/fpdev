#ifndef __REAL__HPP__
#define __REAL__HPP__

/**
 * This file contains interfaces for shadow execution.
 * There are two types of interfaces, i.e., eager evaluation interface (EEI) and lazy evaluation interface (LEI).
 * EEI is placed in the namespace real. The main classes are listed as follows.
 *   - Real: the high precision number class, with operator overloading.
 *   - ShadowState: shadow data structure.
 * LEI is placed in the namespace real::lazy. The main classes are listed as follows.
 *   - Real: the high precision number class, with operator overloading. Note that the arithmetic operators of Real
 *       do not cause the evalution instancely. Instead, it builds the expression tree for lazy evaluation.
 *   - BinaryOperator: binary operations, i.e., +, -, *, /
 *   - TernaryOperator: ternary operations, i.e., FMA, FMS.
 *   - Evaluator: the executor of expression trees.
 *   - ExpressionSlot: support caching expression trees. We must define a global ExpressionSlot for each expression.
 * 
 * Notes:
 * 2021/1/7. LEI is not better than EEI in simple cases without compiler optimazation. 
 *           EEI has already being well optimized to avoid unnecessary storage and calculation.
 *           The trick of pool (real::util::ValuePool) has been used to optimize object allocation and deallocation.
 */

#include <mpfr.h>
#include <stdlib.h>
#include <array>
#include <vector>
#include <list>
#include <stack>
#include <unordered_map>
#include <sys/time.h>
#include <iostream>

#define real_likely(x) __builtin_expect((x), 1)
#define real_unlikely(x) __builtin_expect((x), 0)
typedef unsigned long uint64;

#define PUSH_HEAD(nh, h) \
    nh->next = h;        \
    h = nh;
#define POP_HEAD(h) h = h->next;

//#define KEEP_ORIGINAL true
// #define DELEGATE_TO_POOL true
#define KEY_SHIFT(k) (((uint64)k)>>3)


namespace real
{
    namespace util
    {
        template <typename V>
        class SlotValueInitializer
        {
        public:
            inline void construct(V &v) {}
            inline void destruct(V &v) {}
        };

        template <typename V, int batchSize = 128, typename _Alloc = SlotValueInitializer<V>>
        class ValuePool
        {
        public:
            typedef V *value_ptr;
            static ValuePool INSTANCE;

        private:
            _Alloc initializer;
            class Slot
            {
            public:
                value_ptr value;
                Slot *next;
                Slot(value_ptr v) : value(v) {}
            };
            Slot *availableHead;
            Slot *usedHead;
            Slot *memoryHead;

            void expand()
            {
                value_ptr m = new V[batchSize];
                Slot *mh = new Slot(m);
                PUSH_HEAD(mh, memoryHead)

                for (int i = 0; i < batchSize; i++)
                {
                    if (std::is_same<decltype(initializer), SlotValueInitializer<V>>::value == false)
                    {
                        initializer.construct(m[i]);
                    }
                    Slot *fh = new Slot(&m[i]);
                    PUSH_HEAD(fh, availableHead)
                }
            }

        public:
            value_ptr get()
            {
                Slot *slot;
                if (real_unlikely(availableHead == nullptr))
                {
                    expand();
                }
                slot = availableHead;
                POP_HEAD(availableHead)
                PUSH_HEAD(slot, usedHead)
                return slot->value;
            }

            void put(value_ptr pt)
            {
                Slot *slot;
                if (real_unlikely(usedHead == nullptr))
                {
                    slot = new Slot(pt);
                }
                else
                {
                    slot = usedHead;
                    POP_HEAD(usedHead)
                    slot->value = pt;
                }
                PUSH_HEAD(slot, availableHead)
            }
            ValuePool() : availableHead(nullptr), usedHead(nullptr), memoryHead(nullptr)
            {
                expand();
            }
            ~ValuePool()
            {
                while (availableHead != nullptr)
                {
                    Slot *n = availableHead;
                    POP_HEAD(availableHead)
                    delete n;
                }
                while (usedHead != nullptr)
                {
                    Slot *n = usedHead;
                    POP_HEAD(usedHead)
                    delete n;
                }
                while (memoryHead != nullptr)
                {
                    Slot *n = memoryHead;
                    POP_HEAD(memoryHead)
                    if (std::is_same<decltype(initializer), SlotValueInitializer<V>>::value == false)
                    {
                        for (int i = 0; i < batchSize; i++)
                        {
                            initializer.destruct((n->value)[i]);
                        }
                    }
                    delete[] n->value;
                    delete n;
                }
            }
        };
        template <typename V, int batchSize, typename A>
        ValuePool<V, batchSize, A> ValuePool<V, batchSize, A>::INSTANCE;
    }; // namespace util

    class Real;

    template <typename RealType>
    using RealPool = util::ValuePool<RealType>;


    template <typename Key, typename RealType, int cacheSize = 256, int mask = cacheSize - 1>
    class VariableMap
    {
        using __value_ptr = RealType*;
#ifdef DELEGATE_TO_POOL
        using __value_type = __value_ptr;
#else
        using __value_type = RealType;
#endif
    public:
        static VariableMap<Key, RealType, cacheSize, mask> INSTANCE;

    private:
        std::unordered_map<Key, __value_type> map;
        
        struct RealCache
        {
            Key address;
            __value_ptr real_ptr;
            RealCache()
            {
                address = nullptr;
                real_ptr = nullptr;
            }
        };
        RealCache cache[cacheSize];

    public:
        RealType &operator[](Key address)
        {
            int index = KEY_SHIFT(address) & mask;
            RealCache &c = cache[index];
            if (c.address != address)
            {
#ifdef DELEGATE_TO_POOL
                RealType &ptr = *map[address];
#else
                RealType &ptr = map[KEY_SHIFT(address)];
#endif
                c.address = address;
                c.real_ptr = &ptr;
            }
            return *c.real_ptr;
        }
        void def(Key address)
        {
#ifdef DELEGATE_TO_POOL
            __value_ptr vp = RealPool<RealType>::INSTANCE.get();
            map[address] = vp;
#else
            map[KEY_SHIFT(address)]; // create a real with default constructor
#endif
        }
        void undef(Key address)
        {
            int index = KEY_SHIFT(address) & mask;
            RealCache &c = cache[index];
            if (c.address == address)
            {
                c.address = nullptr;
                c.real_ptr = nullptr;
            }
#ifdef DELEGATE_TO_POOL
            auto it = map.find(KEY_SHIFT(address));
            RealPool<RealType>::INSTANCE.put(it->second);
            map.erase(it);
#else
            map.erase(KEY_SHIFT(address));
#endif
        }
    };
    template <typename K, typename T, int c, int m>
    VariableMap<K, T, c, m> VariableMap<K, T, c, m>::INSTANCE;

    struct ShadowState
    {
        double originalValue;
        mpfr_t shadowValue;
        double avgRelativeError;
    };
    typedef ShadowState *sval_ptr;

    template <int p>
    struct ShadowSlotInitializer
    {
        inline void construct(ShadowState &v)
        {
            mpfr_init2(v.shadowValue, p);
        }
        inline void destruct(ShadowState &v)
        {
            mpfr_clear(v.shadowValue);
        }
    };

    using ShadowPool = util::ValuePool<ShadowState, 128, ShadowSlotInitializer<120>>;

    class Real
    {
    public:
        sval_ptr shadow;

    public:
        Real()
        {
            shadow = ShadowPool::INSTANCE.get();
        }
        Real(double v)
        {
            shadow = ShadowPool::INSTANCE.get();
            mpfr_set_d(shadow->shadowValue, v, MPFR_RNDN);
            shadow->originalValue = v;
            shadow->avgRelativeError = 0;
        }

        Real(Real &&r) noexcept
        {
            this->shadow = r.shadow;
            r.shadow = ShadowPool::INSTANCE.get();;
            RealPool<Real>::INSTANCE.put(&r);
        }

        Real(const Real &r)
        {
            shadow = ShadowPool::INSTANCE.get();
            mpfr_set(shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
            shadow->originalValue = r.shadow->originalValue;
            shadow->avgRelativeError = 0;
        }

        ~Real()
        {
            if (shadow != nullptr)
            {
                ShadowPool::INSTANCE.put(shadow);
                shadow = nullptr;
            }
        }

        inline friend Real &&operator+(const Real &l, const Real &r)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            mpfr_add(res.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue + r.shadow->originalValue;
#endif
            return std::move(res);
        }

        inline friend Real &&operator+(Real &&l, const Real &r)
        {
            mpfr_add(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue += r.shadow->originalValue;
#endif
            return std::move(l);
        }

        inline friend Real &&operator+(const Real &l, Real &&r)
        {
            mpfr_add(r.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            r.shadow->originalValue += l.shadow->originalValue;
#endif
            return std::move(r);
        }

        inline friend Real &&operator+(Real &&l, Real &&r)
        {
            mpfr_add(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue += r.shadow->originalValue;
#endif
            RealPool<Real>::INSTANCE.put(&r); // r should be an useless temp
            return std::move(l);
        }

        inline friend Real &&operator-(const Real &l, const Real &r)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            mpfr_sub(res.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue - r.shadow->originalValue;
#endif
            return std::move(res);
        }

        inline friend Real &&operator-(Real &&l, const Real &r)
        {
            mpfr_sub(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue -= r.shadow->originalValue;
#endif
            return std::move(l);
        }
        inline friend Real &&operator-(Real &&l, Real &&r)
        {
            mpfr_sub(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue -= r.shadow->originalValue;
#endif
            RealPool<Real>::INSTANCE.put(&r);
            return std::move(l);
        }
        inline friend Real &&operator-(const Real &l, Real &&r)
        {
            mpfr_sub(r.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            r.shadow->originalValue = l.shadow->originalValue - r.shadow->originalValue;
#endif
            return std::move(r);
        }
        inline friend Real &&operator*(const Real &l, const Real &r)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            mpfr_mul(res.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue * r.shadow->originalValue;
#endif
            return std::move(res);
        }

        inline friend Real &&operator*(Real &&l, const Real &r)
        {
            mpfr_mul(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue *= r.shadow->originalValue;
#endif
            return std::move(l);
        }

        inline friend Real &&operator*(const Real &l, Real &&r)
        {
            mpfr_mul(r.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            r.shadow->originalValue *= l.shadow->originalValue;
#endif
            return std::move(r);
        }

        inline friend Real &&operator*(Real &&l, Real &&r)
        {
            mpfr_mul(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue *= r.shadow->originalValue;
#endif
            RealPool<Real>::INSTANCE.put(&r);
            return std::move(l);
        }

        inline friend Real &&operator/(const Real &l, const Real &r)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            mpfr_div(res.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue / r.shadow->originalValue;
#endif
            return std::move(res);
        }

        inline friend Real &&operator/(Real &&l, const Real &r)
        {
            mpfr_div(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue /= r.shadow->originalValue;
#endif
            return std::move(l);
        }
        inline friend Real &&operator/(Real &&l, Real &&r)
        {
            mpfr_div(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue /= r.shadow->originalValue;
#endif
            RealPool<Real>::INSTANCE.put(&r);
            return std::move(l);
        }
        inline friend Real &&operator/(const Real &l, Real &&r)
        {
            mpfr_div(r.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            r.shadow->originalValue = l.shadow->originalValue / r.shadow->originalValue;
#endif
            return std::move(r);
        }

        inline friend Real &&operator+(Real &&l, const double i)
        {
            mpfr_add_d(l.shadow->shadowValue, l.shadow->shadowValue, i, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue += i;
#endif
            return std::move(l);
        }
        inline friend Real &&operator+(const Real &l, const double i)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            mpfr_add_d(res.shadow->shadowValue, l.shadow->shadowValue, i, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue + i;
#endif
            return std::move(res);
        }
        inline friend Real &&operator+(const double i, Real &&l)
        {
            mpfr_add_d(l.shadow->shadowValue, l.shadow->shadowValue, i, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue = i + l.shadow->originalValue;
#endif
            return std::move(l);
        }
        inline friend Real &&operator+(const double i, const Real &l)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            mpfr_add_d(res.shadow->shadowValue, l.shadow->shadowValue, i, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            res.shadow->originalValue = i + l.shadow->originalValue;
#endif
            return std::move(res);
        }

        inline friend Real &&operator-(Real &&l, const double i)
        {
            mpfr_sub_d(l.shadow->shadowValue, l.shadow->shadowValue, i, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue -= i;
#endif
            return std::move(l);
        }
        inline friend Real &&operator-(const Real &l, const double i)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            mpfr_sub_d(res.shadow->shadowValue, l.shadow->shadowValue, i, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue - i;
#endif
            return std::move(res);
        }
        inline friend Real &&operator-(const double i, Real &&l)
        {
            mpfr_d_sub(l.shadow->shadowValue, i, l.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue = i - l.shadow->originalValue;
#endif
            return std::move(l);
        }
        inline friend Real &&operator-(const double i, const Real &l)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            mpfr_d_sub(res.shadow->shadowValue, i, l.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            res.shadow->originalValue = i - l.shadow->originalValue;
#endif
            return std::move(res);
        }

        inline friend Real &&operator*(Real &&l, const double i)
        {
            mpfr_mul_d(l.shadow->shadowValue, l.shadow->shadowValue, i, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue *= i;
#endif
            return std::move(l);
        }
        inline friend Real &&operator*(const Real &l, const double i)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            mpfr_mul_d(res.shadow->shadowValue, l.shadow->shadowValue, i, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue * i;
#endif
            return std::move(res);
        }
        inline friend Real &&operator*(const double i, Real &&l)
        {
            mpfr_mul_d(l.shadow->shadowValue, l.shadow->shadowValue, i, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue = i * l.shadow->originalValue;
#endif
            return std::move(l);
        }
        inline friend Real &&operator*(const double i, const Real &l)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            mpfr_mul_d(res.shadow->shadowValue, l.shadow->shadowValue, i, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            res.shadow->originalValue = i * l.shadow->originalValue;
#endif
            return std::move(res);
        }

        inline friend Real &&operator/(Real &&l, const double i)
        {
            mpfr_div_d(l.shadow->shadowValue, l.shadow->shadowValue, i, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue /= i;
#endif
            return std::move(l);
        }
        inline friend Real &&operator/(const Real &l, const double i)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            mpfr_div_d(res.shadow->shadowValue, l.shadow->shadowValue, i, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue / i;
#endif
            return std::move(res);
        }
        inline friend Real &&operator/(const double i, Real &&l)
        {
            mpfr_d_div(l.shadow->shadowValue, i, l.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            l.shadow->originalValue = i / l.shadow->originalValue;
#endif
            return std::move(l);
        }
        inline friend Real &&operator/(const double i, const Real &l)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            mpfr_d_div(res.shadow->shadowValue, i, l.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            res.shadow->originalValue = i / l.shadow->originalValue;
#endif
            return std::move(res);
        }

        inline Real &operator=(const Real &r)
        {
            mpfr_set(shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            shadow->originalValue = r.shadow->originalValue;
#endif
            return *this;
        }
        inline Real &operator=(Real &&r)
        {
            mpfr_swap(shadow->shadowValue, r.shadow->shadowValue);
#ifdef KEEP_ORIGINAL
            shadow->originalValue = r.shadow->originalValue;
#endif
            RealPool<Real>::INSTANCE.put(&r);
            return *this;
        }
        inline Real &operator=(const double r)
        {
            mpfr_set_d(shadow->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            shadow->originalValue = r;
#endif
            return *this;
        }

        inline Real &operator+=(const Real &r)
        {
            mpfr_add(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            shadow->originalValue += r.shadow->originalValue;
#endif
            return *this;
        }

        inline Real &operator-=(const Real &r)
        {
            mpfr_sub(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            shadow->originalValue -= r.shadow->originalValue;
#endif
            return *this;
        }

        inline Real &operator*=(const Real &r)
        {
            mpfr_mul(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            shadow->originalValue *= r.shadow->originalValue;
#endif
            return *this;
        }

        inline Real &operator/=(const Real &r)
        {
            mpfr_div(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            shadow->originalValue /= r.shadow->originalValue;
#endif
            return *this;
        }

        inline Real &operator+=(const double &r)
        {
            mpfr_add_d(this->shadow->shadowValue, this->shadow->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            shadow->originalValue += r;
#endif
            return *this;
        }

        inline Real &operator-=(const double &r)
        {
            mpfr_sub_d(this->shadow->shadowValue, this->shadow->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            shadow->originalValue -= r;
#endif
            return *this;
        }

        inline Real &operator*=(const double &r)
        {
            mpfr_mul_d(this->shadow->shadowValue, this->shadow->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            shadow->originalValue *= r;
#endif
            return *this;
        }

        inline Real &operator/=(const double &r)
        {
            mpfr_div_d(this->shadow->shadowValue, this->shadow->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
            shadow->originalValue /= r;
#endif
            return *this;
        }

        friend std::ostream &operator<<(std::ostream &os, const Real &c)
        {
            static char buf[512];
            mpfr_snprintf(buf, 512, "%.32Re (original = %.16e)", c.shadow->shadowValue, c.shadow->originalValue);
            os << buf;
            return os;
        }
    };

    class Timer
    {
    private:
        timeval start_tv;
        timeval end_tv;
        long accumulation;

    public:
        Timer()
        {
            accumulation = 0;
        }
        void start()
        {
            gettimeofday(&start_tv, NULL);
        }
        void end()
        {
            gettimeofday(&end_tv, NULL);
            long time_use = (end_tv.tv_sec - start_tv.tv_sec) * 1000000 + (end_tv.tv_usec - start_tv.tv_usec);
            accumulation += time_use;
        }
        void reset()
        {
            accumulation = 0;
        }
        long acc()
        {
            return accumulation;
        }
    };

    namespace lazy
    {
        enum Operator
        {
            NOP = 0,
            ADD = 1,
            SUB = 2,
            MUL = 3,
            DIV = 4,
            FMA = 5,
            FMS = 6
        };
        template <typename Derived>
        struct RealBase;
        template <Operator opCode, typename L, typename R>
        struct BinaryOperator;
        template <Operator opCode, typename L, typename M, typename R>
        struct TernaryOperator;
        struct Real;

        template <typename E>
        struct ExpressionEvaluator
        {
            inline static void eval(const real::sval_ptr acc, const E &exp)
            {
                assert(false);
            }
            inline static const real::sval_ptr eval(const E &exp)
            {
                assert(false);
                return nullptr;
            }
        };

        template <typename T>
        struct trails
        {
            static const bool isRealBase = false;
            static const Operator opCode = Operator::NOP;
            typedef T DerivedType;
        };

        template <>
        struct trails<RealBase<Real>>
        {
            static const bool isRealBase = true;
            static const Operator opCode = Operator::NOP;
            using DerivedType = Real;
            using LType = void; // double or RealBase
            using MType = void; // double or RealBase
            using RType = void; // double or RealBase
        };

        template <Operator op, typename L, typename R>
        struct trails<RealBase<BinaryOperator<op, L, R>>>
        {
            static const bool isRealBase = true;
            static const Operator opCode = op;
            using LType = L;    // double or RealBase
            using MType = void; // double or RealBase
            using RType = R;    // double or RealBase
            using DerivedType = BinaryOperator<op, L, R>;
        };

        template <Operator op, typename L, typename M, typename R>
        struct trails<RealBase<TernaryOperator<op, L, M, R>>>
        {
            static const bool isRealBase = true;
            static const Operator opCode = op;
            using LType = L; // double or RealBase
            using MType = M; // double or RealBase
            using RType = R; // double or RealBase
            using DerivedType = TernaryOperator<op, L, M, R>;
        };

        template <typename Derived>
        struct RealBase
        {
            inline const Derived &derived() const { return *static_cast<const Derived *>(this); }

            // FMA1
            template <typename DerivedR>
            inline friend std::enable_if_t<trails<RealBase<Derived>>::opCode == Operator::MUL && trails<typename trails<RealBase<Derived>>::LType>::isRealBase && trails<typename trails<RealBase<Derived>>::RType>::isRealBase,
                                           TernaryOperator<Operator::FMA, typename trails<RealBase<Derived>>::LType, typename trails<RealBase<Derived>>::RType, RealBase<DerivedR>>>
            operator+(const RealBase<Derived> &l, const RealBase<DerivedR> &r)
            {
                const BinaryOperator<Operator::MUL, typename trails<RealBase<Derived>>::LType, typename trails<RealBase<Derived>>::RType> &lop = l.derived();
                return TernaryOperator<Operator::FMA, typename trails<RealBase<Derived>>::LType, typename trails<RealBase<Derived>>::RType, RealBase<DerivedR>>(lop.lhs, lop.rhs, r);
            }

            // FMA2
            template <typename DerivedR>
            inline friend std::enable_if_t<
                !(trails<RealBase<Derived>>::opCode == Operator::MUL && trails<typename trails<RealBase<Derived>>::LType>::isRealBase && trails<typename trails<RealBase<Derived>>::RType>::isRealBase) && (trails<RealBase<DerivedR>>::opCode == Operator::MUL && trails<typename trails<RealBase<DerivedR>>::LType>::isRealBase && trails<typename trails<RealBase<DerivedR>>::RType>::isRealBase),
                TernaryOperator<Operator::FMA, typename trails<RealBase<DerivedR>>::LType, typename trails<RealBase<DerivedR>>::RType, RealBase<Derived>>>
            operator+(const RealBase<Derived> &l, const RealBase<DerivedR> &r)
            {
                const BinaryOperator<Operator::MUL, typename trails<RealBase<DerivedR>>::LType, typename trails<RealBase<DerivedR>>::RType> &rop = r.derived();
                return TernaryOperator<Operator::FMA, typename trails<RealBase<DerivedR>>::LType, typename trails<RealBase<DerivedR>>::RType, RealBase<Derived>>(rop.lhs, rop.rhs, l);
            }

            template <typename DerivedR>
            inline friend std::enable_if_t<
                !(trails<RealBase<Derived>>::opCode == Operator::MUL && trails<typename trails<RealBase<Derived>>::LType>::isRealBase && trails<typename trails<RealBase<Derived>>::RType>::isRealBase) && !(trails<RealBase<DerivedR>>::opCode == Operator::MUL && trails<typename trails<RealBase<DerivedR>>::LType>::isRealBase && trails<typename trails<RealBase<DerivedR>>::RType>::isRealBase),
                BinaryOperator<Operator::ADD, RealBase<Derived>, RealBase<DerivedR>>>
            operator+(const RealBase<Derived> &l, const RealBase<DerivedR> &r)
            {
                return BinaryOperator<Operator::ADD, RealBase<Derived>, RealBase<DerivedR>>(l, r);
            }

            // FMS1
            template <typename DerivedR>
            inline friend std::enable_if_t<trails<RealBase<Derived>>::opCode == Operator::MUL && trails<typename trails<RealBase<Derived>>::LType>::isRealBase && trails<typename trails<RealBase<Derived>>::RType>::isRealBase,
                                           TernaryOperator<Operator::FMS, typename trails<RealBase<Derived>>::LType, typename trails<RealBase<Derived>>::RType, RealBase<DerivedR>>>
            operator-(const RealBase<Derived> &l, const RealBase<DerivedR> &r)
            {
                const BinaryOperator<Operator::MUL, typename trails<RealBase<Derived>>::LType, typename trails<RealBase<Derived>>::RType> &lop = l.derived();
                return TernaryOperator<Operator::FMS, typename trails<RealBase<Derived>>::LType, typename trails<RealBase<Derived>>::RType, RealBase<DerivedR>>(lop.lhs, lop.rhs, r);
            }

            // FMS2
            template <typename DerivedR>
            inline friend std::enable_if_t<
                !(trails<RealBase<Derived>>::opCode == Operator::MUL && trails<typename trails<RealBase<Derived>>::LType>::isRealBase && trails<typename trails<RealBase<Derived>>::RType>::isRealBase) && (trails<RealBase<DerivedR>>::opCode == Operator::MUL && trails<typename trails<RealBase<DerivedR>>::LType>::isRealBase && trails<typename trails<RealBase<DerivedR>>::RType>::isRealBase),
                TernaryOperator<Operator::FMS, typename trails<RealBase<DerivedR>>::LType, typename trails<RealBase<DerivedR>>::RType, RealBase<Derived>>>
            operator-(const RealBase<Derived> &l, const RealBase<DerivedR> &r)
            {
                const BinaryOperator<Operator::MUL, typename trails<RealBase<DerivedR>>::LType, typename trails<RealBase<DerivedR>>::RType> &rop = r.derived();
                return TernaryOperator<Operator::FMS, typename trails<RealBase<DerivedR>>::LType, typename trails<RealBase<DerivedR>>::RType, RealBase<Derived>>(rop.lhs, rop.rhs, l);
            }

            template <typename DerivedR>
            inline friend std::enable_if_t<
                !(trails<RealBase<Derived>>::opCode == Operator::MUL && trails<typename trails<RealBase<Derived>>::LType>::isRealBase && trails<typename trails<RealBase<Derived>>::RType>::isRealBase) && !(trails<RealBase<DerivedR>>::opCode == Operator::MUL && trails<typename trails<RealBase<DerivedR>>::LType>::isRealBase && trails<typename trails<RealBase<DerivedR>>::RType>::isRealBase),
                BinaryOperator<Operator::SUB, RealBase<Derived>, RealBase<DerivedR>>>
            operator-(const RealBase<Derived> &l, const RealBase<DerivedR> &r)
            {
                return BinaryOperator<Operator::SUB, RealBase<Derived>, RealBase<DerivedR>>(l, r);
            }

            template <typename DerivedR>
            inline friend BinaryOperator<Operator::MUL, RealBase<Derived>, RealBase<DerivedR>> operator*(const RealBase<Derived> &l, const RealBase<DerivedR> &r)
            {
                return BinaryOperator<Operator::MUL, RealBase<Derived>, RealBase<DerivedR>>(l, r);
            }

            template <typename DerivedR>
            inline friend BinaryOperator<Operator::DIV, RealBase<Derived>, RealBase<DerivedR>> operator/(const RealBase<Derived> &l, const RealBase<DerivedR> &r)
            {
                return BinaryOperator<Operator::DIV, RealBase<Derived>, RealBase<DerivedR>>(l, r);
            }

            inline friend BinaryOperator<Operator::ADD, RealBase<Derived>, double> operator+(const RealBase<Derived> &l, const double &r)
            {
                return BinaryOperator<Operator::ADD, RealBase<Derived>, double>(l, r);
            }
            inline friend BinaryOperator<Operator::ADD, RealBase<Derived>, double> operator+(const double &r, const RealBase<Derived> &l)
            {
                return BinaryOperator<Operator::ADD, RealBase<Derived>, double>(l, r);
            }

            inline friend BinaryOperator<Operator::MUL, RealBase<Derived>, double> operator-(const RealBase<Derived> &l, const double &r)
            {
                return BinaryOperator<Operator::SUB, RealBase<Derived>, double>(l, r);
            }

            inline friend BinaryOperator<Operator::MUL, double, RealBase<Derived>> operator-(const double &r, const RealBase<Derived> &l)
            {
                return BinaryOperator<Operator::SUB, double, RealBase<Derived>>(r, l);
            }

            inline friend BinaryOperator<Operator::MUL, RealBase<Derived>, double> operator*(const double &r, const RealBase<Derived> &l)
            {
                return BinaryOperator<Operator::MUL, RealBase<Derived>, double>(l, r);
            }

            inline friend BinaryOperator<Operator::MUL, RealBase<Derived>, double> operator*(const RealBase<Derived> &l, const double &r)
            {
                return BinaryOperator<Operator::MUL, RealBase<Derived>, double>(l, r);
            }

            inline friend BinaryOperator<Operator::DIV, double, RealBase<Derived>> operator/(const double &r, const RealBase<Derived> &l)
            {
                return BinaryOperator<Operator::DIV, double, RealBase<Derived>>(r, l);
            }

            inline friend BinaryOperator<Operator::DIV, RealBase<Derived>, double> operator/(const RealBase<Derived> &l, const double &r)
            {
                return BinaryOperator<Operator::DIV, RealBase<Derived>, double>(l, r);
            }
        };

        template <Operator opCode, typename L, typename R>
        struct BinaryOperator : public RealBase<BinaryOperator<opCode, L, R>>
        {
            const L &lhs;
            const R &rhs;
            BinaryOperator(const L &l, const R &r) : lhs(l), rhs(r) {}
            const Operator getOpCode() const { return opCode; }
        };

        template <Operator opCode, typename L, typename M, typename R>
        struct TernaryOperator : public RealBase<TernaryOperator<opCode, L, M, R>>
        {
            const L &lhs;
            const M &mhs;
            const R &rhs;
            TernaryOperator(const L &l, const M &m, const R &r) : lhs(l), mhs(m), rhs(r) {}
            const Operator getOpCode() const { return opCode; }
        };

        enum RealType
        {
            formal,
            actual
        };
        struct Real : public RealBase<Real>
        {

            real::sval_ptr shadow;
            RealType formal;
            Real() : formal(RealType::actual)
            {
                shadow = ShadowPool::INSTANCE.get();
            }
            ~Real()
            {
                if (!formal && shadow != nullptr)
                {
                    ShadowPool::INSTANCE.put(shadow);
                    shadow = nullptr;
                }
            }
            Real(double v)
            {
                shadow = ShadowPool::INSTANCE.get();
                mpfr_set_d(shadow->shadowValue, v, MPFR_RNDN);
                shadow->originalValue = v;
                shadow->avgRelativeError = 0;
            }
            Real(const Real &r)
            {
                shadow = ShadowPool::INSTANCE.get();
                mpfr_set(shadow->shadowValue, r.shadow->shadowValue, MPFR_RNDN);
                shadow->originalValue = r.shadow->originalValue;
                shadow->avgRelativeError = 0;
            }
            Real(Real &&r) noexcept
            {
                this->shadow = r.shadow;
                r.shadow = ShadowPool::INSTANCE.get();;
            }

            template <typename Derived>
            inline Real &operator=(const RealBase<Derived> &r)
            {
                ExpressionEvaluator<RealBase<Derived>>::eval(this->shadow, r);
                return *this;
            }
            
            template <typename Derived>
            inline Real &operator+=(const RealBase<Derived> &r)
            {
                *this = *this + r;
                return *this;
            }

            template <typename Derived>
            inline Real &operator-=(const RealBase<Derived> &r)
            {
                *this = *this - r;
                return *this;
            }

            template <typename Derived>
            inline Real &operator*=(const RealBase<Derived> &r)
            {
                *this = *this * r;
                return *this;
            }

            template <typename Derived>
            inline Real &operator/=(const RealBase<Derived> &r)
            {
                *this = *this / r;
                return *this;
            }

            inline Real &operator=(const double &r)
            {
                mpfr_set_d(this->shadow->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                shadow->originalValue = r;
#endif
                return *this;
            }

            inline Real &operator+=(const double &r)
            {
                mpfr_add_d(this->shadow->shadowValue, this->shadow->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                shadow->originalValue += r;
#endif
                return *this;
            }
            inline Real &operator-=(const double &r)
            {
                mpfr_sub_d(this->shadow->shadowValue, this->shadow->shadowValue,r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                shadow->originalValue -= r;
#endif
                return *this;
            }
            inline Real &operator*=(const double &r)
            {
                mpfr_mul_d(this->shadow->shadowValue, this->shadow->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                shadow->originalValue *= r;
#endif
                return *this;
            }
            inline Real &operator/=(const double &r)
            {
                mpfr_div_d(this->shadow->shadowValue, this->shadow->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                shadow->originalValue /= r;
#endif
                return *this;
            }

        private:
            void makeFormal()
            {
                if (formal != RealType::formal)
                {
                    formal = RealType::formal;
                    ShadowPool::INSTANCE.put(shadow);
                    shadow = nullptr;
                }
            }
            template <int, int>
            friend struct InitLoop;

            friend std::ostream &operator<<(std::ostream &os, const Real &c)
            {
                static char buf[512];
                mpfr_snprintf(buf, 512, "%.32Re (original = %.16e)", c.shadow->shadowValue, c.shadow->originalValue);
                os << buf;
                return os;
            }
        };

        template <>
        struct ExpressionEvaluator<RealBase<Real>>
        {
            inline static void eval(const real::sval_ptr acc, const RealBase<Real> &v)
            {
                mpfr_set(acc->shadowValue, v.derived().shadow->shadowValue, MPFR_RNDN);
            }

            inline static const real::sval_ptr eval(const RealBase<Real> &r)
            {
                return nullptr; // should not use
            }
        };

        template <Operator opCode, typename L, typename R>
        struct ExpressionEvaluator<RealBase<BinaryOperator<opCode, L, R>>>
        {
            inline static const real::sval_ptr eval(const RealBase<BinaryOperator<opCode, L, R>> &r)
            {
                sval_ptr tmp = ShadowPool::INSTANCE.get();
                ExpressionEvaluator<RealBase<BinaryOperator<opCode, L, R>>>::eval(tmp, r);
                return tmp;
            }

            /**
             * For addition, there are six cases 
             */
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::ADD && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const RealBase<Real> &r = exp.derived().rhs;
                const real::sval_ptr ll = l.derived().shadow;
                const real::sval_ptr rr = r.derived().shadow;
                mpfr_add(acc->shadowValue, ll->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = ll->originalValue + rr->originalValue;
#endif
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::ADD && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<R>::isRealBase == false, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const double &r = exp.derived().rhs;
                const real::sval_ptr ll = l.derived().shadow;
                mpfr_add_d(acc->shadowValue, ll->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = ll->originalValue + r;
#endif
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::ADD && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &r = exp.derived().rhs;
                const sval_ptr rr = r.derived().shadow;
                if (real_likely(rr != acc))
                {
                    ExpressionEvaluator<L>::eval(acc, exp.derived().lhs);
                    mpfr_add(acc->shadowValue, acc->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = acc->originalValue + rr->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                    mpfr_add(acc->shadowValue, tmp->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = tmp->originalValue + rr->originalValue;
#endif
                    ShadowPool::INSTANCE.put(tmp);
                }
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::ADD && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<R>::isRealBase == false, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                ExpressionEvaluator<L>::eval(acc, exp.derived().lhs);
                const double &r = exp.derived().rhs;
                mpfr_add_d(acc->shadowValue, acc->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = acc->originalValue + r;
#endif
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::ADD && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const sval_ptr ll = l.derived().shadow;
                if (real_likely(ll != acc))
                {
                    ExpressionEvaluator<R>::eval(acc, exp.derived().rhs);
                    mpfr_add(acc->shadowValue, ll->shadowValue, acc->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = ll->originalValue + acc->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                    mpfr_add(acc->shadowValue, ll->shadowValue, tmp->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = ll->originalValue + tmp->originalValue;
#endif
                    ShadowPool::INSTANCE.put(tmp);
                }
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::ADD && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                sval_ptr tmp1 = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                sval_ptr tmp2 = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                mpfr_add(acc->shadowValue, tmp1->shadowValue, tmp2->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = tmp1->originalValue + tmp2->originalValue;
#endif
                ShadowPool::INSTANCE.put(tmp2);
                ShadowPool::INSTANCE.put(tmp1);
            }

            /**
             * For multiplication, there are six cases 
             */
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::MUL && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const RealBase<Real> &r = exp.derived().rhs;
                const real::sval_ptr ll = l.derived().shadow;
                const real::sval_ptr rr = r.derived().shadow;
                mpfr_mul(acc->shadowValue, ll->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = ll->originalValue * rr->originalValue;
#endif
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::MUL && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<R>::isRealBase == false, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const double &r = exp.derived().rhs;
                const real::sval_ptr ll = l.derived().shadow;
                mpfr_mul_d(acc->shadowValue, ll->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = ll->originalValue * r;
#endif
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::MUL && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &r = exp.derived().rhs;
                const sval_ptr rr = r.derived().shadow;
                if (real_likely(rr != acc))
                {
                    ExpressionEvaluator<L>::eval(acc, exp.derived().lhs);
                    mpfr_mul(acc->shadowValue, acc->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = acc->originalValue * rr->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                    mpfr_mul(acc->shadowValue, tmp->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = tmp->originalValue * rr->originalValue;
#endif
                    ShadowPool::INSTANCE.put(tmp);
                }
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::MUL && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<R>::isRealBase == false, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                ExpressionEvaluator<L>::eval(acc, exp.derived().lhs);
                const double &r = exp.derived().rhs;
                mpfr_mul_d(acc->shadowValue, acc->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = acc->originalValue * r;
#endif
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::MUL && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const sval_ptr ll = l.derived().shadow;
                if (real_likely(ll != acc))
                {
                    ExpressionEvaluator<R>::eval(acc, exp.derived().rhs);
                    mpfr_mul(acc->shadowValue, ll->shadowValue, acc->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = ll->originalValue * acc->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                    mpfr_mul(acc->shadowValue, ll->shadowValue, tmp->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = ll->originalValue * tmp->originalValue;
#endif
                    ShadowPool::INSTANCE.put(tmp);
                }
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::MUL && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                sval_ptr tmp1 = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                sval_ptr tmp2 = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                mpfr_mul(acc->shadowValue, tmp1->shadowValue, tmp2->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = tmp1->originalValue * tmp2->originalValue;
#endif
                ShadowPool::INSTANCE.put(tmp2);
                ShadowPool::INSTANCE.put(tmp1);
            }

            /**
             * For substraction, there are eight cases 
             */
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::SUB && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const RealBase<Real> &r = exp.derived().rhs;
                const real::sval_ptr ll = l.derived().shadow;
                const real::sval_ptr rr = r.derived().shadow;
                mpfr_sub(acc->shadowValue, ll->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = ll->originalValue - rr->originalValue;
#endif
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::SUB && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<R>::isRealBase == false, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const double &r = exp.derived().rhs;
                const real::sval_ptr ll = l.derived().shadow;
                mpfr_sub_d(acc->shadowValue, ll->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = ll->originalValue - r;
#endif
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::SUB && trails<L>::isRealBase == false && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const double &l = exp.derived().lhs;
                const RealBase<Real> &r = exp.derived().rhs;
                const real::sval_ptr rr = r.derived().shadow;
                mpfr_d_sub(acc->shadowValue, l, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = l - rr->originalValue;
#endif
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::SUB && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &r = exp.derived().rhs;
                const sval_ptr rr = r.derived().shadow;
                if (real_likely(rr != acc))
                {
                    ExpressionEvaluator<L>::eval(acc, exp.derived().lhs);
                    mpfr_sub(acc->shadowValue, acc->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = acc->originalValue - rr->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                    mpfr_sub(acc->shadowValue, tmp->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = tmp->originalValue - rr->originalValue;
#endif
                    ShadowPool::INSTANCE.put(tmp);
                }
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::SUB && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<R>::isRealBase == false, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                ExpressionEvaluator<L>::eval(acc, exp.derived().lhs);
                const double &r = exp.derived().rhs;
                mpfr_sub_d(acc->shadowValue, acc->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = acc->originalValue - r;
#endif
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::SUB && trails<L>::isRealBase == false && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const double &l = exp.derived().lhs;
                ExpressionEvaluator<R>::eval(acc, exp.derived().rhs);
                mpfr_d_sub(acc->shadowValue, l, acc->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = l - acc->originalValue;
#endif
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::SUB && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const sval_ptr ll = l.derived().shadow;
                if (real_likely(ll != acc))
                {
                    ExpressionEvaluator<R>::eval(acc, exp.derived().rhs);
                    mpfr_sub(acc->shadowValue, ll->shadowValue, acc->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = ll->originalValue - acc->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                    mpfr_sub(acc->shadowValue, ll->shadowValue, tmp->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = ll->originalValue - tmp->originalValue;
#endif
                    ShadowPool::INSTANCE.put(tmp);
                }
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::SUB && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                sval_ptr tmp1 = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                sval_ptr tmp2 = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                mpfr_sub(acc->shadowValue, tmp1->shadowValue, tmp2->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = tmp1->originalValue - tmp2->originalValue;
#endif
                ShadowPool::INSTANCE.put(tmp2);
                ShadowPool::INSTANCE.put(tmp1);
            }

            /**
             * For division, there are eight cases 
             */
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::DIV && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const RealBase<Real> &r = exp.derived().rhs;
                const real::sval_ptr ll = l.derived().shadow;
                const real::sval_ptr rr = r.derived().shadow;
                mpfr_div(acc->shadowValue, ll->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = ll->originalValue / rr->originalValue;
#endif
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::DIV && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<R>::isRealBase == false, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const double &r = exp.derived().rhs;
                const real::sval_ptr ll = l.derived().shadow;
                mpfr_div_d(acc->shadowValue, ll->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = ll->originalValue / r;
#endif
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::DIV && trails<L>::isRealBase == false && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const double &l = exp.derived().lhs;
                const RealBase<Real> &r = exp.derived().rhs;
                const real::sval_ptr rr = r.derived().shadow;
                mpfr_d_div(acc->shadowValue, l, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = l / rr->originalValue;
#endif
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::DIV && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &r = exp.derived().rhs;
                const sval_ptr rr = r.derived().shadow;
                if (real_likely(rr != acc))
                {
                    ExpressionEvaluator<L>::eval(acc, exp.derived().lhs);
                    mpfr_div(acc->shadowValue, acc->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = acc->originalValue / rr->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                    mpfr_div(acc->shadowValue, tmp->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = tmp->originalValue / rr->originalValue;
#endif
                    ShadowPool::INSTANCE.put(tmp);
                }
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::DIV && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<R>::isRealBase == false, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                ExpressionEvaluator<L>::eval(acc, exp.derived().lhs);
                const double &r = exp.derived().rhs;
                mpfr_div_d(acc->shadowValue, acc->shadowValue, r, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = acc->originalValue / r;
#endif
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::DIV && trails<L>::isRealBase == false && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const double &l = exp.derived().lhs;
                ExpressionEvaluator<R>::eval(acc, exp.derived().rhs);
                mpfr_d_div(acc->shadowValue, l, acc->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = l / acc->originalValue;
#endif
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::DIV && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const sval_ptr ll = l.derived().shadow;
                if (ll != acc)
                {
                    ExpressionEvaluator<R>::eval(acc, exp.derived().rhs);
                    mpfr_div(acc->shadowValue, ll->shadowValue, acc->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = ll->originalValue / acc->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                    mpfr_div(acc->shadowValue, ll->shadowValue, tmp->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                    acc->originalValue = ll->originalValue / tmp->originalValue;
#endif
                    ShadowPool::INSTANCE.put(tmp);
                }
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<BinaryOperator<opCode, L, R>>>::value && opCode == Operator::DIV && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP, void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                sval_ptr tmp1 = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                sval_ptr tmp2 = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                mpfr_div(acc->shadowValue, tmp1->shadowValue, tmp2->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = tmp1->originalValue / tmp2->originalValue;
#endif
                ShadowPool::INSTANCE.put(tmp2);
                ShadowPool::INSTANCE.put(tmp1);
            }
        };

        template <Operator opCode, typename L, typename M, typename R>
        struct ExpressionEvaluator<RealBase<TernaryOperator<opCode, L, M, R>>>
        {
            inline static const real::sval_ptr eval(const RealBase<TernaryOperator<opCode, L, M, R>> &r)
            {
                sval_ptr tmp = ShadowPool::INSTANCE.get();
                ExpressionEvaluator<RealBase<TernaryOperator<opCode, L, M, R>>>::eval(tmp, r);
                return tmp;
            }

            /**
             * For FMA, there are eight cases 
             */
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMA && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<M>::isRealBase && trails<M>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const RealBase<Real> &m = exp.derived().mhs;
                const RealBase<Real> &r = exp.derived().rhs;
                const real::sval_ptr ll = l.derived().shadow;
                const real::sval_ptr mm = m.derived().shadow;
                const real::sval_ptr rr = r.derived().shadow;
                mpfr_fma(acc->shadowValue, ll->shadowValue, mm->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(ll->originalValue, mm->originalValue, rr->originalValue);
#endif
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMA && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<M>::isRealBase && trails<M>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const sval_ptr tmpL = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                const RealBase<Real> &m = exp.derived().mhs;
                const RealBase<Real> &r = exp.derived().rhs;
                const real::sval_ptr mm = m.derived().shadow;
                const real::sval_ptr rr = r.derived().shadow;
                mpfr_fma(acc->shadowValue, tmpL->shadowValue, mm->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(tmpL->originalValue, mm->originalValue, rr->originalValue);
#endif
                ShadowPool::INSTANCE.put(tmpL);
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMA && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<M>::isRealBase && trails<M>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const sval_ptr tmpM = ExpressionEvaluator<M>::eval(exp.derived().mhs);
                const RealBase<Real> &r = exp.derived().rhs;
                const real::sval_ptr ll = l.derived().shadow;
                const real::sval_ptr rr = r.derived().shadow;

                mpfr_fma(acc->shadowValue, ll->shadowValue, tmpM->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(ll->originalValue, tmpM->originalValue, rr->originalValue);
#endif
                ShadowPool::INSTANCE.put(tmpM);
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMA && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<M>::isRealBase && trails<M>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const RealBase<Real> &m = exp.derived().mhs;
                const sval_ptr tmpR = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                const real::sval_ptr ll = l.derived().shadow;
                const real::sval_ptr mm = m.derived().shadow;
                mpfr_fma(acc->shadowValue, ll->shadowValue, mm->shadowValue, tmpR->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(ll->originalValue, mm->originalValue, tmpR->originalValue);
#endif
                ShadowPool::INSTANCE.put(tmpR);
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMA && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<M>::isRealBase && trails<M>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const sval_ptr tmpL = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                const sval_ptr tmpM = ExpressionEvaluator<M>::eval(exp.derived().mhs);
                const RealBase<Real> &r = exp.derived().rhs;
                const real::sval_ptr rr = r.derived().shadow;
                mpfr_fma(acc->shadowValue, tmpL->shadowValue, tmpM->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(tmpL->originalValue, tmpM->originalValue, rr->originalValue);
#endif
                ShadowPool::INSTANCE.put(tmpM);
                ShadowPool::INSTANCE.put(tmpL);
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMA && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<M>::isRealBase && trails<M>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const sval_ptr tmpL = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                const RealBase<Real> &m = exp.derived().mhs;
                const sval_ptr tmpR = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                const real::sval_ptr mm = m.derived().shadow;
                mpfr_fma(acc->shadowValue, tmpL->shadowValue, mm->shadowValue, tmpR->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(tmpL->originalValue, mm->originalValue, tmpR->originalValue);
#endif
                ShadowPool::INSTANCE.put(tmpR);
                ShadowPool::INSTANCE.put(tmpL);
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMA && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<M>::isRealBase && trails<M>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const sval_ptr tmpM = ExpressionEvaluator<M>::eval(exp.derived().mhs);
                const sval_ptr tmpR = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                const real::sval_ptr ll = l.derived().shadow;
                mpfr_fma(acc->shadowValue, ll->shadowValue, tmpM->shadowValue, tmpR->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(ll->originalValue, tmpM->originalValue, tmpR->originalValue);
#endif
                ShadowPool::INSTANCE.put(tmpR);
                ShadowPool::INSTANCE.put(tmpM);
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMA && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<M>::isRealBase && trails<M>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const sval_ptr tmpL = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                const sval_ptr tmpM = ExpressionEvaluator<M>::eval(exp.derived().mhs);
                const sval_ptr tmpR = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                mpfr_fma(acc->shadowValue, tmpL->shadowValue, tmpM->shadowValue, tmpR->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(tmpL->originalValue, tmpM->originalValue, tmpR->originalValue);
#endif
                ShadowPool::INSTANCE.put(tmpR);
                ShadowPool::INSTANCE.put(tmpM);
                ShadowPool::INSTANCE.put(tmpL);
            }

            /**
             * For FMS, there are eight cases 
             */
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMS && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<M>::isRealBase && trails<M>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const RealBase<Real> &m = exp.derived().mhs;
                const RealBase<Real> &r = exp.derived().rhs;
                const real::sval_ptr ll = l.derived().shadow;
                const real::sval_ptr mm = m.derived().shadow;
                const real::sval_ptr rr = r.derived().shadow;
                mpfr_fms(acc->shadowValue, ll->shadowValue, mm->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(ll->originalValue, mm->originalValue, -rr->originalValue);
#endif
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMS && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<M>::isRealBase && trails<M>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const sval_ptr tmpL = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                const RealBase<Real> &m = exp.derived().mhs;
                const RealBase<Real> &r = exp.derived().rhs;
                const real::sval_ptr mm = m.derived().shadow;
                const real::sval_ptr rr = r.derived().shadow;
                mpfr_fms(acc->shadowValue, tmpL->shadowValue, mm->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(tmpL->originalValue, mm->originalValue, -rr->originalValue);
#endif

                ShadowPool::INSTANCE.put(tmpL);
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMS && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<M>::isRealBase && trails<M>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const sval_ptr tmpM = ExpressionEvaluator<M>::eval(exp.derived().mhs);
                const RealBase<Real> &r = exp.derived().rhs;
                const real::sval_ptr ll = l.derived().shadow;
                const real::sval_ptr rr = r.derived().shadow;
                mpfr_fms(acc->shadowValue, ll->shadowValue, tmpM->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(ll->originalValue, tmpM->originalValue, -rr->originalValue);
#endif

                ShadowPool::INSTANCE.put(tmpM);
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMS && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<M>::isRealBase && trails<M>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const RealBase<Real> &m = exp.derived().mhs;
                const real::sval_ptr ll = l.derived().shadow;
                const real::sval_ptr mm = m.derived().shadow;
                const sval_ptr tmpR = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                mpfr_fms(acc->shadowValue, ll->shadowValue, mm->shadowValue, tmpR->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(ll->originalValue, mm->originalValue, -tmpR->originalValue);
#endif

                ShadowPool::INSTANCE.put(tmpR);
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMS && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<M>::isRealBase && trails<M>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode == Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const sval_ptr tmpL = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                const sval_ptr tmpM = ExpressionEvaluator<M>::eval(exp.derived().mhs);
                const RealBase<Real> &r = exp.derived().rhs;
                const real::sval_ptr rr = r.derived().shadow;
                mpfr_fms(acc->shadowValue, tmpL->shadowValue, tmpM->shadowValue, rr->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(tmpL->originalValue, tmpM->originalValue, -rr->originalValue);
#endif
                ShadowPool::INSTANCE.put(tmpM);
                ShadowPool::INSTANCE.put(tmpL);
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMS && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<M>::isRealBase && trails<M>::opCode == Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const sval_ptr tmpL = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                const RealBase<Real> &m = exp.derived().mhs;
                const sval_ptr tmpR = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                const real::sval_ptr mm = m.derived().shadow;
                mpfr_fms(acc->shadowValue, tmpL->shadowValue, mm->shadowValue, tmpR->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(tmpL->originalValue, mm->originalValue, -tmpR->originalValue);
#endif
                ShadowPool::INSTANCE.put(tmpR);
                ShadowPool::INSTANCE.put(tmpL);
            }
            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMS && trails<L>::isRealBase && trails<L>::opCode == Operator::NOP && trails<M>::isRealBase && trails<M>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const RealBase<Real> &l = exp.derived().lhs;
                const sval_ptr tmpM = ExpressionEvaluator<M>::eval(exp.derived().mhs);
                const sval_ptr tmpR = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                const real::sval_ptr ll = l.derived().shadow;
                mpfr_fms(acc->shadowValue, ll->shadowValue, tmpM->shadowValue, tmpR->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(ll->originalValue, tmpM->originalValue, -tmpR->originalValue);
#endif
                ShadowPool::INSTANCE.put(tmpR);
                ShadowPool::INSTANCE.put(tmpM);
            }

            template <typename EXP>
            inline static std::enable_if_t<
                std::is_same<EXP, RealBase<TernaryOperator<opCode, L, M, R>>>::value && opCode == Operator::FMS && trails<L>::isRealBase && trails<L>::opCode != Operator::NOP && trails<M>::isRealBase && trails<M>::opCode != Operator::NOP && trails<R>::isRealBase && trails<R>::opCode != Operator::NOP,
                void>
            eval(const real::sval_ptr acc, const EXP &exp)
            {
                const sval_ptr tmpL = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                const sval_ptr tmpM = ExpressionEvaluator<M>::eval(exp.derived().mhs);
                const sval_ptr tmpR = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                mpfr_fms(acc->shadowValue, tmpL->shadowValue, tmpM->shadowValue, tmpR->shadowValue, MPFR_RNDN);
#ifdef KEEP_ORIGINAL
                acc->originalValue = fma(tmpL->originalValue, tmpM->originalValue, -tmpR->originalValue);
#endif
                ShadowPool::INSTANCE.put(tmpR);
                ShadowPool::INSTANCE.put(tmpM);
                ShadowPool::INSTANCE.put(tmpL);
            }
        };

        template <typename T>
        struct builder_trails
        {
            using resultType = T;
            static const int argSize = -1;
        };

        template <typename BuilderType, typename R, int size>
        struct builder_trails<R (BuilderType::*)(const Real (&)[size]) const>
        {
            using resultType = R;
            static const int argSize = size;
        };

        template <int count, int size = count>
        struct InitLoop
        {
            inline static void run(Real (&arguments)[size])
            {
                arguments[count - 1].makeFormal();
                InitLoop<count - 1, size>::run(arguments);
            }
        };
        template <int size>
        struct InitLoop<0, size>
        {
            inline static void run(Real (&arguments)[size]) {}
        };

        template <typename BuilderType,
                  typename _Fp = decltype(&BuilderType::operator()),
                  int argSize = builder_trails<_Fp>::argSize,
                  typename T = typename builder_trails<_Fp>::resultType>
        struct ExpressionSlot
        {
            Real arguments[argSize]; // place-holders
            const T expression;

            ExpressionSlot(const BuilderType &builder) : arguments(), expression(builder.operator()(arguments))
            {
                InitLoop<argSize>::run(arguments);
            }

            template <int start = 0, typename... Args>
            inline const T &operator()(const Real &first, const Args &... rest)
            {
                static_assert(start < argSize);
                substitute(arguments[start], first);
                return operator()<start + 1>(rest...);
            }

            template <int start = 0>
            inline const T &operator()()
            {
                static_assert(start == argSize);
                return expression;
            }

        private:
            inline void substitute(Real &formal, const Real &actual)
            {
                formal.shadow = actual.shadow;
            }
        };
    }; // namespace lazy

}; // namespace real

#endif
