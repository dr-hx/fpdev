#include "stdlib.h"
#include "stdio.h"
#include "varusage.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolingSampleCategory("ScDebug Tool");

int main(int argc, char const *argv[])
{
    
    CommonOptionsParser op(argc, argv, ToolingSampleCategory);
    ClangTool Tool(op.getCompilations(), op.getSourcePathList());
    MatchFinder Finder;

    auto fpType = realFloatingPointType();
    ustb::clang::varusage::VarReferenceTable table;
    ustb::clang::varusage::VarDefRefHandler handler(&table, "var-def", "var-ref");
 
    ustb::clang::varusage::buildDefMatcher(fpType, "var-def", Finder, handler);
    ustb::clang::varusage::buildRefMatcher("var-ref", Finder, handler);

    return Tool.run(newFrontendActionFactory(&Finder).get());
}
