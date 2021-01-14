#include <string>
#include <set>
#include "util/random.h"
#include "transformer/transformer.hpp"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ScDebugTool("ScDebug Normalization Tool");

// Pass Zero - NullStmt & EmptyChild
// 1. DeclStmt in if condition, loop init, switch condition should be moved out
// 2. Flatten multiple decls
auto fpFunc = functionDecl(anyOf(hasType(realFloatingPointType()), hasAnyParameter(hasType(realFloatingPointType()))));

auto singleStmt = stmt(
    unless(expr()), 
    unless(declStmt()),
    unless(hasParent(compoundStmt())), 
    anyOf(nullStmt(), unless(compoundStmt()))).bind("stmt");

auto fpDecl = declStmt(has(varDecl(hasType(realFloatingPointType()))));
auto IfStmtPat = ifStmt(anyOf(hasConditionVariableStatement(fpDecl), hasInitStatement(fpDecl))).bind("stmt");
auto ForStmtPat = forStmt(hasLoopInit(fpDecl));
// in switch condition, we cannot define a fp variable
// auto SwitchStmtPat = switchStmt(hasCondition(hasDescendant(callExpr(callee(fpFunc)))));
auto WhileStmtPat = whileStmt(has(fpDecl.bind("condVar")));

auto stmtWithScope = stmt(anyOf(IfStmtPat,ForStmtPat,WhileStmtPat)).bind("stmt");

auto target = stmt(anyOf(singleStmt, stmtWithScope));

class SingleStmtPatHandler : public MatchHandler
{
public:
    SingleStmtPatHandler(std::map<std::string, Replacements> &r) : MatchHandler(r) {}
    std::set<const Stmt*> stmtVector;
    virtual void run(const MatchFinder::MatchResult &Result)
    {
        Manager = Result.SourceManager;
        const Stmt *stmt = Result.Nodes.getNodeAs<Stmt>("stmt");

        if(isa<WhileStmt>(stmt)) {
            WhileStmt* ws = (WhileStmt*)stmt;
            if(ws->getConditionVariableDeclStmt()!=NULL) {
                const DeclStmt* decl = ws->getConditionVariableDeclStmt();
                const DeclStmt* condVar = Result.Nodes.getNodeAs<DeclStmt>("condVar");
                if(decl==condVar) { // inexact if the matching is unordered
                    stmtVector.insert(stmt);
                    addAsRoot(stmt);
                }
            }
        } else {
            stmtVector.insert(stmt);
            addAsRoot(stmt);
        }

    }
    std::vector<const Stmt*> rootStmt;
    void addAsRoot(const Stmt* stmt) {
        std::vector<const Stmt*> toBeDel;
        auto stmtIt = rootStmt.begin();
        while(stmtIt!=rootStmt.end()) {
            if(stmt->getSourceRange().fullyContains((*stmtIt)->getSourceRange())) {
                toBeDel.push_back(*stmtIt);
            }
            stmtIt++;
        }
        rootStmt.erase(toBeDel.begin(), toBeDel.end());
        rootStmt.push_back(stmt);
    }
    class ParnStmtHelper : public PrinterHelper
    {
    public:
        ParnStmtHelper(std::set<const Stmt *> &sv) : stmtVector(sv) {}
        std::set<const Stmt *> &stmtVector;
        virtual bool handledStmt(Stmt *E, raw_ostream &OS)
        {
            if(stmtVector.count(E)!=0) {
                stmtVector.erase(E);
                if(isa<NullStmt>(E)) {
                    OS << "{}";
                } else {
                    OS << "{";
                    E->printPretty(OS, this, PrintingPolicy(LangOptions()));
                    OS << "}";
                }
                return true;
            }
            return false;
        }
    };
    SourceManager *Manager;

    virtual void onEndOfTranslationUnit() {
        Replacements *Replace = NULL;
        ParnStmtHelper helper(stmtVector);
        for(auto s : rootStmt) {
            if(Replace==NULL) {
                std::string fn = (Manager->getFilename(s->getBeginLoc()).str());
                Replace = &ReplaceMap[fn];
            }
            std::string str;
            llvm::raw_string_ostream stream(str);
            s->printPretty(stream, &helper, PrintingPolicy(LangOptions()));
            Replacement Rep(*Manager, s, stream.str());
            llvm::Error err = Replace->add(Rep);
        }
        rootStmt.clear();
        stmtVector.clear();
        Manager = NULL;
    }
};

int main(int argc, const char **argv)
{
    CodeTransformationTool tool(argc, argv, ScDebugTool);

    tool.add<decltype(target), SingleStmtPatHandler>(target);

    tool.run();

    return 0;
}