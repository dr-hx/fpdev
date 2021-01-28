#ifndef DD_PORT_HPP
#define DD_PORT_HPP
#include <iomanip>
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

#if KEEP_ORIGINAL
#define STREAM_OUT(os, r)  {\
    std::ios_base::fmtflags old_flags = os.flags(); \
    std::streamsize old_prec = os.precision(19); \
    os << std::scientific; \
    os << "[ " << std::setw(27) << r.shadow->shadowValue.x[0] << ", " << std::setw(27) << r.shadow->shadowValue.x[1] << " ]"; \
    os << " (original = " << r.shadow->originalValue <<")"; \
    os.precision(old_prec); \
    os.flags(old_flags); \
}
#else
#define STREAM_OUT(os, r)  {\
    std::ios_base::fmtflags old_flags = os.flags(); \
    std::streamsize old_prec = os.precision(19); \
    os << std::scientific; \
    os << "[ " << std::setw(27) << r.shadow->shadowValue.x[0] << ", " << std::setw(27) << r.shadow->shadowValue.x[1] << " ]"; \
    os.precision(old_prec); \
    os.flags(old_flags); \
}
#endif

#define LESS_RR(l,r) l<r
#define LESSEQ_RR(l,r) l<=r
#define EQUAL_RR(l,r) l==r
#define GREATER_RR(l,r) l>r
#define GREATEREQ_RR(l,r) l>=r

#define EXP_R(res, r) res = exp(r)
#define POW_RR(res, a, b) res=pow(a,b)

#define COPY_EXP_D(res, d) __HI(res.x[0])=__HI(d)
#define CLEAR_LOWS(res) __LO(res.x[0]) = 0, res.x[1] = 0
#endif