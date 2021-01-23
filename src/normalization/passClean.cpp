#include <string>
#include <set>
#include "../util/random.h"
#include "../transformer/transformer.hpp"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolingSampleCategory("ScDebug Tool");

auto target = nullStmt(isExpansionInMainFile(), hasParent(compoundStmt())).bind("null");

class CleanStmtPatHandler : public MatchHandler
{
public:
    CleanStmtPatHandler(std::map<std::string, Replacements> &r) : MatchHandler(r) {}
    
    virtual void run(const MatchFinder::MatchResult &Result)
    {
        
        const SourceManager *Manager = Result.SourceManager;
        const NullStmt *nullStmt = Result.Nodes.getNodeAs<NullStmt>("null");

        if(nullStmt!=NULL) {
            Replacement Rep = ReplacementBuilder::create(*Manager, nullStmt, "");
            Replacements &Replace = ReplaceMap[Rep.getFilePath().str()];
            llvm::Error err = Replace.add(Rep);
        }
    }
};

int main(int argc, const char **argv)
{
    CommonOptionsParser Options(argc, argv, ToolingSampleCategory);
    CodeTransformationTool tool(Options, "Pass Clean: remove empty statements");
    tool.add<decltype(target), CleanStmtPatHandler>(target);
    tool.run();
    return 0;
}