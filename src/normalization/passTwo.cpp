#include <string>
#include "../util/random.h"
#include "../transformer/transformer.hpp"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ScDebugTool("ScDebug Tool");

//Pass Two - normalize calls to fp functions
// An fp func is a func that takes or returns fp values.
// 1. call to a fp func must be atomic
// 2. fp parameters of a call to a fp func must be atomic
// 3. all returned floating point values must be variables or constants

auto fpFunc = functionDecl(anyOf(returns(realFloatingPointType()), hasAnyParameter(hasType(realFloatingPointType()))));
// auto fpCxxMemFpFunc = cxxMethodDecl(anyOf(hasType(realFloatingPointType()),hasAnyParameter(hasType(realFloatingPointType()))));

auto rootStmt = stmt(anyOf(unless(expr()), expr(hasParent(compoundStmt()))));

//FIXME, stmt(unless(expr()) cannot handle assignments and other cases
auto callFpFunc = callExpr(
                      callee(fpFunc),                                                                         // call to fp funct
                      hasAncestor(rootStmt.bind("stmt")),                                         // find root stmt
                      unless(hasParent(compoundStmt())), // exclude direct call
                      unless(hasParent(binaryOperator(isAssignmentOperator()))),                              // exclude direct assignment
                      unless(hasParent(varDecl()))                                                            // exclude direct initialization
                      )
                      .bind("expression");
//auto callCxxMemFpFunc = cxxMemberCallExpr(callee(fpCxxMemFpFunc),hasAncestor(stmt().bind("stmt"))).bind("expressions");

// auto containerStmt = stmt(anyOf(compoundStmt(), ifStmt(), forStmt(), switchStmt(), switchCase(), whileStmt(), doStmt()));
auto calls = expr(anyOf(callExpr(), cxxMemberCallExpr(), cxxOperatorCallExpr()));
auto arth = expr(anyOf(unaryOperator(), binaryOperator(), calls));

auto nonSimpleExpr = expr(allOf(hasAncestor(expr(allOf(calls, hasAncestor(rootStmt.bind("stmt"))))), arth));

auto paramOfCall = callExpr(forEachArgumentWithParam(nonSimpleExpr.bind("expression"), parmVarDecl(hasType(realFloatingPointType()))));
// auto paramOfCall = expr(allOf(nonSimpleExpr, hasType(realFloatingPointType()), hasAncestor(callExpr()))).bind("expression");
//auto paramOfMemberCall = cxxMemberCallExpr(forEachArgumentWithParam(nonSimpleExpr.bind("expression"), parmVarDecl(hasType(realFloatingPointType()))));
//auto paramOfOperatorCall = cxxOperatorCallExpr(forEachArgumentWithParam(nonSimpleExpr.bind("expression"), parmVarDecl(hasType(realFloatingPointType()))));

auto RetStmtPat = returnStmt(hasReturnValue(expr(allOf(hasType(realFloatingPointType()), arth)).bind("expression"))).bind("stmt");

//returnStmt(hasReturnValue(expr(allOf(hasType(realFloatingPointType()), nonSimpleExpr)).bind("expression"))).bind("stmt")

auto target = stmt(isExpansionInMainFile(), eachOf(paramOfCall, RetStmtPat, callFpFunc));

class ParamPatHandler : public MatchHandler
{
public:
    virtual void onEndOfTranslationUnit()
    {
        ExtractionPrinterHelper helper;
        auto exts = extractions;

        std::ostringstream out;
        const ExpressionExtractionRequest *lastExt = NULL;

        ExpressionExtractionRequest::sort(exts);

        auto unique = std::unique(exts.begin(), exts.end());
        exts.erase(unique, exts.end());

        auto extIt = exts.begin();
        std::string filename;
        while (lastExt != NULL || extIt != exts.end())
        {
            if(lastExt==NULL) {
                filename = extIt->manager->getFilename(extIt->statement->getBeginLoc()).str();
            }

            if (lastExt != NULL && (extIt == exts.end() || lastExt->statement != extIt->statement))
            {
                out.flush();
                
                std::string parmDef = out.str();
                Replacement Rep1 = ReplacementBuilder::create(*lastExt->manager, lastExt->statement->getBeginLoc(), 0, parmDef);
                addReplacement(Rep1);

                lastExt = NULL;
                out.str("");
            }
            else
            {
                std::string parTmp = randomIdentifier("paramTmp");
                out << "const double"
                    << " " << parTmp << " = " << print(extIt->expression, &helper) << ";\n";
                helper.addShortcut(extIt->expression, parTmp); // must after the above line

                if(isNonOverlappedStmt(extIt->expression)) {
                    std::string stmt_Str = print(extIt->expression, &helper);
                    Replacement Rep2 = ReplacementBuilder::create(*extIt->manager, extIt->expression, stmt_Str);
                    addReplacement(Rep2);
                }
                
                lastExt = &*extIt;
                extIt++;
            }
        }
        extractions.clear();
        nonOverlappedStmts.clear();
    }

    ParamPatHandler(std::map<std::string, Replacements> &r) : MatchHandler(r) {}

    std::vector<ExpressionExtractionRequest> extractions;
    virtual void run(const MatchFinder::MatchResult &Result)
    {
        
        const Expr *actual = Result.Nodes.getNodeAs<Expr>("expression");
        const Stmt *stmt = Result.Nodes.getNodeAs<Stmt>("stmt");
        const ParmVarDecl *formal = Result.Nodes.getNodeAs<ParmVarDecl>("formal");
        QualType type;
        if (formal == NULL)
            type = actual->getType();
        else
            type = formal->getType();
        
        if(isa<CallExpr>(actual))
        {
            auto call = (const CallExpr*)actual;
            if(!isInTargets(call->getDirectCallee(), Result.SourceManager)) // will not be translated
            {
                if(!call->getType().getTypePtrOrNull()->isRealFloatingType()) return; // do not return fp result
            }
        }

        extractions.push_back(ExpressionExtractionRequest(actual, type, stmt, Result.SourceManager));
        addAsNonOverlappedStmt(actual, extractions.back().actualExpressionRange);
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
    CommonOptionsParser Options(argc, argv, ScDebugTool);
    CodeTransformationTool tool(Options, "Pass Two: normalize call to floating point functions");

    ParamPatHandler handler(tool.GetReplacements());

    tool.add(target, handler);
    // tool.add(callFpFunc,handler);

    tool.run();

    return 0;
}