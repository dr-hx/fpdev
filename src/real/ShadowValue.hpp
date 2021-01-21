#ifndef SHADOW_VALUE_HPP
#define SHADOW_VALUE_HPP

#include "MPFRPort.hpp"
#include "RealUtil.hpp"



namespace real
{
    struct ShadowState
    {
        double originalValue;
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