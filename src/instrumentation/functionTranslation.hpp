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
        if(it==abstractedFunctions.end()) return NULL;
        else return &it->second;
    }
};

#endif