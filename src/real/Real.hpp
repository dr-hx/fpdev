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

#include "RealConfigure.h"
#include "ShadowValue.hpp"

namespace real
{
    using namespace real::util;

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
            ASSIGN_D(shadow->shadowValue, v);
#if KEEP_ORIGINAL
            shadow->originalValue = v;
#endif
            shadow->avgRelativeError = 0;
        }

        Real(Real &&r) noexcept
        {
            this->shadow = r.shadow;
            r.shadow = ShadowPool::INSTANCE.get();
            RealPool<Real>::INSTANCE.put(&r);
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

        ~Real()
        {
            if (shadow != nullptr)
            {
                ShadowPool::INSTANCE.put(shadow);
                shadow = nullptr;
            }
        }

        INLINE_FLAGS Real &&operator-()
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            SUB_DR(res.shadow->shadowValue, 0, this->shadow->shadowValue);
#if KEEP_ORIGINAL
            res.shadow->originalValue = - shadow->originalValue;
#endif
            return std::move(res);
        }

        INLINE_FLAGS friend Real &&operator+(const Real &l, const Real &r)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            ADD_RR(res.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue + r.shadow->originalValue;
#endif
            return std::move(res);
        }

        INLINE_FLAGS friend Real &&operator+(Real &&l, const Real &r)
        {
            ADD_RR(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            l.shadow->originalValue += r.shadow->originalValue;
#endif
            return std::move(l);
        }

        INLINE_FLAGS friend Real &&operator+(const Real &l, Real &&r)
        {
            ADD_RR(r.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            r.shadow->originalValue += l.shadow->originalValue;
#endif
            return std::move(r);
        }

        INLINE_FLAGS friend Real &&operator+(Real &&l, Real &&r)
        {
            ADD_RR(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            l.shadow->originalValue += r.shadow->originalValue;
#endif
            RealPool<Real>::INSTANCE.put(&r); // r should be an useless temp
            return std::move(l);
        }

        INLINE_FLAGS friend Real &&operator-(const Real &l, const Real &r)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            SUB_RR(res.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue - r.shadow->originalValue;
#endif
            return std::move(res);
        }

        INLINE_FLAGS friend Real &&operator-(Real &&l, const Real &r)
        {
            SUB_RR(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            l.shadow->originalValue -= r.shadow->originalValue;
#endif
            return std::move(l);
        }
        INLINE_FLAGS friend Real &&operator-(Real &&l, Real &&r)
        {
            SUB_RR(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            l.shadow->originalValue -= r.shadow->originalValue;
#endif
            RealPool<Real>::INSTANCE.put(&r);
            return std::move(l);
        }
        INLINE_FLAGS friend Real &&operator-(const Real &l, Real &&r)
        {
            SUB_RR(r.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            r.shadow->originalValue = l.shadow->originalValue - r.shadow->originalValue;
#endif
            return std::move(r);
        }
        INLINE_FLAGS friend Real &&operator*(const Real &l, const Real &r)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            MUL_RR(res.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue * r.shadow->originalValue;
#endif
            return std::move(res);
        }

        INLINE_FLAGS friend Real &&operator*(Real &&l, const Real &r)
        {
            MUL_RR(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            l.shadow->originalValue *= r.shadow->originalValue;
#endif
            return std::move(l);
        }

        INLINE_FLAGS friend Real &&operator*(const Real &l, Real &&r)
        {
            MUL_RR(r.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            r.shadow->originalValue *= l.shadow->originalValue;
#endif
            return std::move(r);
        }

        INLINE_FLAGS friend Real &&operator*(Real &&l, Real &&r)
        {
            MUL_RR(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            l.shadow->originalValue *= r.shadow->originalValue;
#endif
            RealPool<Real>::INSTANCE.put(&r);
            return std::move(l);
        }

        INLINE_FLAGS friend Real &&operator/(const Real &l, const Real &r)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            DIV_RR(res.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue / r.shadow->originalValue;
#endif
            return std::move(res);
        }

        INLINE_FLAGS friend Real &&operator/(Real &&l, const Real &r)
        {
            DIV_RR(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            l.shadow->originalValue /= r.shadow->originalValue;
#endif
            return std::move(l);
        }
        INLINE_FLAGS friend Real &&operator/(Real &&l, Real &&r)
        {
            DIV_RR(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            l.shadow->originalValue /= r.shadow->originalValue;
#endif
            RealPool<Real>::INSTANCE.put(&r);
            return std::move(l);
        }
        INLINE_FLAGS friend Real &&operator/(const Real &l, Real &&r)
        {
            DIV_RR(r.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            r.shadow->originalValue = l.shadow->originalValue / r.shadow->originalValue;
#endif
            return std::move(r);
        }

        INLINE_FLAGS friend Real &&operator+(Real &&l, const double i)
        {
            ADD_RD(l.shadow->shadowValue, l.shadow->shadowValue, i);
#if KEEP_ORIGINAL
            l.shadow->originalValue += i;
#endif
            return std::move(l);
        }
        INLINE_FLAGS friend Real &&operator+(const Real &l, const double i)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            ADD_RD(res.shadow->shadowValue, l.shadow->shadowValue, i);
#if KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue + i;
#endif
            return std::move(res);
        }
        INLINE_FLAGS friend Real &&operator+(const double i, Real &&l)
        {
            ADD_RD(l.shadow->shadowValue, l.shadow->shadowValue, i);
#if KEEP_ORIGINAL
            l.shadow->originalValue = i + l.shadow->originalValue;
#endif
            return std::move(l);
        }
        INLINE_FLAGS friend Real &&operator+(const double i, const Real &l)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            ADD_RD(res.shadow->shadowValue, l.shadow->shadowValue, i);
#if KEEP_ORIGINAL
            res.shadow->originalValue = i + l.shadow->originalValue;
#endif
            return std::move(res);
        }

        INLINE_FLAGS friend Real &&operator-(Real &&l, const double i)
        {
            SUB_RD(l.shadow->shadowValue, l.shadow->shadowValue, i);
#if KEEP_ORIGINAL
            l.shadow->originalValue -= i;
#endif
            return std::move(l);
        }
        INLINE_FLAGS friend Real &&operator-(const Real &l, const double i)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            SUB_RD(res.shadow->shadowValue, l.shadow->shadowValue, i);
#if KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue - i;
#endif
            return std::move(res);
        }
        INLINE_FLAGS friend Real &&operator-(const double i, Real &&l)
        {
            SUB_DR(l.shadow->shadowValue, i, l.shadow->shadowValue);
#if KEEP_ORIGINAL
            l.shadow->originalValue = i - l.shadow->originalValue;
#endif
            return std::move(l);
        }
        INLINE_FLAGS friend Real &&operator-(const double i, const Real &l)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            SUB_DR(res.shadow->shadowValue, i, l.shadow->shadowValue);
#if KEEP_ORIGINAL
            res.shadow->originalValue = i - l.shadow->originalValue;
#endif
            return std::move(res);
        }

        INLINE_FLAGS friend Real &&operator*(Real &&l, const double i)
        {
            MUL_RD(l.shadow->shadowValue, l.shadow->shadowValue, i);
#if KEEP_ORIGINAL
            l.shadow->originalValue *= i;
#endif
            return std::move(l);
        }
        INLINE_FLAGS friend Real &&operator*(const Real &l, const double i)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            MUL_RD(res.shadow->shadowValue, l.shadow->shadowValue, i);
#if KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue * i;
#endif
            return std::move(res);
        }
        INLINE_FLAGS friend Real &&operator*(const double i, Real &&l)
        {
            MUL_RD(l.shadow->shadowValue, l.shadow->shadowValue, i);
#if KEEP_ORIGINAL
            l.shadow->originalValue = i * l.shadow->originalValue;
#endif
            return std::move(l);
        }
        INLINE_FLAGS friend Real &&operator*(const double i, const Real &l)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            MUL_RD(res.shadow->shadowValue, l.shadow->shadowValue, i);
#if KEEP_ORIGINAL
            res.shadow->originalValue = i * l.shadow->originalValue;
#endif
            return std::move(res);
        }

        INLINE_FLAGS friend Real &&operator/(Real &&l, const double i)
        {
            DIV_RD(l.shadow->shadowValue, l.shadow->shadowValue, i);
#if KEEP_ORIGINAL
            l.shadow->originalValue /= i;
#endif
            return std::move(l);
        }
        INLINE_FLAGS friend Real &&operator/(const Real &l, const double i)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            DIV_RD(res.shadow->shadowValue, l.shadow->shadowValue, i);
#if KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue / i;
#endif
            return std::move(res);
        }
        INLINE_FLAGS friend Real &&operator/(const double i, Real &&l)
        {
            DIV_DR(l.shadow->shadowValue, i, l.shadow->shadowValue);
#if KEEP_ORIGINAL
            l.shadow->originalValue = i / l.shadow->originalValue;
#endif
            return std::move(l);
        }
        INLINE_FLAGS friend Real &&operator/(const double i, const Real &l)
        {
            Real &res = *RealPool<Real>::INSTANCE.get();
            DIV_DR(res.shadow->shadowValue, i, l.shadow->shadowValue);
#if KEEP_ORIGINAL
            res.shadow->originalValue = i / l.shadow->originalValue;
#endif
            return std::move(res);
        }

        INLINE_FLAGS Real &operator=(const Real &r)
        {
            ASSIGN(shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue = r.shadow->originalValue;
#endif
            return *this;
        }
        INLINE_FLAGS Real &operator=(Real &&r)
        {
            SWAP(shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue = r.shadow->originalValue;
#endif
            RealPool<Real>::INSTANCE.put(&r);
            return *this;
        }
        INLINE_FLAGS Real &operator=(const double r)
        {
            ASSIGN_D(shadow->shadowValue, r);
#if KEEP_ORIGINAL
            shadow->originalValue = r;
#endif
            return *this;
        }

        INLINE_FLAGS Real &operator+=(const Real &r)
        {
            ADD_RR(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue += r.shadow->originalValue;
#endif
            return *this;
        }

        INLINE_FLAGS Real &operator-=(const Real &r)
        {
            SUB_RR(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue -= r.shadow->originalValue;
#endif
            return *this;
        }

        INLINE_FLAGS Real &operator*=(const Real &r)
        {
            MUL_RR(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue *= r.shadow->originalValue;
#endif
            return *this;
        }

        INLINE_FLAGS Real &operator/=(const Real &r)
        {
            DIV_RR(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue /= r.shadow->originalValue;
#endif
            return *this;
        }

        INLINE_FLAGS Real &operator+=(const double &r)
        {
            ADD_RD(this->shadow->shadowValue, this->shadow->shadowValue, r);
#if KEEP_ORIGINAL
            shadow->originalValue += r;
#endif
            return *this;
        }

        INLINE_FLAGS Real &operator-=(const double &r)
        {
            SUB_RD(this->shadow->shadowValue, this->shadow->shadowValue, r);
#if KEEP_ORIGINAL
            shadow->originalValue -= r;
#endif
            return *this;
        }

        INLINE_FLAGS Real &operator*=(const double &r)
        {
            MUL_RD(this->shadow->shadowValue, this->shadow->shadowValue, r);
#if KEEP_ORIGINAL
            shadow->originalValue *= r;
#endif
            return *this;
        }

        INLINE_FLAGS Real &operator/=(const double &r)
        {
            DIV_RD(this->shadow->shadowValue, this->shadow->shadowValue, r);
#if KEEP_ORIGINAL
            shadow->originalValue /= r;
#endif
            return *this;
        }

        friend std::ostream &operator<<(std::ostream &os, const Real &c)
        {
            STREAM_OUT(os, c);
            return os;
        }

        INLINE_FLAGS friend bool operator<(const Real &l, const Real &r)
        {
            return LESS_RR(l.shadow->shadowValue, r.shadow->shadowValue);
        }
        INLINE_FLAGS friend bool operator<=(const Real &l, const Real &r)
        {
            return LESSEQ_RR(l.shadow->shadowValue, r.shadow->shadowValue);
        }
        INLINE_FLAGS friend bool operator==(const Real &l, const Real &r)
        {
            return EQUAL_RR(l.shadow->shadowValue, r.shadow->shadowValue);
        }
        INLINE_FLAGS friend bool operator>(const Real &l, const Real &r)
        {
            return GREATER_RR(l.shadow->shadowValue, r.shadow->shadowValue);
        }
        INLINE_FLAGS friend bool operator<=(const Real &l, const Real &r)
        {
            return GREATEREQ_RR(l.shadow->shadowValue, r.shadow->shadowValue);
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

    real::Real &&RealExp(const real::Real &r)
    {
        real::Real &res = *real::RealPool<real::Real>::INSTANCE.get();
        EXP_R(res.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
        res.shadow->originalValue = exp(r.shadow->originalValue);
#endif
        return std::move(res);
    }

    real::Real &&RealPow(const real::Real &a, const real::Real &b)
    {
        real::Real &res = *real::RealPool<real::Real>::INSTANCE.get();
        POW_RR(res.shadow->shadowValue, a.shadow->shadowValue, b.shadow->shadowValue);
#if KEEP_ORIGINAL
        res.shadow->originalValue = pow(a.shadow->originalValue, b.shadow->originalValue);
#endif
        return std::move(res);
    }
}; // namespace real

#endif
