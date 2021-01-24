#ifndef EAST_H
#define EAST_H
#include "ShadowExecution.hpp"
// EAST interface for developers

void EAST_DUMP(std::ostream& stream, double d) {} // pseudo function

void EAST_DUMP(std::ostream& stream, const SVal &d) {
    stream << d << "\n";
}

void EAST_ANALYZE(std::ostream& stream, double d) {stream << d <<"\n";} // pseudo function

void EAST_ANALYZE(std::ostream& stream, const SVal &sv, double ov) 
{
    double dsv = TO_DOUBLE(sv.shadow->shadowValue);
    if(dsv==0) dsv = 1.1E-16;
    double re = (dsv-ov)/dsv;
    long *pre = (long*)&re;
    *pre &= 0x7FFFFFFFFFFFFFFF;
    EAST_DUMP(stream, sv);
    if(re==0)
    {
        stream <<"Relative error is smaller than 10^-16\n";
    }
    else
    {
        stream <<"Relative error is " << re << "\n";
    }
}

bool EAST_CONDITION(std::ostream& stream, double v) {return v;}
bool EAST_CONDITION(std::ostream& stream, bool sv, bool ov)
{
    if(sv!=ov) stream << "Control flow divergence!\n"
    return ov;
}

#define EAST_ESCAPE_BEGIN PUSHCALL(0); // push an empty frame
#define EAST_ESCAPE_END POPCALL(); // pop the empty frame

#endif