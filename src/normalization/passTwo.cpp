#include <string>
#include "util/random.h"
#include "transformer/transformer.hpp"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ScDebugTool("ScDebug Normalization Tool");

//Pass Two - normalize calls to fp functions
// An fp func is a func that takes or returns fp values.
// 1. call to a fp func must be atomic
// 2. fp parameters of a call to a fp func must be atomic
// 3. all returned floating point values must be variables or constants

auto fpFunc = functionDecl(anyOf(hasType(realFloatingPointType()),hasAnyParameter(hasType(realFloatingPointType()))));
// auto fpCxxMemFpFunc = cxxMethodDecl(anyOf(hasType(realFloatingPointType()),hasAnyParameter(hasType(realFloatingPointType()))));

auto callFpFunc = callExpr(callee(fpFunc),hasAncestor(stmt(unless(expr())).bind("stmt"))).bind("expression");
//auto callCxxMemFpFunc = cxxMemberCallExpr(callee(fpCxxMemFpFunc),hasAncestor(stmt().bind("stmt"))).bind("expressions");

// auto containerStmt = stmt(anyOf(compoundStmt(), ifStmt(), forStmt(), switchStmt(), switchCase(), whileStmt(), doStmt()));
auto calls = expr(anyOf(callExpr(), cxxMemberCallExpr(), cxxOperatorCallExpr()));
auto arth = expr(anyOf(unaryOperator(), binaryOperator(), calls));

auto nonSimpleExpr = expr(allOf(hasAncestor(expr(allOf(calls, hasAncestor(stmt(unless(expr())).bind("stmt"))))), arth));

auto paramOfCall = callExpr(forEachArgumentWithParam(nonSimpleExpr.bind("expression"), parmVarDecl(hasType(realFloatingPointType()))));
// auto paramOfCall = expr(allOf(nonSimpleExpr, hasType(realFloatingPointType()), hasAncestor(callExpr()))).bind("expression");
//auto paramOfMemberCall = cxxMemberCallExpr(forEachArgumentWithParam(nonSimpleExpr.bind("expression"), parmVarDecl(hasType(realFloatingPointType()))));
//auto paramOfOperatorCall = cxxOperatorCallExpr(forEachArgumentWithParam(nonSimpleExpr.bind("expression"), parmVarDecl(hasType(realFloatingPointType()))));

auto RetStmtPat = returnStmt(hasReturnValue(expr(allOf(hasType(realFloatingPointType()), arth)).bind("expression"))).bind("stmt");

//returnStmt(hasReturnValue(expr(allOf(hasType(realFloatingPointType()), nonSimpleExpr)).bind("expression"))).bind("stmt")

auto target = stmt(eachOf(paramOfCall, RetStmtPat, callFpFunc));

class ParamPatHandler : public MatchHandler
{
public:
    virtual void onEndOfTranslationUnit()
    {
        ExprReplacementHelper helper;
        auto exts = extractions;
        std::sort(exts.begin(), exts.end());

        std::ostringstream out;
        const ExpressionExtractionRequest *lastExt = NULL;

        auto unique = std::unique(exts.begin(), exts.end());
        exts.erase(unique, exts.end());

        auto extIt = exts.begin();
        while (lastExt != NULL || extIt != exts.end())
        {
            if (lastExt!=NULL && (extIt == exts.end() || lastExt->statement != extIt->statement))
            {
                out.flush();
                std::string parmDef = out.str();
                auto filename = lastExt->manager->getFilename(lastExt->statement->getBeginLoc()).str();
                Replacement Rep1(*lastExt->manager, lastExt->statement->getBeginLoc(), 0, "{\n" + parmDef);
                std::string stmt_Str = print(lastExt->expression, &helper);
                Replacement Rep2(*lastExt->manager, lastExt->expression, stmt_Str);
                Replacement Rep3(*lastExt->manager, lastExt->statement->getEndLoc(), 0, "}\n");
                Replacements &Replace = ReplaceMap[filename];
                llvm::Error err1 = Replace.add(Rep1);
                llvm::Error err2 = Replace.add(Rep2);
                llvm::Error err3 = Replace.add(Rep3);
                lastExt = NULL;
                out.str("");
            }
            else
            {
                std::string parTmp = randomIdentifier("paramTmp");
                out << extIt->type.getAsString()
                    << " " << parTmp << " = " << print(extIt->expression, &helper) << ";" << std::endl;
                helper.addShortcut(extIt->expression, parTmp); // must after the above line
                lastExt = &*extIt;
                extIt++;
            }
        }
        extractions.clear();
    }

    ParamPatHandler(std::map<std::string, Replacements> &r) : MatchHandler(r) {}
    

    std::vector<ExpressionExtractionRequest> extractions;
    virtual void run(const MatchFinder::MatchResult &Result)
    {
        const Expr *actual = Result.Nodes.getNodeAs<Expr>("expression");
        const Stmt *stmt = Result.Nodes.getNodeAs<Stmt>("stmt");
        const ParmVarDecl *formal = Result.Nodes.getNodeAs<ParmVarDecl>("formal");
        QualType type;
        if(formal==NULL) type = actual->getType();
        else type = formal->getType();
        // actual->dumpColor();
        extractions.push_back(ExpressionExtractionRequest(actual, type, stmt, Result.SourceManager));
    }
};

// Pass Two
// all assignments to double variable or integeral pointer (if we know it points to a double value)
//  in compound expression
//  in for condition/inc
//  in while/do-while condition
//  in function-call
//  in return;
// flatten multiple declarations

// auto SwitchStmtPat_DeclInCond = switchStmt(hasInitStatement(anything())).bind("parent");
// auto WhileStmtPat_DeclInCond = whileStmt(hasConditionVariableStatement(declStmt().bind("decl"))).bind("parent");

int main(int argc, const char **argv)
{
    CodeTransformationTool tool(argc, argv, ScDebugTool);

    ParamPatHandler handler(tool.GetReplacements());

    tool.add(target,handler);
    // tool.add(callFpFunc,handler);

    tool.run();

    return 0;
}