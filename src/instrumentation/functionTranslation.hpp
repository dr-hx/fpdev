#ifndef FUNCTION_TRANSLATION_STRATEGY
#define FUNCTION_TRANSLATION_STRATEGY

#include <set>
#include <map>
#include "../transformer/transformer.hpp"

struct FunctionTranslationStrategy
{
    std::set<clang::SourceLocation> translatedFunctions;
    std::set<std::string> originalFunctions;
    std::map<std::string, std::string> abstractedFunctions;

    FunctionTranslationStrategy()
    {
        buildFunctionAbstraction();
    }

    void buildFunctionAbstraction()
    {
        abstractedFunctions.clear();
        abstractedFunctions["pow"] = "real::RealPow";
        abstractedFunctions["exp"] = "real::RealExp";
        abstractedFunctions["sqrt"] = "real::RealSqrt";
    }

    bool isTranslated(const clang::FunctionDecl* Func, const clang::SourceManager *Manager)
    {
        auto loc = Manager->getFileLoc(Func->getBeginLoc());
        return (translatedFunctions.count(loc)!=0);
    }

    bool useOriginalFunction(const std::string& name)
    {
        
        return originalFunctions.count(name)!=0;
    }

    bool isPseudoFunction(const std::string& name)
    {
        return name.find_first_of("EAST_", 0) == 0;
    }

    const std::string* hasAbstractedFunction(const std::string& name)
    {
        auto it = abstractedFunctions.find(name);
        if(it==abstractedFunctions.end()) return nullptr;
        else return &it->second;
    }

    void doTranslatePseudoFunctionCall(llvm::raw_ostream& stream, const clang::CallExpr* call, clang::PrinterHelper& helper)
    {
        auto funcName = call->getDirectCallee()->getNameAsString();
        
        if(funcName == "EAST_DUMP")
        {
            stream << MatchHandler::print(call, &helper) << "";
        }
        else if(funcName == "EAST_DUMP_ERROR" || funcName == "EAST_CONDITION")
        {
            stream << funcName << "("
                   << MatchHandler::print(call->getArg(0), &helper)
                   <<","
                   << MatchHandler::print(call->getArg(1), &helper)
                   <<","
                   << MatchHandler::print(call->getArg(1))
                   << ")";
        }
        else if(funcName == "EAST_ANALYZE_ERROR" || funcName == "EAST_SYNC" || funcName == "EAST_FIX")
        {
            stream << funcName << "("
                   << MatchHandler::print(call->getArg(0), &helper)
                   <<","
                   << MatchHandler::print(call->getArg(0))
                   << ")";
        }
        else if(funcName == "EAST_DRAW_ERROR")
        {
            stream << funcName << "("
                   << MatchHandler::print(call->getArg(0))
                   <<","
                   << MatchHandler::print(call->getArg(1), &helper)
                   <<","
                   << MatchHandler::print(call->getArg(2))
                   << ")";
        }
    }
};

#endif