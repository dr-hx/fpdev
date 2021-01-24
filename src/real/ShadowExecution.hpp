#ifndef REAL_CONFIGURE_HPP
#define REAL_CONFIGURE_HPP
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
        if(maxArg==0) realArgs = NULL;
        else realArgs = new SVal*[maxArg] {NULL};
        realRet = NULL;
        prev = NULL;
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
// #define SVAR(v) VARMAP.getOrInit(&(v))
#define SVAR(v) VARMAP[&(v)]

void ARRDEF(double* arr, uint size)
{
    for(int i=0;i<size;i++)
    {
        DEF(arr[i]);
    } 
}

void ARRUNDEF(double* arr, uint size)
{
    for(int i=0;i<size;i++)
    {
        UNDEF(arr[i]);
    } 
}

std::unordered_map<Addr, uint> dynArrSize;

double* DYNDEF(uint size)
{
    double *res = new double[size];
    dynArrSize[(Addr)KEY_SHIFT(res)] = size;
    ARRDEF(res, size);
    return res;
}

void DYNUNDEF(double * res)
{
    uint size = dynArrSize[(Addr)KEY_SHIFT(res)];
    ARRUNDEF(res, size);
    delete res;
}

#define PUSHCALL(num) topFrame = topFrame->pushCall(num) 
#define POPCALL() topFrame = topFrame->popCall()
#define PUSHARG(id, svar) topFrame->pushArg(id, svar)
#define LOADPARM(id, svar, ovar)  topFrame->loadParm(id, svar, ovar)
#define PUSHRET(id, svar) topFrame->pushRet(id, svar)
#define POPRET(id, svar, ovar) topFrame->popRet(id, svar, ovar); topFrame = POPCALL()

#endif