#ifndef DD_PORT_HPP
#define DD_PORT_HPP
#include <qd/dd_real.h>

#define HP_TYPE dd_real

#define ADD_RR(t, l, r) t = l + r
#define SUB_RR(t, l, r) t = l - r
#define MUL_RR(t, l, r) t = l * r
#define DIV_RR(t, l, r) t = l / r

#define ADD_RD(t, l, r) t = l + r
#define SUB_RD(t, l, r) t = l - r
#define SUB_DR(t, l, r) t = l - r
#define MUL_RD(t, l, r) t = l * r
#define DIV_RD(t, l, r) t = l / r
#define DIV_DR(t, l, r) t = l / r

#define ASSIGN(l,r) l = r
#define ASSIGN_D(l,r) l = r
#define SWAP(l,r) l = r

#define INIT(r, p) /*DO NOTHING*/
#define CLEAR(r) /*DO NOTHING*/

#define TO_DOUBLE(r) to_double(r)

#define FMA(t, l, m, r) t = l * m + r
#define FMS(t, l, m, r) t = l * m - r

#define STREAM_OUT(os, r)   r.shadow->shadowValue.dump("", os)


#define EXP_R(res, r) res = exp(r)
#define POW_RR(res, a, b) res=pow(a,b)

#endif