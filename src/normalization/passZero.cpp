#include <string>
#include <set>
#include "util/random.h"
#include "transformer/transformer.hpp"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ScDebugTool("ScDebug Normalization Tool");

// Pass Zero - turn empty or single statement into compound statement
auto fpFunc = functionDecl(anyOf(hasType(realFloatingPointType()), hasAnyParameter(hasType(realFloatingPointType()))));

// auto singleStmt = stmt(
//     // unless(expr()), 
//     unless(compoundStmt()),
//     unless(declStmt()),
//     unless(hasParent(compoundStmt())),
//     unless(hasParent(expr())), 
//     anyOf(nullStmt(), unless(compoundStmt()))).bind("stmt");

auto hangThen = ifStmt(hasThen(stmt(unless(compoundStmt())).bind("stmt")));
auto elseThen = ifStmt(hasElse(stmt(unless(compoundStmt())).bind("stmt")));
auto hangForBody = forStmt(hasBody(stmt(unless(compoundStmt())).bind("stmt")));
auto hangWhileBody = whileStmt(hasBody(stmt(unless(compoundStmt())).bind("stmt")));
auto hangDoBody = doStmt(hasBody(stmt(unless(compoundStmt())).bind("stmt")));

auto hangStmt = stmt(eachOf(hangThen,elseThen,hangForBody,hangWhileBody,hangDoBody));

auto fpDecl = declStmt(has(varDecl(hasType(realFloatingPointType()))));
auto IfStmtPat = ifStmt(anyOf(hasConditionVariableStatement(fpDecl), hasInitStatement(fpDecl))).bind("stmt");
auto ForStmtPat = forStmt(hasLoopInit(fpDecl));
// in switch condition, we cannot define a fp variable
// auto SwitchStmtPat = switchStmt(hasCondition(hasDescendant(callExpr(callee(fpFunc)))));
auto WhileStmtPat = whileStmt(has(fpDecl.bind("condVar")));

auto stmtWithScope = stmt(anyOf(IfStmtPat,ForStmtPat,WhileStmtPat)).bind("stmt");


auto target = stmt(eachOf(stmtWithScope, hangStmt));

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
                }
                else {
                    OS << "{";
                    // OS << "//" << std::string(E->getStmtClassName()) <<"\n";
                    E->printPretty(OS, this, PrintingPolicy(LangOptions()));
                    if(isa<Expr>(E)) {
                        OS << ";";
                    }
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
            // if(isa<Expr>(s)) {
            //     Replacement Rep(*Manager, s, stream.str());
            //     llvm::Error err = Replace->add(Rep);
            //     Replacement Rep2(*Manager, s->getEndLoc().getLocWithOffset(1), 1, ""); // this intends to clear ";"
            //     llvm::Error err2 = Replace->add(Rep2);
            // } else {
            Replacement Rep(*Manager, s, stream.str());
            llvm::Error err = Replace->add(Rep);
            // }
        }
        rootStmt.clear();
        stmtVector.clear();
        Manager = NULL;
    }
};

int main(int argc, const char **argv)
{
    CodeTransformationTool tool(argc, argv, ScDebugTool, "Pass Zero: turn empty and hanging statements");
    tool.add<decltype(target), SingleStmtPatHandler>(target);

    tool.run();

    return 0;
}