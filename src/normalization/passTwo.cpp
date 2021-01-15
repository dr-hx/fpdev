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

auto fpFunc = functionDecl(anyOf(hasType(realFloatingPointType()), hasAnyParameter(hasType(realFloatingPointType()))));
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

auto target = stmt(eachOf(paramOfCall, RetStmtPat, callFpFunc));

class ParamPatHandler : public MatchHandler
{
public:
    std::set<const Stmt*> rootStmt;
    void addAsRoot(const Stmt* stmt) {
        std::vector<const Stmt*> toBeDel;
        for(auto root : rootStmt) {
            if(stmt->getSourceRange().fullyContains(root->getSourceRange())) {
                toBeDel.push_back(root);
            }
        }
        for(auto del : toBeDel) {
            rootStmt.erase(del);
        }
        rootStmt.insert(stmt);
    }
    bool isRoot(const Stmt* stmt) {
        return rootStmt.count(stmt)!=0;
    }

    virtual void onEndOfTranslationUnit()
    {
        ExtractionPrinterHelper helper;
        auto exts = extractions;

        std::ostringstream out;
        const ExpressionExtractionRequest *lastExt = NULL;

        std::sort(exts.begin(), exts.end());
        auto unique = std::unique(exts.begin(), exts.end());
        exts.erase(unique, exts.end());

        auto extIt = exts.begin();
        std::string filename;
        Replacements *Replace;
        while (lastExt != NULL || extIt != exts.end())
        {
            if(lastExt==NULL) {
                filename = extIt->manager->getFilename(extIt->statement->getBeginLoc()).str();
                Replace = &ReplaceMap[filename];
            }

            if (lastExt != NULL && (extIt == exts.end() || lastExt->statement != extIt->statement))
            {
                out.flush();
                
                std::string parmDef = out.str();
                Replacement Rep1(*lastExt->manager, lastExt->statement->getBeginLoc(), 0, parmDef);
                llvm::Error err1 = Replace->add(Rep1);

                lastExt = NULL;
                out.str("");
            }
            else
            {
                std::string parTmp = randomIdentifier("paramTmp");
                out << "const " << extIt->type.getAsString()
                    << " " << parTmp << " = " << print(extIt->expression, &helper) << ";\n";
                helper.addShortcut(extIt->expression, parTmp); // must after the above line

                if(isRoot(extIt->expression)) {
                    std::string stmt_Str = print(extIt->expression, &helper);
                    Replacement Rep2(*extIt->manager, extIt->expression, stmt_Str);
                    llvm::Error err2 = Replace->add(Rep2);
                }
                
                lastExt = &*extIt;
                extIt++;
            }
        }
        extractions.clear();
        rootStmt.clear();
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
        // actual->dumpColor();
        extractions.push_back(ExpressionExtractionRequest(actual, type, stmt, Result.SourceManager));
        addAsRoot(actual);
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
    CodeTransformationTool tool(argc, argv, ScDebugTool, "Pass Two: normalize call to floating point functions");

    ParamPatHandler handler(tool.GetReplacements());

    tool.add(target, handler);
    // tool.add(callFpFunc,handler);

    tool.run();

    return 0;
}