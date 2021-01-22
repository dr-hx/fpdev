#ifndef SHADOW_VALUE_HPP
#define SHADOW_VALUE_HPP
#include "RealConfigure.h"

#if  PORT_TYPE == DD_PORT
#include "DDPort.hpp"
#else
#include "MPFRPort.hpp"
#endif

#include "RealUtil.hpp"



namespace real
{
    struct ShadowState
    {
#if KEEP_ORIGINAL
        double originalValue;
#endif
        HP_TYPE shadowValue;
        double avgRelativeError;
    };
    typedef ShadowState *sval_ptr;

    template <int p>
    struct ShadowSlotInitializer
    {
        inline void construct(ShadowState &v)
        {
            INIT(v.shadowValue, p);
        }
        inline void destruct(ShadowState &v)
        {
            CLEAR(v.shadowValue);
        }
    };

    using ShadowPool = util::ValuePool<ShadowState, 128, ShadowSlotInitializer<120>>;
};
#endif