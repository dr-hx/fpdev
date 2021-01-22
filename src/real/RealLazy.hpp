#ifndef REAL_LAZY_HPP
#define REAL_LAYZ_HPP
#include "RealConfigure.h"
#include "ShadowValue.hpp"

namespace real
{
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
                ASSIGN_D(shadow->shadowValue, v);
#if KEEP_ORIGINAL
                shadow->originalValue = v;
#endif
                shadow->avgRelativeError = 0;
            }
            Real(const Real &r)
            {
                shadow = ShadowPool::INSTANCE.get();
                ASSIGN(shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
                shadow->originalValue = r.shadow->originalValue;
#endif
                shadow->avgRelativeError = 0;
            }
            Real(Real &&r) noexcept
            {
                this->shadow = r.shadow;
                r.shadow = ShadowPool::INSTANCE.get();
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
                ASSIGN_D(this->shadow->shadowValue, r);
#if KEEP_ORIGINAL
                shadow->originalValue = r;
#endif
                return *this;
            }

            inline Real &operator+=(const double &r)
            {
                ADD_RD(this->shadow->shadowValue, this->shadow->shadowValue, r);
#if KEEP_ORIGINAL
                shadow->originalValue += r;
#endif
                return *this;
            }
            inline Real &operator-=(const double &r)
            {
                SUB_RD(this->shadow->shadowValue, this->shadow->shadowValue, r);
#if KEEP_ORIGINAL
                shadow->originalValue -= r;
#endif
                return *this;
            }
            inline Real &operator*=(const double &r)
            {
                MUL_RD(this->shadow->shadowValue, this->shadow->shadowValue, r);
#if KEEP_ORIGINAL
                shadow->originalValue *= r;
#endif
                return *this;
            }
            inline Real &operator/=(const double &r)
            {
                DIV_RD(this->shadow->shadowValue, this->shadow->shadowValue, r);
#if KEEP_ORIGINAL
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
                STREAM_OUT(os, c);
                return os;
            }
        };

        template <>
        struct ExpressionEvaluator<RealBase<Real>>
        {
            inline static void eval(const real::sval_ptr acc, const RealBase<Real> &v)
            {
                ASSIGN(acc->shadowValue, v.derived().shadow->shadowValue);
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
                ADD_RR(acc->shadowValue, ll->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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
                ADD_RD(acc->shadowValue, ll->shadowValue, r);
#if KEEP_ORIGINAL
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
                    ADD_RR(acc->shadowValue, acc->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
                    acc->originalValue = acc->originalValue + rr->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                    ADD_RR(acc->shadowValue, tmp->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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
                ADD_RD(acc->shadowValue, acc->shadowValue, r);
#if KEEP_ORIGINAL
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
                    ADD_RR(acc->shadowValue, ll->shadowValue, acc->shadowValue);
#if KEEP_ORIGINAL
                    acc->originalValue = ll->originalValue + acc->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                    ADD_RR(acc->shadowValue, ll->shadowValue, tmp->shadowValue);
#if KEEP_ORIGINAL
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
                ADD_RR(acc->shadowValue, tmp1->shadowValue, tmp2->shadowValue);
#if KEEP_ORIGINAL
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
                MUL_RR(acc->shadowValue, ll->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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
                MUL_RD(acc->shadowValue, ll->shadowValue, r);
#if KEEP_ORIGINAL
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
                    MUL_RR(acc->shadowValue, acc->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
                    acc->originalValue = acc->originalValue * rr->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                    MUL_RR(acc->shadowValue, tmp->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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
                MUL_RD(acc->shadowValue, acc->shadowValue, r);
#if KEEP_ORIGINAL
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
                    MUL_RR(acc->shadowValue, ll->shadowValue, acc->shadowValue);
#if KEEP_ORIGINAL
                    acc->originalValue = ll->originalValue * acc->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                    MUL_RR(acc->shadowValue, ll->shadowValue, tmp->shadowValue);
#if KEEP_ORIGINAL
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
                MUL_RR(acc->shadowValue, tmp1->shadowValue, tmp2->shadowValue);
#if KEEP_ORIGINAL
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
                SUB_RR(acc->shadowValue, ll->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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
                SUB_RD(acc->shadowValue, ll->shadowValue, r);
#if KEEP_ORIGINAL
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
                SUB_DR(acc->shadowValue, l, rr->shadowValue);
#if KEEP_ORIGINAL
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
                    SUB_RR(acc->shadowValue, acc->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
                    acc->originalValue = acc->originalValue - rr->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                    SUB_RR(acc->shadowValue, tmp->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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
                SUB_RD(acc->shadowValue, acc->shadowValue, r);
#if KEEP_ORIGINAL
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
                SUB_DR(acc->shadowValue, l, acc->shadowValue);
#if KEEP_ORIGINAL
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
                    SUB_RR(acc->shadowValue, ll->shadowValue, acc->shadowValue);
#if KEEP_ORIGINAL
                    acc->originalValue = ll->originalValue - acc->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                    SUB_RR(acc->shadowValue, ll->shadowValue, tmp->shadowValue);
#if KEEP_ORIGINAL
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
                SUB_RR(acc->shadowValue, tmp1->shadowValue, tmp2->shadowValue);
#if KEEP_ORIGINAL
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
                DIV_RR(acc->shadowValue, ll->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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
                DIV_RD(acc->shadowValue, ll->shadowValue, r);
#if KEEP_ORIGINAL
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
                DIV_DR(acc->shadowValue, l, rr->shadowValue);
#if KEEP_ORIGINAL
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
                    DIV_RR(acc->shadowValue, acc->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
                    acc->originalValue = acc->originalValue / rr->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<L>::eval(exp.derived().lhs);
                    DIV_RR(acc->shadowValue, tmp->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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
                DIV_RD(acc->shadowValue, acc->shadowValue, r);
#if KEEP_ORIGINAL
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
                DIV_DR(acc->shadowValue, l, acc->shadowValue);
#if KEEP_ORIGINAL
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
                    DIV_RR(acc->shadowValue, ll->shadowValue, acc->shadowValue);
#if KEEP_ORIGINAL
                    acc->originalValue = ll->originalValue / acc->originalValue;
#endif
                }
                else
                {
                    sval_ptr tmp = ExpressionEvaluator<R>::eval(exp.derived().rhs);
                    DIV_RR(acc->shadowValue, ll->shadowValue, tmp->shadowValue);
#if KEEP_ORIGINAL
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
                DIV_RR(acc->shadowValue, tmp1->shadowValue, tmp2->shadowValue);
#if KEEP_ORIGINAL
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
                FMA(acc->shadowValue, ll->shadowValue, mm->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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
                FMA(acc->shadowValue, tmpL->shadowValue, mm->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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

                FMA(acc->shadowValue, ll->shadowValue, tmpM->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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
                FMA(acc->shadowValue, ll->shadowValue, mm->shadowValue, tmpR->shadowValue);
#if KEEP_ORIGINAL
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
                FMA(acc->shadowValue, tmpL->shadowValue, tmpM->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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
                FMA(acc->shadowValue, tmpL->shadowValue, mm->shadowValue, tmpR->shadowValue);
#if KEEP_ORIGINAL
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
                FMA(acc->shadowValue, ll->shadowValue, tmpM->shadowValue, tmpR->shadowValue);
#if KEEP_ORIGINAL
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
                FMA(acc->shadowValue, tmpL->shadowValue, tmpM->shadowValue, tmpR->shadowValue);
#if KEEP_ORIGINAL
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
                FMS(acc->shadowValue, ll->shadowValue, mm->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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
                FMS(acc->shadowValue, tmpL->shadowValue, mm->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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
                FMS(acc->shadowValue, ll->shadowValue, tmpM->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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
                FMS(acc->shadowValue, ll->shadowValue, mm->shadowValue, tmpR->shadowValue);
#if KEEP_ORIGINAL
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
                FMS(acc->shadowValue, tmpL->shadowValue, tmpM->shadowValue, rr->shadowValue);
#if KEEP_ORIGINAL
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
                FMS(acc->shadowValue, tmpL->shadowValue, mm->shadowValue, tmpR->shadowValue);
#if KEEP_ORIGINAL
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
                FMS(acc->shadowValue, ll->shadowValue, tmpM->shadowValue, tmpR->shadowValue);
#if KEEP_ORIGINAL
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
                FMS(acc->shadowValue, tmpL->shadowValue, tmpM->shadowValue, tmpR->shadowValue);
#if KEEP_ORIGINAL
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
};     // namespace real

#endif
