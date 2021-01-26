#ifndef EAST_H
#define EAST_H
#include "ShadowExecution.hpp"
// EAST interface for developers

void EAST_DUMP(std::ostream& stream, double d) {} // pseudo function

void EAST_DUMP(std::ostream& stream, const SVal &d) {
    stream << d << "\n";
}

void EAST_DUMP_ERROR(std::ostream& stream, double d) {stream << d <<"\n";} // pseudo function

void logError(std::ostream& stream, const SVal &sv, double re) 
{
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

void EAST_DUMP_ERROR(std::ostream& stream, const SVal &sv, double ov) 
{
    double re = CALCERR(sv, ov);
    logError(stream, sv, re);
}

void EAST_ANALYZE_ERROR(std::ostream& stream, double d) {} // pseudo function
void EAST_ANALYZE_ERROR(std::ostream& stream, const SVal &sv, double ov)
{
#if TRACK_ERROR
#if ACTIVE_TRACK_ERROR
    // we do nothing because the error has been tracked automatically
#else
    SVal::UpdError(sv, ov);
#endif
    double re = sv.shadow->error.maxRelativeError;
    logError(stream, sv, re);
#else
    EAST_DUMP_ERROR(stream, sv, ov);
#endif
}

bool EAST_CONDITION(std::ostream& stream, double v) {return v;}
bool EAST_CONDITION(std::ostream& stream, bool sv, bool ov)
{
    if(sv!=ov) stream << "Control flow divergence!\n";
    return ov;
}

#define EAST_ESCAPE_BEGIN PUSHCALL(0); // push an empty frame
#define EAST_ESCAPE_END POPCALL(); // pop the empty frame

#endif