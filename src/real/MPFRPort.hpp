#ifndef MPFR_PORT_HPP
#define MPFR_PORT_HPP
#include <mpfr.h>

#define HP_TYPE mpfr_t
#define RND MPFR_RNDN

#define ADD_RR(t, l, r) mpfr_add(t, l, r, RND)
#define SUB_RR(t, l, r) mpfr_sub(t, l, r, RND)
#define MUL_RR(t, l, r) mpfr_mul(t, l, r, RND)
#define DIV_RR(t, l, r) mpfr_div(t, l, r, RND)

#define ADD_RD(t, l, r) mpfr_add_d(t, l, r, RND)
#define SUB_RD(t, l, r) mpfr_sub_d(t, l, r, RND)
#define SUB_DR(t, l, r) mpfr_d_sub(t, l, r, RND)
#define MUL_RD(t, l, r) mpfr_mul_d(t, l, r, RND)
#define DIV_RD(t, l, r) mpfr_div_d(t, l, r, RND)
#define DIV_DR(t, l, r) mpfr_d_div(t, l, r, RND)

#define ASSIGN(l,r) mpfr_set(l,r, RND)
#define ASSIGN_D(l,r) mpfr_set_d(l,r, RND)
#define SWAP(l,r) mpfr_swap(l, r)

#define INIT(r, p) mpfr_init2(r, p)
#define CLEAR(r) mpfr_clear(r)

#define TO_DOUBLE(r) mpfr_get_d(r, RND)


#if KEEP_ORIGINAL
#define STREAM_OUT(os, r)   static char buf[512]; \
                            mpfr_snprintf(buf, 512, "%.32Re (original = %.16e)", c.shadow->shadowValue, c.shadow->originalValue); \
                            os << buf
#else
#define STREAM_OUT(os, r)   static char buf[512]; \
                            mpfr_snprintf(buf, 512, "%.32Re", c.shadow->shadowValue); \
                            os << buf
#endif

#define FMA(t, l, m, r) mpfr_fma(t, l, m, r, RND)
#define FMS(t, l, m, r) mpfr_fms(t, l, m, r, RND)


#define LESS_RR(l,r) mpfr_less_p(l,r)
#define LESSEQ_RR(l,r) mpfr_lessequal_p(l,r)
#define EQUAL_RR(l,r) mpfr_equal_p(l,r)
#define GREATER_RR(l,r) mpfr_greater_p(l,r)
#define GREATEREQ_RR(l,r) mpfr_greaterequal_p(l,r)



#define EXP_R(res, r) mpfr_exp(res, r, RND)
#define POW_RR(res, a, b) mpfr_pow(res, a, b, RND) 
#define SQRT_R(res, r) mpfr_sqrt(res, r, RND)


#define COPY_EXP_D(res, d) mpfr_set_exp(res, __EXP_BITS(d))


static const int MP_LIMB_T_SIZE = sizeof(mp_limb_t)*8;

#define CLEAR_LOWS(res) do {\
    int __l = (res->_mpfr_prec+MP_LIMB_T_SIZE-1)/MP_LIMB_T_SIZE; \
    for(int __i=0; __i<__l-1;__i++) res->_mpfr_d[__i]=0;\
    int clearBits = MP_LIMB_T_SIZE - 21; \
    res->_mpfr_d[__l-1] = (res->_mpfr_d[__l-1] >> clearBits) << clearBits; \
    } while(0)

#endif