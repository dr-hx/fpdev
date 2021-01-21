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
#include <assert.h>



// #define KEEP_ORIGINAL true
// #define DELEGATE_TO_POOL true

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
            mpfr_set_d(shadow->shadowValue, v, MPFR_RNDN);
            shadow->originalValue = v;
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
}; // namespace real

#endif
