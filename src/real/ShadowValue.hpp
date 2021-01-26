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
    // error state is related to memory
    struct SymbolicVarError
    {
        double maxRelativeError;
        double relativeErrorOfLastCheck;
        uint64 errorCausingCalculationID;
        uint64 errorCausingCalculationIDOfLastCheck;

        void update(const SymbolicVarError& r)
        {
            relativeErrorOfLastCheck = r.relativeErrorOfLastCheck;
            errorCausingCalculationIDOfLastCheck = r.errorCausingCalculationIDOfLastCheck;
            if(maxRelativeError < r.maxRelativeError)
            {
                maxRelativeError = r.maxRelativeError;
                errorCausingCalculationID = r.errorCausingCalculationID;
            }
        }

        void update(double re, uint id)
        {
            relativeErrorOfLastCheck = re;
            errorCausingCalculationIDOfLastCheck = id;
            if(maxRelativeError < re)
            {
                maxRelativeError = re;
                errorCausingCalculationID = id;
            }
        }
    };

    struct CalculationError
    {
        double maxRelativeError;
        double relativeErrorOfLastCheck;
        std::vector<SymbolicVarError> inputVars;

        CalculationError() : maxRelativeError(0),relativeErrorOfLastCheck(0) {}

        void updateSymbolicVarError(const SymbolicVarError& var, uint id)
        {
            if(inputVars.size() == id)
            {
                inputVars.push_back(var);
                return; // short-cut
            }
            
            if(inputVars.size() <= id)
            {
                inputVars.reserve(id<32 ? 32 : (id + id/2));
                inputVars.resize(id+1);
            }

            inputVars[id].update(var);
        }
    };

    struct ProgramErrorState
    {
        uint64 programCounter;
        uint symbolicVarId;
        const char **locationStrings;
        CalculationError* errors;

        ProgramErrorState() : programCounter(0), symbolicVarId(0), locationStrings(NULL),errors(NULL) 
        {
#ifdef PC_COUNT
            std::cout<<"Error State Inited!\n";
            initErrors(PC_COUNT);
            setLocationStrings(PATH_STRINGS);
#endif
        }
        ~ProgramErrorState()
        {
            if(errors)
            {
                delete[] errors;
                errors = NULL;
            }
        }

        void initErrors(uint64 count)
        {
            errors = new CalculationError[count];
        }

        void setLocationStrings(const char **ls)
        {
            locationStrings = ls;
        }

        void moveTo(uint64 c)
        {
            programCounter = c;
            symbolicVarId = 0;
        }

        void setError(SymbolicVarError &var, double re)
        {
            var.maxRelativeError = re;
            var.relativeErrorOfLastCheck = re;
            var.errorCausingCalculationID = programCounter;
            var.errorCausingCalculationIDOfLastCheck = programCounter;
        }

        void updateError(SymbolicVarError &var, double re)
        {
            var.update(re, programCounter);
        }

        void updateSymbolicVarError(const SymbolicVarError &var)
        {
            if(errors)
            {
                errors[programCounter].updateSymbolicVarError(var, symbolicVarId++);
            }
        }
    };

#if TRACK_ERROR
static ProgramErrorState programErrorState;

#define ERROR_STATE real::programErrorState
#endif


    struct ShadowState
    {
#if KEEP_ORIGINAL
        double originalValue;
#endif
        HP_TYPE shadowValue;
#if TRACK_ERROR
        SymbolicVarError error;
#endif
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