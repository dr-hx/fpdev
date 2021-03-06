#ifndef REAL_CONFIGURE_HPP
#define REAL_CONFIGURE_HPP
/*
Low level instructions for instrumentation
*/
#include "RealConfigure.h"
#include "Real.hpp"

using Addr = void*;
using SVal = real::Real;
using VarMap = real::util::VariableMap<Addr, SVal, CACHE_SIZE>;

// SHADOW FRAMEWORK
struct ShadowStackFrame
{
    SVal **realArgs;
    SVal *realRet;
    ShadowStackFrame* prev;
    ShadowStackFrame(int maxArg=0)
    {
        if(maxArg==0) realArgs = nullptr;
        else realArgs = new SVal*[maxArg] {nullptr};
        realRet = nullptr;
        prev = nullptr;
    }
    ~ShadowStackFrame()
    {
        delete[] realArgs;
    }

    inline ShadowStackFrame* pushCall(int maxArg)
    {
        ShadowStackFrame* nf = new ShadowStackFrame(maxArg);
        nf->prev = this;
        return nf;
    }
    inline void pushArg(int id, SVal& var)
    {
        realArgs[id] = &var;
    }
    inline void loadParm(int id, SVal& var, double ovar)
    {
        if(realArgs && realArgs[id])
        {
            var = *realArgs[id];
        }
        else
        {
            std::cout<<"use original in loadParm\n";
            var = ovar;
        }
    }
    inline void pushRet(int id, SVal& var)
    {
        realRet = &var;
    }
    inline ShadowStackFrame* popCall()
    {
        auto t = this->prev;
        delete this;
        return t;
    }
    inline void popRet(int id, SVal& svar, double ovar)
    {
        if(realRet)
        {
            svar = *realRet;
        }
        else
        {
            std::cout<<"use original in popRet\n";
            svar = ovar;
        }
    }
};
static ShadowStackFrame rootFrame;
static ShadowStackFrame* topFrame = &rootFrame;



// SHADOW LANGUAGE

#define L_SVAL static SVal
#define S_SVAL SVal&

#define VARMAP VarMap::INSTANCE
#define DEF(v) VARMAP.def(&(v))
#define UNDEF(v) VARMAP.undef(&(v))
#define SVAR(v) VARMAP.getOrInit(&(v))
#define ARR_SVAR(arr, idx) VARMAP.getFromArray(arr, idx)
// static real::Real tmp;
// #define SVAR(v) tmp

//VARMAP[&(v)]

void ARRDEF(double* arr, uint size)
{
    VARMAP.defArray(arr, size);
}

void ARRUNDEF(double* arr, uint size)
{
    // for(int i=0;i<size;i++)
    // {
    //     UNDEF(arr[i]);
    // } 
    VARMAP.undefArray(arr);
}

std::unordered_map<Addr, uint> dynArrSize;

double* DYNDEF(uint size)
{
    double *res = new double[size];
    // dynArrSize[(Addr)KEY_SHIFT(res)] = size;
    ARRDEF(res, size);
    return res;
}

void DYNUNDEF(double * res)
{
    // uint size = dynArrSize[(Addr)KEY_SHIFT(res)];
    ARRUNDEF(res, 0);
    delete res;
}

#if TRACK_ERROR
#define PC(id) ERROR_STATE.moveTo(id)
#else
#define PC(id) 
#endif

#define CALCERR(svar, ovar) real::Real::CalcError(svar, ovar)

#if TRACK_ERROR
#define UPDERR(svar, ovar) real::Real::UpdError(svar, ovar)
#endif

#define PUSHCALL(num) topFrame = topFrame->pushCall(num) 
#define POPCALL() topFrame = topFrame->popCall()
#define PUSHARG(id, svar) topFrame->pushArg(id, svar)
#define LOADPARM(id, svar, ovar)  topFrame->loadParm(id, svar, ovar)
#define PUSHRET(id, svar) topFrame->pushRet(id, svar)
#define POPRET(id, svar, ovar) topFrame->popRet(id, svar, ovar); topFrame = POPCALL()

#if TRACK_ERROR && KEEP_ORIGINAL==false
#define AUTOTRACK(v) EAST_ANALYSE_ERROR(v)
#else
#define AUTOTRACK(v)
#endif


#endif