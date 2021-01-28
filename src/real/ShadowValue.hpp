#ifndef SHADOW_VALUE_HPP
#define SHADOW_VALUE_HPP

#ifdef __LITTLE_ENDIAN
#define __HI(x) *(1+(int*)&x)
#define __LO(x) *(int*)&x
#else
#define __HI(x) *(int*)&x
#define __LO(x) *(1+(int*)&x)
#endif

#define __HI_SIG_BITS(x) (__HI(x) & 0x000FFFFF)
#define __EXP_BITS(x) ((__HI(x) & 0x7FF00000) >> 20)


#if  PORT_TYPE == DD_PORT
#include "DDPort.hpp"
#else
#include "MPFRPort.hpp"
#endif

#include "RealUtil.hpp"
#include "ErrorState.hpp"


namespace real
{
    struct ShadowState
    {
#if KEEP_ORIGINAL
        double originalValue;
#endif
        HP_TYPE shadowValue;
#if TRACK_ERROR
        SymbolicVarError error;
#endif
    };
    typedef ShadowState *sval_ptr;

    template <int p>
    struct ShadowSlotInitializer
    {
        inline void construct(ShadowState &v)
        {
            INIT(v.shadowValue, p);
#if TRACK_ERROR
            v.error = {0,0,0,0};
#endif
        }
        inline void destruct(ShadowState &v)
        {
            CLEAR(v.shadowValue);
        }
    };
    using ShadowPool = util::ValuePool<ShadowState, 128, ShadowSlotInitializer<120>>;


};


#endif