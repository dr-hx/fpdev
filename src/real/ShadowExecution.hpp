#ifndef REAL_CONFIGURE_HPP
#define REAL_CONFIGURE_HPP
#include "Real.hpp"

using Addr = void*;
using SVal = real::Real;
using VarMap = real::VariableMap<Addr, SVal, 0x800>;

#define VARMAP VarMap::INSTANCE
#define DEF(v) VARMAP[&(v)]
#define UNDEF(v) VARMAP[&(v)]
#define SVAL(v) VARMAP[&(v)]
#endif