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

    static inline double CalcError(const Real &svar, double ovar)
    {
        double dsv = TO_DOUBLE(svar.shadow->shadowValue);
        if (dsv == 0) {
            if(ovar==0) return 0;
            dsv = 1.1E-16;
        }
        double re = (dsv - ovar) / dsv;
        long *pre = (long *)&re;
        *pre &= 0x7FFFFFFFFFFFFFFF;
        return re;
    }
    #if TRACK_ERROR
    static inline void UpdError(const real::Real &svar, double ovar)
    {
        double re = real::Real::CalcError(svar, ovar);
        if(re>1E-5) 
            assert(false);
        ERROR_STATE.updateError(svar.shadow->error, re);
    }
    #define INTERNAL_INIT_ERROR(v) ERROR_STATE.setError((v).shadow->error,0)
#if ACTIVE_TRACK_ERROR
    #define INTERNAL_ESTIMATE_ERROR(v) UpdError(v, (v).shadow->originalValue)
#else
    #define INTERNAL_ESTIMATE_ERROR(v)
#endif
#else 
    #define INTERNAL_ESTIMATE_ERROR(v)  
    #define INTERNAL_INIT_ERROR(v) 
#endif

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
#if TRACK_ERROR
            ERROR_STATE.setError(shadow->error, 0);
#endif
        }

        Real(Real &&r) noexcept
        {
            this->shadow = r.shadow;
            r.shadow = ShadowPool::INSTANCE.get();
            RealPool<Real>::INSTANCE.put(&r);
        }

        Real(const Real &r)
        {
#if TRACK_ERROR
            ERROR_STATE.setError(shadow->error, r.shadow->error.maxRelativeError);
#endif
            shadow = ShadowPool::INSTANCE.get();
            ASSIGN(shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue = r.shadow->originalValue;
#endif
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
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(shadow->error);
#endif
            Real &res = *RealPool<Real>::INSTANCE.get();
            SUB_DR(res.shadow->shadowValue, 0, this->shadow->shadowValue);
#if KEEP_ORIGINAL
            res.shadow->originalValue = - shadow->originalValue;
#endif
            return std::move(res);
        }

        INLINE_FLAGS friend Real &&operator+(const Real &l, const Real &r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
            ERROR_STATE.updateSymbolicVarError(r.shadow->error);
#endif
            Real &res = *RealPool<Real>::INSTANCE.get();
            ADD_RR(res.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue + r.shadow->originalValue;
#endif
            return std::move(res);
        }

        INLINE_FLAGS friend Real &&operator+(Real &&l, const Real &r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(r.shadow->error);
#endif
            ADD_RR(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            l.shadow->originalValue += r.shadow->originalValue;
#endif
            return std::move(l);
        }

        INLINE_FLAGS friend Real &&operator+(const Real &l, Real &&r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
#endif
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
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
            ERROR_STATE.updateSymbolicVarError(r.shadow->error);
#endif
            Real &res = *RealPool<Real>::INSTANCE.get();
            SUB_RR(res.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue - r.shadow->originalValue;
#endif
            return std::move(res);
        }

        INLINE_FLAGS friend Real &&operator-(Real &&l, const Real &r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(r.shadow->error);
#endif
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
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
#endif
            SUB_RR(r.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            r.shadow->originalValue = l.shadow->originalValue - r.shadow->originalValue;
#endif
            return std::move(r);
        }
        INLINE_FLAGS friend Real &&operator*(const Real &l, const Real &r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
            ERROR_STATE.updateSymbolicVarError(r.shadow->error);
#endif
            Real &res = *RealPool<Real>::INSTANCE.get();
            MUL_RR(res.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue * r.shadow->originalValue;
#endif
            return std::move(res);
        }

        INLINE_FLAGS friend Real &&operator*(Real &&l, const Real &r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(r.shadow->error);
#endif
            MUL_RR(l.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            l.shadow->originalValue *= r.shadow->originalValue;
#endif
            return std::move(l);
        }

        INLINE_FLAGS friend Real &&operator*(const Real &l, Real &&r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
#endif
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
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
            ERROR_STATE.updateSymbolicVarError(r.shadow->error);
#endif
            Real &res = *RealPool<Real>::INSTANCE.get();
            DIV_RR(res.shadow->shadowValue, l.shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            res.shadow->originalValue = l.shadow->originalValue / r.shadow->originalValue;
#endif
            return std::move(res);
        }

        INLINE_FLAGS friend Real &&operator/(Real &&l, const Real &r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(r.shadow->error);
#endif
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
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
#endif
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
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
#endif
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
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
#endif
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
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
#endif
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
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
#endif
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
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
#endif
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
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
#endif
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
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
#endif
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
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(l.shadow->error);
#endif
            Real &res = *RealPool<Real>::INSTANCE.get();
            DIV_DR(res.shadow->shadowValue, i, l.shadow->shadowValue);
#if KEEP_ORIGINAL
            res.shadow->originalValue = i / l.shadow->originalValue;
#endif
            return std::move(res);
        }

        INLINE_FLAGS Real &operator=(const Real &r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(r.shadow->error);
#endif
            ASSIGN(shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue = r.shadow->originalValue;
#endif
            INTERNAL_ESTIMATE_ERROR(*this);
            return *this;
        }
        INLINE_FLAGS Real &operator=(Real &&r)
        {
            SWAP(shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue = r.shadow->originalValue;
#endif
            RealPool<Real>::INSTANCE.put(&r);
            INTERNAL_ESTIMATE_ERROR(*this);
            return *this;
        }
        INLINE_FLAGS Real &operator=(const double r)
        {
            ASSIGN_D(shadow->shadowValue, r);
#if KEEP_ORIGINAL
            shadow->originalValue = r;
#endif
            INTERNAL_INIT_ERROR(*this);
            return *this;
        }

        INLINE_FLAGS Real &operator+=(const Real &r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(shadow->error);
            ERROR_STATE.updateSymbolicVarError(r.shadow->error);
#endif
            ADD_RR(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue += r.shadow->originalValue;
#endif
            INTERNAL_ESTIMATE_ERROR(*this);
            return *this;
        }

        INLINE_FLAGS Real &operator-=(const Real &r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(shadow->error);
            ERROR_STATE.updateSymbolicVarError(r.shadow->error);
#endif
            SUB_RR(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue -= r.shadow->originalValue;
#endif
            INTERNAL_ESTIMATE_ERROR(*this);
            return *this;
        }

        INLINE_FLAGS Real &operator*=(const Real &r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(shadow->error);
            ERROR_STATE.updateSymbolicVarError(r.shadow->error);
#endif
            MUL_RR(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue *= r.shadow->originalValue;
#endif
            INTERNAL_ESTIMATE_ERROR(*this);
            return *this;
        }

        INLINE_FLAGS Real &operator/=(const Real &r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(shadow->error);
            ERROR_STATE.updateSymbolicVarError(r.shadow->error);
#endif
            DIV_RR(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue /= r.shadow->originalValue;
#endif
            INTERNAL_ESTIMATE_ERROR(*this);
            return *this;
        }

        INLINE_FLAGS Real &operator+=(Real &&r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(shadow->error);
#endif
            ADD_RR(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue += r.shadow->originalValue;
#endif
            RealPool<Real>::INSTANCE.put(&r);
            INTERNAL_ESTIMATE_ERROR(*this);
            return *this;
        }

        INLINE_FLAGS Real &operator-=(Real &&r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(shadow->error);
#endif
            SUB_RR(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue -= r.shadow->originalValue;
#endif
            RealPool<Real>::INSTANCE.put(&r);
            INTERNAL_ESTIMATE_ERROR(*this);
            return *this;
        }

        INLINE_FLAGS Real &operator*=(Real &&r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(shadow->error);
#endif
            MUL_RR(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue *= r.shadow->originalValue;
#endif
            RealPool<Real>::INSTANCE.put(&r);
            INTERNAL_ESTIMATE_ERROR(*this);
            return *this;
        }

        INLINE_FLAGS Real &operator/=(Real &&r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(shadow->error);
#endif
            DIV_RR(this->shadow->shadowValue, this->shadow->shadowValue, r.shadow->shadowValue);
#if KEEP_ORIGINAL
            shadow->originalValue /= r.shadow->originalValue;
#endif
            RealPool<Real>::INSTANCE.put(&r);
            INTERNAL_ESTIMATE_ERROR(*this);
            return *this;
        }

        INLINE_FLAGS Real &operator+=(const double &r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(shadow->error);
#endif
            ADD_RD(this->shadow->shadowValue, this->shadow->shadowValue, r);
#if KEEP_ORIGINAL
            shadow->originalValue += r;
#endif
            INTERNAL_ESTIMATE_ERROR(*this);
            return *this;
        }

        INLINE_FLAGS Real &operator-=(const double &r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(shadow->error);
#endif
            SUB_RD(this->shadow->shadowValue, this->shadow->shadowValue, r);
#if KEEP_ORIGINAL
            shadow->originalValue -= r;
#endif
            INTERNAL_ESTIMATE_ERROR(*this);
            return *this;
        }

        INLINE_FLAGS Real &operator*=(const double &r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(shadow->error);
#endif
            MUL_RD(this->shadow->shadowValue, this->shadow->shadowValue, r);
#if KEEP_ORIGINAL
            shadow->originalValue *= r;
#endif
            INTERNAL_ESTIMATE_ERROR(*this);
            return *this;
        }

        INLINE_FLAGS Real &operator/=(const double &r)
        {
#if TRACK_ERROR
            ERROR_STATE.updateSymbolicVarError(shadow->error);
#endif
            DIV_RD(this->shadow->shadowValue, this->shadow->shadowValue, r);
#if KEEP_ORIGINAL
            shadow->originalValue /= r;
#endif
            INTERNAL_ESTIMATE_ERROR(*this);
            return *this;
        }

        friend std::ostream &operator<<(std::ostream &os, const Real &c)
        {
            STREAM_OUT(os, c);
            return os;
        }

        // Relational operator

        INLINE_FLAGS friend bool operator<(const Real &l, const Real &r)
        {
            return LESS_RR(l.shadow->shadowValue, r.shadow->shadowValue);
        }
        INLINE_FLAGS friend bool operator<(Real &&l, const Real &r)
        {
            bool ret = LESS_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&l);
            return ret;
        }
        INLINE_FLAGS friend bool operator<(const Real &l, Real &&r)
        {
            bool ret = LESS_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&r);
            return ret;
        }
        INLINE_FLAGS friend bool operator<(Real &&l, Real &&r)
        {
            bool ret = LESS_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&l);
            RealPool<Real>::INSTANCE.put(&r);
            return ret;
        }

        INLINE_FLAGS friend bool operator<=(const Real &l, const Real &r)
        {
            return LESSEQ_RR(l.shadow->shadowValue, r.shadow->shadowValue);
        }
        INLINE_FLAGS friend bool operator<=(Real &&l, const Real &r)
        {
            bool ret = LESSEQ_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&l);
            return ret;
        }
        INLINE_FLAGS friend bool operator<=(const Real &l, Real &&r)
        {
            bool ret = LESSEQ_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&r);
            return ret;
        }
        INLINE_FLAGS friend bool operator<=(Real &&l, Real &&r)
        {
            bool ret = LESSEQ_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&l);
            RealPool<Real>::INSTANCE.put(&r);
            return ret;
        }



        INLINE_FLAGS friend bool operator==(const Real &l, const Real &r)
        {
            return EQUAL_RR(l.shadow->shadowValue, r.shadow->shadowValue);
        }
        INLINE_FLAGS friend bool operator==(Real &&l, const Real &r)
        {
            bool ret = EQUAL_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&l);
            return ret;
        }
        INLINE_FLAGS friend bool operator==(const Real &l, Real &&r)
        {
            bool ret = EQUAL_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&r);
            return ret;
        }
        INLINE_FLAGS friend bool operator==(Real &&l, Real &&r)
        {
            bool ret = EQUAL_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&l);
            RealPool<Real>::INSTANCE.put(&r);
            return ret;
        }

        INLINE_FLAGS friend bool operator!=(const Real &l, const Real &r)
        {
            return !(EQUAL_RR(l.shadow->shadowValue, r.shadow->shadowValue));
        }
        INLINE_FLAGS friend bool operator!=(Real &&l, const Real &r)
        {
            bool ret = EQUAL_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&l);
            return !ret;
        }
        INLINE_FLAGS friend bool operator!=(const Real &l, Real &&r)
        {
            bool ret = EQUAL_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&r);
            return !ret;
        }
        INLINE_FLAGS friend bool operator!=(Real &&l, Real &&r)
        {
            bool ret = EQUAL_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&l);
            RealPool<Real>::INSTANCE.put(&r);
            return !ret;
        }


        INLINE_FLAGS friend bool operator>(const Real &l, const Real &r)
        {
            return GREATER_RR(l.shadow->shadowValue, r.shadow->shadowValue);
        }
        INLINE_FLAGS friend bool operator>(Real &&l, const Real &r)
        {
            bool ret = GREATER_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&l);
            return ret;
        }
        INLINE_FLAGS friend bool operator>(const Real &l, Real &&r)
        {
            bool ret = GREATER_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&r);
            return ret;
        }
        INLINE_FLAGS friend bool operator>(Real &&l, Real &&r)
        {
            bool ret = GREATER_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&l);
            RealPool<Real>::INSTANCE.put(&r);
            return ret;
        }


        INLINE_FLAGS friend bool operator>=(const Real &l, const Real &r)
        {
            return GREATEREQ_RR(l.shadow->shadowValue, r.shadow->shadowValue);
        }
        INLINE_FLAGS friend bool operator>=(Real &&l, const Real &r)
        {
            bool ret = GREATEREQ_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&l);
            return ret;
        }
        INLINE_FLAGS friend bool operator>=(const Real &l, Real &&r)
        {
            bool ret = GREATEREQ_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&r);
            return ret;
        }
        INLINE_FLAGS friend bool operator>=(Real &&l, Real &&r)
        {
            bool ret = GREATEREQ_RR(l.shadow->shadowValue, r.shadow->shadowValue);
            RealPool<Real>::INSTANCE.put(&l);
            RealPool<Real>::INSTANCE.put(&r);
            return ret;
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


#if KEEP_ORIGINAL
#define EXPBODY \
    EXP_R(res.shadow->shadowValue, r.shadow->shadowValue); \
    res.shadow->originalValue = exp(r.shadow->originalValue);\
    return std::move(res)
#else
#define EXPBODY \
    EXP_R(res.shadow->shadowValue, r.shadow->shadowValue); \
    return std::move(res)
#endif

    real::Real &&RealExp(const real::Real &r)
    {
#if TRACK_ERROR
        ERROR_STATE.updateSymbolicVarError(r.shadow->error);
#endif
        real::Real &res = *real::RealPool<real::Real>::INSTANCE.get();
        EXPBODY;
    }
    real::Real &&RealExp(real::Real &&r)
    {
        real::Real &res = r;
        EXPBODY;
    }
    real::Real &&RealExp(double dr)
    {
        real::Real &r = *real::RealPool<real::Real>::INSTANCE.get();
        real::Real &res = r;
        r = dr;
        EXPBODY;
    }
    

#if KEEP_ORIGINAL
#define POWBODY \
    POW_RR(res.shadow->shadowValue, a.shadow->shadowValue, b.shadow->shadowValue); \
    res.shadow->originalValue = pow(a.shadow->originalValue, b.shadow->originalValue); \
    return std::move(res)
#define POWBODY2(x) \
    POW_RR(res.shadow->shadowValue, a.shadow->shadowValue, b.shadow->shadowValue); \
    res.shadow->originalValue = pow(a.shadow->originalValue, b.shadow->originalValue); \
    real::RealPool<real::Real>::INSTANCE.put(&x); \
    return std::move(res)
#else
#define POWBODY \
    POW_RR(res.shadow->shadowValue, a.shadow->shadowValue, b.shadow->shadowValue); \
    return std::move(res)
#define POWBODY2(x) \
    POW_RR(res.shadow->shadowValue, a.shadow->shadowValue, b.shadow->shadowValue); \
    real::RealPool<real::Real>::INSTANCE.put(&x); \
    return std::move(res)
#endif

    real::Real &&RealPow(const real::Real &a, const real::Real &b)
    {
#if TRACK_ERROR
        ERROR_STATE.updateSymbolicVarError(a.shadow->error);
        ERROR_STATE.updateSymbolicVarError(b.shadow->error);
#endif
        real::Real &res = *real::RealPool<real::Real>::INSTANCE.get();
        POWBODY;
    }
    real::Real &&RealPow(real::Real &&a, const real::Real &b)
    {
#if TRACK_ERROR
        ERROR_STATE.updateSymbolicVarError(b.shadow->error);
#endif
        real::Real &res = a;
        POWBODY;
    }
    real::Real &&RealPow(const real::Real &a, real::Real &&b)
    {
#if TRACK_ERROR
        ERROR_STATE.updateSymbolicVarError(a.shadow->error);
#endif
        real::Real &res = b;
        POWBODY;
    }
    real::Real &&RealPow(real::Real &&a, real::Real &&b)
    {
        real::Real &res = a;
        POWBODY2(b);
    }
    real::Real &&RealPow(const real::Real &a, double db)
    {
#if TRACK_ERROR
        ERROR_STATE.updateSymbolicVarError(a.shadow->error);
#endif
        real::Real &b = *real::RealPool<real::Real>::INSTANCE.get();
        real::Real &res = b;
        b = db;
        POWBODY;
    }
    real::Real &&RealPow(double da, const real::Real &b)
    {
#if TRACK_ERROR
        ERROR_STATE.updateSymbolicVarError(b.shadow->error);
#endif
        real::Real &a = *real::RealPool<real::Real>::INSTANCE.get();
        real::Real &res = a;
        a = da;
        POWBODY;
    }
    real::Real &&RealPow(real::Real &&a, double db)
    {
        real::Real &res = a;
        real::Real b(db);
        POWBODY;
    }
    real::Real &&RealPow(double da, real::Real &&b)
    {
        real::Real a(da);
        real::Real &res = b;
        POWBODY;
    }
}; // namespace real

#endif
