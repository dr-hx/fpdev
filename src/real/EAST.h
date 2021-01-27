#ifndef EAST_H
#define EAST_H
#include "ShadowExecution.hpp"
// EAST interface for developers

void EAST_DUMP(std::ostream& stream, double d) {} // pseudo function

void EAST_DUMP(std::ostream& stream, const SVal &d) {
    stream << d << "\n";
}

void EAST_DUMP_ERROR(std::ostream& stream, double d) {stream << d <<"\n";} // pseudo function

void EAST_DUMP_ERROR(std::ostream& stream, const SVal &sv, double ov) 
{
    stream << "[ERROR]\t" << "Shadow value is ";
    EAST_DUMP(stream, sv);
#if TRACK_ERROR
    if(sv.shadow->error.maxRelativeError==0)
    {
        stream <<"[ERROR]\t" << "MRE < 10^-16\n";
    }
    else 
    {
        stream <<"[ERROR]\t" << "MRE is "<<sv.shadow->error.maxRelativeError<<", caused by "<< ERROR_STATE.locationStrings[sv.shadow->error.errorCausingCalculationID] <<"\n";
    }
    if(sv.shadow->error.relativeErrorOfLastCheck==0)
    {
        stream <<"[ERROR]\t" << "LRE < 10^-16\n";
    }
    else 
    {
        stream <<"[ERROR]\t" << "LRE is "<<sv.shadow->error.relativeErrorOfLastCheck<<", caused by "<< ERROR_STATE.locationStrings[sv.shadow->error.errorCausingCalculationIDOfLastCheck] <<"\n";
    }
#if ACTIVE_TRACK_ERROR
    double re = CALCERR(sv, ov);
    if(re==0)
    {
        stream <<"[ERROR]\t" << "Current RE < 10^-16\n";
    }
    else 
    {
        stream <<"[ERROR]\t" << "Current RE is "<<re<<"\n";
    }
#endif
#else
    double re = CALCERR(sv, ov);
    if(re==0)
    {
        stream <<"[ERROR]\t" << "Current RE < 10^-16\n";
    }
    else 
    {
        stream <<"[ERROR]\t" << "Current RE is "<<re<<"\n";
    }
#endif

}

void EAST_ANALYZE_ERROR(std::ostream& stream, double d) {} // pseudo function
void EAST_ANALYZE_ERROR(std::ostream& stream, const SVal &sv, double ov)
{
#if TRACK_ERROR
#if ACTIVE_TRACK_ERROR
#if DEBUG_INTERNAL
    if(ov != sv.shadow->originalValue)
    {
        assert(false);
    }
#endif
#else // track error debugging mode
    SVal::UpdError(sv, ov);
#endif
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

void EAST_DRAW_ERROR(std::string name, double v, std::string file) {}
void EAST_DRAW_ERROR(std::string name, const SVal &sv, std::string file) 
{
#if TRACK_ERROR
    ERROR_STATE.visualizeTo(file, sv.shadow->error, name);
#endif
}

#if ACTIVE_TRACK_ERROR && TRACKING_ON==false
#define EAST_TRACKING_ON() ERROR_STATE.setTracking(true)
#define EAST_TRACKING_OFF() ERROR_STATE.setTracking(false)
#else
#define EAST_TRACKING_ON()
#define EAST_TRACKING_OFF() 
#endif


#endif