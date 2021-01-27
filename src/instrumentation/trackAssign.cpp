#include <string>
#include <set>
#include "../util/random.h"
#include "../transformer/transformer.hpp"
#include "../transformer/analysis.hpp"
#include "functionTranslation.hpp"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ScDebugTool("ScDebug Tool");


auto fpType = realFloatingPointType();
auto fpInc = unaryOperator(hasAnyOperatorName("++", "--"), hasUnaryOperand(expr(hasType(fpType)).bind("var"))).bind("stmt");
auto fpAssign = binaryOperator(isAssignmentOperator(), hasType(fpType), hasLHS(expr().bind("var"))).bind("stmt");
auto target = compoundStmt(isExpansionInMainFile(), forEach(stmt(anyOf(fpInc, fpAssign))));

class FpAssignmentTracking : public MatchHandler
{
public:
    FpAssignmentTracking(std::map<std::string, Replacements> &r) : MatchHandler(r) {}

    virtual void run(const MatchFinder::MatchResult &Result)
    {
        const Stmt *st = Result.Nodes.getNodeAs<Stmt>("stmt");
        const Expr *v = Result.Nodes.getNodeAs<Expr>("var");

        std::string code;
        llvm::raw_string_ostream stream(code);

        stream  << print(st) <<";"
                << "EAST_ANALYSE_ERROR("<<print(v)<<");";
        
        auto rep = ReplacementBuilder::create(*Result.SourceManager, st, code);
        addReplacement(rep);
    }
};

int main(int argc, const char **argv)
{
    CommonOptionsParser Options(argc, argv, ScDebugTool);
    CodeTransformationTool tool(Options, "Instrumentation: FpArith");
    FpAssignmentTracking handler(tool.GetReplacements());
    // var use
    tool.add(target, handler);
    tool.run();
    return 0;
}