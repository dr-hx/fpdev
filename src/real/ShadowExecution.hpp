#ifndef REAL_CONFIGURE_HPP
#define REAL_CONFIGURE_HPP
#include "RealConfigure.h"
#include "Real.hpp"

using Addr = void*;
using SVal = real::Real;
using VarMap = real::util::VariableMap<Addr, SVal, 0x800>;


#define L_SVAL static SVal
#define S_SVAL SVal&

#define VARMAP VarMap::INSTANCE
#define DEF(v) VARMAP[&(v)]
#define UNDEF(v) VARMAP.undef(&(v))
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

#endif