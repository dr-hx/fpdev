#include <string>
#include "../util/random.h"
#include "../transformer/transformer.hpp"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ScDebugTool("ScDebug Tool");

// Pass One - normalize declarations and control structures
// 1. DeclStmt in if condition, loop init should be moved out
// 2. Flatten multiple decls

// IfStmt
// 1 condition var stmt, & init stmt -> move out
// -- 2 fp condition -> out? NO NEED because it is the base case
auto fpDecl = declStmt(has(varDecl(hasType(realFloatingPointType()))));

//hasUnaryOperand
auto fpInc = unaryOperator(hasAnyOperatorName("++", "--"), hasUnaryOperand(hasType(realFloatingPointType())));
auto fpAssign = binaryOperator(isAssignmentOperator(), hasType(realFloatingPointType()));
auto fpChange = expr(anyOf(fpInc, fpAssign));

auto IfStmtPat = ifStmt(isExpansionInMainFile(), 
    optionally(hasConditionVariableStatement(fpDecl.bind("decl"))), 
    optionally(hasInitStatement(fpDecl.bind("init")))
    ).bind("parent");

// ForStmt/while
// 1. fp loopInit decl -> out
// 2. fp loopInit expr -> out
// 3. fp change condition to a ifstmt for further rewriting
// 4. fp inc -> in
// auto ForStmtPat_DeclInInit = forStmt(optionally(hasLoopInit(declStmt(hasDescendant(varDecl(hasType(realFloatingPointType())))).bind("decl"))), optionally(hasLoopInit(expr().bind("init")))).bind("forStmt");
// auto ForStmtPat_FpInit = forStmt(hasLoopInit(expr(findAll(binaryOperator(isAssignmentOperator(), hasType(realFloatingPointType())))).bind("decl"))).bind("forStmt");

class IfStmtPatHandler : public MatchHandler
{
public:
    IfStmtPatHandler(std::map<std::string, Replacements> &r) : MatchHandler(r) {}
    virtual void run(const MatchFinder::MatchResult &Result)
    {
        
        const Stmt *stmt = Result.Nodes.getNodeAs<Stmt>("parent");
        const DeclStmt *cvs = Result.Nodes.getNodeAs<DeclStmt>("decl");
        const DeclStmt *lis = Result.Nodes.getNodeAs<DeclStmt>("init");
        if (cvs == NULL && lis == NULL)
            return;

        std::ostringstream out;
        if (lis != NULL)
        {
            out << flatten_declStmt(lis) << std::endl;
        }
        if (cvs != NULL)
        {
            out << flatten_declStmt(cvs) << std::endl;
        }
        Replacement RepCond1 = ReplacementBuilder::create(*(Result.SourceManager), stmt->getBeginLoc(), 0, out.str());
        addReplacement(RepCond1);

        if (lis != NULL)
        {
            Replacement RepCond2 = ReplacementBuilder::create(*(Result.SourceManager), lis, "");
            addReplacement(RepCond2);
        }
        if (cvs != NULL)
        {
            VarDecl *var = (VarDecl *)cvs->getSingleDecl();
            Replacement RepCond2 = ReplacementBuilder::create(*(Result.SourceManager), cvs, var->getNameAsString());
            addReplacement(RepCond2);
        }
    }
};

auto ForStmtPat = forStmt(isExpansionInMainFile(), 
    optionally(hasLoopInit(fpDecl.bind("decl"))),
    optionally(hasLoopInit(expr(anyOf(fpChange, hasDescendant(fpChange))).bind("init"))),
    optionally(hasCondition(expr(anyOf(fpChange, hasDescendant(fpChange))).bind("cond"))),
    optionally(hasIncrement(expr(anyOf(fpChange, hasDescendant(fpChange))).bind("inc")))
).bind("parent");

class ForStmtPatHandler : public MatchHandler
{
public:
    ForStmtPatHandler(std::map<std::string, Replacements> &r) : MatchHandler(r) {}
    virtual void run(const MatchFinder::MatchResult &Result)
    {
        
        const ForStmt *stmt = Result.Nodes.getNodeAs<ForStmt>("parent");
        const DeclStmt *decl = Result.Nodes.getNodeAs<DeclStmt>("decl");
        const Expr *init = Result.Nodes.getNodeAs<Expr>("init");
        const Expr *cond = Result.Nodes.getNodeAs<Expr>("cond");
        const Expr *inc = Result.Nodes.getNodeAs<Expr>("inc");

        if (decl == NULL && init == NULL && cond==NULL && inc==NULL)
            return;

        std::ostringstream prefixOut;

        // handle init
        if (decl != NULL)
        {
            prefixOut << flatten_declStmt(decl) << std::endl;
        }
        if (init != NULL) // a=b or a=b=c or a=b, c=d=e, ...
        {
            prefixOut << print(init) << ";" << std::endl; // it is better to extract fp changes out only
        }
        Replacement RepPrefix = ReplacementBuilder::create(*(Result.SourceManager), stmt->getBeginLoc(), 0, prefixOut.str());
        addReplacement(RepPrefix);

        if (decl != NULL)
        {
            Replacement RepInit = ReplacementBuilder::create(*(Result.SourceManager), decl, ";");
            addReplacement(RepInit);
        }
        if (init != NULL)
        {
            Replacement RepInit = ReplacementBuilder::create(*(Result.SourceManager), init, "");
            addReplacement(RepInit);
        }

        std::ostringstream bodyOut;

        if (cond != NULL)
        {
            Replacement RepCond = ReplacementBuilder::create(*(Result.SourceManager), cond, "");
            addReplacement(RepCond);
            bodyOut << "{\n";
            bodyOut << "if(!(" << print(cond) << ")) {break;}\n";
            // because body must be something like {...}, we replace the open bracket
            Replacement RepBodyF = ReplacementBuilder::create(*(Result.SourceManager), stmt->getBody()->getBeginLoc(), 1, bodyOut.str());
            addReplacement(RepBodyF);
        }
        bodyOut.str("");
        if (inc != NULL)
        {
            Replacement RepInc = ReplacementBuilder::create(*(Result.SourceManager), inc, "");
            addReplacement(RepInc);
            bodyOut << print(inc) << ";\n}\n";
            Replacement RepBodyB = ReplacementBuilder::create(*(Result.SourceManager), stmt->getBody()->getEndLoc(), 1, bodyOut.str());
            addReplacement(RepBodyB);
        }
    }
};

// I don't know the meaning of this statement
auto WhileStmtPat = whileStmt(isExpansionInMainFile(), 
    optionally(has(fpDecl.bind("decl"))),
    optionally(hasCondition(expr(anyOf(fpChange, hasDescendant(fpChange))).bind("cond")))
).bind("parent");

class WhileStmtPatHandler : public MatchHandler
{
public:
    WhileStmtPatHandler(std::map<std::string, Replacements> &r) : MatchHandler(r) {}
    virtual void run(const MatchFinder::MatchResult &Result)
    {
        
        const WhileStmt *stmt = Result.Nodes.getNodeAs<WhileStmt>("parent");
        const DeclStmt *decl = Result.Nodes.getNodeAs<DeclStmt>("decl");
        const Expr *cond = Result.Nodes.getNodeAs<Expr>("cond");

        if (decl == NULL && cond==NULL)
            return;

        std::ostringstream prefixOut;

        // handle init
        if (decl != NULL)
        {
            prefixOut << "// semantics of this while may be changed !!\n";
            prefixOut << flatten_declStmt(decl) << std::endl;
            Replacement RepPrefix = ReplacementBuilder::create(*(Result.SourceManager), stmt->getBeginLoc(), 0, prefixOut.str());
            addReplacement(RepPrefix);

            Replacement RepInit = ReplacementBuilder::create(*(Result.SourceManager), decl, "");
            addReplacement(RepInit);
        }

        std::ostringstream bodyOut;

        if (cond != NULL)
        { 
            Replacement RepCond = ReplacementBuilder::create(*(Result.SourceManager), cond, "true");
            addReplacement(RepCond);
            bodyOut << "{\n";
            bodyOut << "if(!(" << print(cond) << ")) {break;}\n";
            // because body must be something like {...}, we replace the open bracket
            Replacement RepBodyF = ReplacementBuilder::create(*(Result.SourceManager), stmt->getBody()->getBeginLoc(), 1, bodyOut.str());
            addReplacement(RepBodyF);
        }
    }
};

auto DoStmtPat = doStmt(isExpansionInMainFile(), 
    hasCondition(expr(anyOf(fpChange, hasDescendant(fpChange))).bind("cond"))
).bind("parent");

class DoStmtPatHandler : public MatchHandler
{
public:
    DoStmtPatHandler(std::map<std::string, Replacements> &r) : MatchHandler(r) {}
    virtual void run(const MatchFinder::MatchResult &Result)
    {
        
        const DoStmt *stmt = Result.Nodes.getNodeAs<DoStmt>("parent");
        const Expr *cond = Result.Nodes.getNodeAs<Expr>("cond");

        std::ostringstream bodyOut;

        Replacement RepCond = ReplacementBuilder::create(*(Result.SourceManager), cond, "true");
        addReplacement(RepCond);
        bodyOut << "if(!(" << print(cond) << ")) {break;}\n";
        bodyOut << "}";
        // because body must be something like {...}, we replace the open bracket
        Replacement RepBodyF = ReplacementBuilder::create(*(Result.SourceManager), stmt->getBody()->getEndLoc(), 1, bodyOut.str());
        addReplacement(RepBodyF);
    }
};

// auto SwitchStmtPat = switchStmt(
//     optionally(hasConditionVariableStatement<SwitchStmt>(fpDecl.bind("decl"))), 
//     optionally(hasInitStatement(fpDecl.bind("init")))).bind("parent");

// auto conditionStmtPat = stmt(anyOf(IfStmtPat, SwitchStmtPat));

// the following pattern cannot handle the code like "if(...) int x=1,y=2;", since the code is meaningless
// the following pattern and above three patterns are disjoint
auto MultiDeclPat_DeclInBlock = declStmt(allOf(unless(hasSingleDecl(anything())), hasParent(compoundStmt()))).bind("decl");


int main(int argc, const char **argv)
{
    CommonOptionsParser Options(argc, argv, ScDebugTool);
    CodeTransformationTool tool(Options, "Pass One: normalize control structures");
    tool.add<decltype(IfStmtPat), IfStmtPatHandler>(IfStmtPat);
    // tool.add<decltype(SwitchStmtPat), IfStmtPatHandler>(SwitchStmtPat);
    tool.add<decltype(ForStmtPat), ForStmtPatHandler>(ForStmtPat);
    tool.add<decltype(WhileStmtPat), WhileStmtPatHandler>(WhileStmtPat);
    tool.add<decltype(DoStmtPat), DoStmtPatHandler>(DoStmtPat);
    // tool.add<decltype(ForStmtPat_DeclInInit), DeclPatHandler>(ForStmtPat_DeclInInit);
    // tool.add<decltype(MultiDeclPat_DeclInBlock), DeclPatHandler>(MultiDeclPat_DeclInBlock);

    tool.run();

    return 0;
}