#ifndef ERROR_STATE_HPP
#define ERROR_STATE_HPP
#include <set>
#include <sstream>
#include <string>
#include <fstream>
#include "RealConfigure.h"
#include "RealUtil.hpp"

#define MAX_ULONG 0xFFFFFFFFFFFFFFFF

namespace real
{
    // error state is related to memory
    struct SymbolicVarError
    {
        double maxRelativeError;
        uint64 errorCausingCalculationID;
        double relativeErrorOfLastCheck;
        uint64 errorCausingCalculationIDOfLastCheck;

        void update(const SymbolicVarError &r)
        {
            relativeErrorOfLastCheck = r.relativeErrorOfLastCheck;
            errorCausingCalculationIDOfLastCheck = r.errorCausingCalculationIDOfLastCheck;
            if (maxRelativeError < r.maxRelativeError)
            {
                maxRelativeError = r.maxRelativeError;
                errorCausingCalculationID = r.errorCausingCalculationID;
            }
        }

        void update(double re, uint64 id)
        {
            relativeErrorOfLastCheck = re;
            errorCausingCalculationIDOfLastCheck = id;
            if (maxRelativeError < re)
            {
                maxRelativeError = re;
                errorCausingCalculationID = id;
            }
        }

        inline void initError(double re, uint64 id)
        {
            maxRelativeError = re;
            relativeErrorOfLastCheck = re;
            errorCausingCalculationID = id;
            errorCausingCalculationIDOfLastCheck = id;
        }
    };

    struct CalculationError
    {
        // double maxRelativeError;
        // double relativeErrorOfLastCheck;
        std::vector<SymbolicVarError> inputVars;

        CalculationError() {}
        //: maxRelativeError(0),relativeErrorOfLastCheck(0) {}

        void updateSymbolicVarError(const SymbolicVarError &var, uint id)
        {
            if (inputVars.size() == id)
            {
                inputVars.push_back(var);
                return; // short-cut
            }

            if (inputVars.size() < id)
            {
                inputVars.reserve(id < 32 ? 32 : (id + id / 2));
                inputVars.resize(id + 1);
            }

            inputVars[id].update(var);
        }
    };

    struct ProgramErrorState
    {
        uint64 programCounter;
        uint symbolicVarId;
        const char **locationStrings;
#if ACTIVE_TRACK_ERROR && TRACKING_ON==false
        bool tracking;
#endif

#ifdef PC_COUNT
        CalculationError errors[PC_COUNT];
#else
        CalculationError *errors;
#endif
        ProgramErrorState() : programCounter(0), symbolicVarId(0), locationStrings(nullptr)
        {
#ifdef PC_COUNT
            std::cout << "Error State Inited!\n";
            setLocationStrings(PATH_STRINGS);
#else
            errors = nullptr;
#endif

#if ACTIVE_TRACK_ERROR && TRACKING_ON==false
            tracking = false;
#endif
            std::cout 
                    << "Tracking Error: "<<TRACK_ERROR<<"\n"
                    << "Active Tracking Error: "<<ACTIVE_TRACK_ERROR<<"\n"
                    << "Tracking On: "<<TRACKING_ON<<"\n";
        }
        ~ProgramErrorState()
        {
#ifndef PC_COUNT
            if (errors)
            {
                delete[] errors;
                errors = nullptr;
            }
#endif
        }

#ifndef PC_COUNT
        void initErrors(uint64 count)
        {
            errors = new CalculationError[count];
        }
#endif

#if ACTIVE_TRACK_ERROR && TRACKING_ON==false
        inline void setTracking(bool flag)
        {
            tracking = flag;
        }
#endif
        void setLocationStrings(const char **ls)
        {
            locationStrings = ls;
        }

        inline void moveTo(uint64 c)
        {
            programCounter = c;
            symbolicVarId = 0;
        }

        inline void setError(SymbolicVarError &var, double re)
        {
#if ACTIVE_TRACK_ERROR && TRACKING_ON==false
            if(!tracking) return;
#endif
            var.initError(re, programCounter);
        }

        inline void updateError(SymbolicVarError &var, double re)
        {
#if ACTIVE_TRACK_ERROR && TRACKING_ON==false
            if(!tracking) return;
#endif
            var.update(re, programCounter);
        }

        inline void updateSymbolicVarError(const SymbolicVarError &var)
        {
#if ACTIVE_TRACK_ERROR && TRACKING_ON==false
            if(!tracking) return;
#endif
#ifndef PC_COUNT
            if (errors)
            {
                errors[programCounter].updateSymbolicVarError(var, symbolicVarId++);
            }
#else
            errors[programCounter].updateSymbolicVarError(var, symbolicVarId++);
#endif
        }

        void visualizeTo(const std::string &filename, const SymbolicVarError &root, const std::string &name)
        {
            std::ofstream outfile;
            outfile.open(filename, std::ios::out | std::ios::trunc);
            visualize(outfile, root, name);
            outfile.flush();
            outfile.close();
        }

        void visualize(std::ostream &stream, const SymbolicVarError &root, const std::string &name)
        {
            std::set<void *> visited;
            stream << "digraph root {\n";
            visualize(stream, root, name, visited);
            stream << "}";
        }

        void visualize(std::ostream &stream, const SymbolicVarError &var, const std::string &name, std::set<void *> &visited, const std::string *from = nullptr)
        {
            if (visited.count((void *)&var) != 0)
                return;
            visited.insert((void *)&var);
            stream << name
                   << " [shape=record, label=\"{"
                   << name << "|"
                   << "MRE=" << var.maxRelativeError
                   << "}\"];\n";
            uint64 causing = var.errorCausingCalculationID;
            if (from != nullptr)
            {
                stream << *from << "->" << name << ";\n";
            }
            visualize(stream, causing, visited, &name);
        }
        void visualize(std::ostream &stream, uint64 PC, std::set<void *> &visited, const std::string *from = nullptr)
        {
            if (visited.count((void *)&errors[PC]) != 0)
                return;
            visited.insert((void *)&errors[PC]);
            auto &calc = errors[PC];

            std::ostringstream calcNameString("");
            calcNameString << "_CALC_" << PC << "";
            std::string name = calcNameString.str();

            stream << name
                   << " [shape=ellipse, label=\""
                   << PC << ":" << locationStrings[PC]
                   << "\"];\n";
            if (from != nullptr)
            {
                stream << *from << "->" << name << ";\n";
            }
            for (int i = 0, size = calc.inputVars.size(); i < size; i++)
            {
                auto &var = calc.inputVars[i];
                std::ostringstream namestream("");
                namestream << "_CALC_" << PC << "_" << i;
                std::string varName = namestream.str();
                visualize(stream, var, varName, visited, &name);
            }
        }
    };

#if TRACK_ERROR
    static ProgramErrorState programErrorState;

#define ERROR_STATE real::programErrorState
#endif
}; // namespace real

#endif