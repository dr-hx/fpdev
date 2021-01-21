#ifndef SHADOW_VALUE_HPP
#define SHADOW_VALUE_HPP
#include <mpfr.h>
#include "RealUtil.hpp"

namespace real
{
    struct ShadowState
    {
        double originalValue;
        mpfr_t shadowValue;
        double avgRelativeError;
    };
    typedef ShadowState *sval_ptr;

    template <int p>
    struct ShadowSlotInitializer
    {
        inline void construct(ShadowState &v)
        {
            mpfr_init2(v.shadowValue, p);
        }
        inline void destruct(ShadowState &v)
        {
            mpfr_clear(v.shadowValue);
        }
    };

    using ShadowPool = util::ValuePool<ShadowState, 128, ShadowSlotInitializer<120>>;
};
#endif