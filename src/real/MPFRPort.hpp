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

#define FMA(t, l, m, r) mpfr_fma(t, l, m, r, RND)
#define FMS(t, l, m, r) mpfr_fms(t, l, m, r, RND)

#endif