#include <string>
#include "util/random.h"
#include "transformer/transformer.hpp"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ScDebugTool("ScDebug Normalization Tool");

// Pass One - normalize declarations and control structures
// 1. DeclStmt in if condition, loop init, switch condition should be moved out
// 2. Flatten multiple decls

// IfStmt 
// 1 condition var stmt, & init stmt -> move out
// -- 2 fp condition -> out? NO NEED
auto fpDecl = declStmt(has(varDecl(hasType(realFloatingPointType()))));

auto IfStmtCondPat = ifStmt(optionally(hasConditionVariableStatement(fpDecl.bind("decl"))), optionally(hasInitStatement(fpDecl.bind("init")))).bind("parent");



// ForStmt/while
// 1. fp loopInit decl -> out
// 2. fp loopInit expr -> out
// -- 3. fp condition -> in NO NEED
// 4. fp inc -> in
auto ForStmtPat_DeclInInit = forStmt(hasLoopInit(declStmt(hasDescendant(varDecl(hasType(realFloatingPointType())))).bind("decl"))).bind("forStmt");
auto ForStmtPat_FpInit = forStmt(hasLoopInit(expr(findAll(binaryOperator(isAssignmentOperator(), hasType(realFloatingPointType())))).bind("decl"))).bind("forStmt");

auto ForStmtPat = forStmt(
    
).bind("parent");

class IfStmtDeclPatHandler : public MatchHandler
{
public:
    IfStmtDeclPatHandler(std::map<std::string, Replacements> &r) : MatchHandler(r) {}
    virtual void run(const MatchFinder::MatchResult &Result) {
        const IfStmt* stmt = Result.Nodes.getNodeAs<IfStmt>("parent");
        const DeclStmt* cvs = Result.Nodes.getNodeAs<DeclStmt>("decl");
        const DeclStmt* lis = Result.Nodes.getNodeAs<DeclStmt>("init");
        if(cvs==NULL && lis == NULL) return;
        
        auto filename = Result.SourceManager->getFilename(stmt->getBeginLoc()).str();
        Replacements &Replace = ReplaceMap[filename];

        std::ostringstream out;
        out << "{" << std::endl;
        if(lis!=NULL) {
            out << flatten_declStmt(lis) << std::endl;
        }
        if(cvs!=NULL) {
            out << flatten_declStmt(cvs) << std::endl;
        }
        Replacement RepCond1(*(Result.SourceManager), stmt->getBeginLoc(), 0, out.str());
        llvm::Error err1 = Replace.add(RepCond1);

        if(lis!=NULL) {
            Replacement RepCond2(*(Result.SourceManager), lis, "");
            llvm::Error err2 = Replace.add(RepCond2);
        }
        if(cvs!=NULL) {
            VarDecl *var = (VarDecl *)cvs->getSingleDecl();
            Replacement RepCond2(*(Result.SourceManager), cvs, var->getNameAsString());
            llvm::Error err2 = Replace.add(RepCond2);
        }
        Replacement RepCond3(*(Result.SourceManager), stmt->getEndLoc(), 0, "}");
        llvm::Error err3 = Replace.add(RepCond3);
    }
};

// the following pattern cannot handle the code like "if(...) int x=1,y=2;", since the code is meaningless
// the following pattern and above three patterns are disjoint
auto MultiDeclPat_DeclInBlock = declStmt(allOf(unless(hasSingleDecl(anything())), hasParent(compoundStmt()))).bind("decl");

class DeclPatHandler : public MatchHandler
{
public:
    DeclPatHandler(std::map<std::string, Replacements> &r) : MatchHandler(r) {}

    virtual void run(const MatchFinder::MatchResult &Result)
    {
        const DeclStmt *decl = Result.Nodes.getNodeAs<clang::DeclStmt>("decl");
        if (decl != NULL)
        {
            auto filename = Result.SourceManager->getFilename(decl->getBeginLoc()).str();
            Replacements &Replace = ReplaceMap[filename];
            std::ostringstream out;

            const ForStmt *forS = Result.Nodes.getNodeAs<clang::ForStmt>("forStmt");
            if (forS != NULL) // decl is not used as a condition
            {
                out << "{"
                    << std::endl
                    << flatten_declStmt(decl)
                    << std::endl;

                Replacement RepCond1(*(Result.SourceManager), forS->getBeginLoc(), 0, out.str());
                Replacement RepCond2(*(Result.SourceManager), decl, ";");
                Replacement RepCond3(*(Result.SourceManager), forS->getEndLoc(), 0, "\n}\n");
                llvm::Error err1 = Replace.add(RepCond1);
                llvm::Error err2 = Replace.add(RepCond2);
                llvm::Error err3 = Replace.add(RepCond3);
            }
            else 
            {
                const Stmt *parent = Result.Nodes.getNodeAs<clang::Stmt>("parent");
                if(parent==NULL) { // multiple decls in compound stmt
                    Replacement RepCond1(*(Result.SourceManager), decl, print(decl));
                    llvm::Error err1 = Replace.add(RepCond1);
                } else { // decl is used as a condition, it must be a single decl
                    VarDecl *var = (VarDecl *)decl->getSingleDecl();
                    out << "{"
                        << std::endl
                        << flatten_declStmt(decl)
                        << std::endl;

                    Replacement RepCond1(*(Result.SourceManager), parent->getBeginLoc(), 0, out.str());
                    Replacement RepCond2(*(Result.SourceManager), decl, var->getNameAsString());
                    Replacement RepCond3(*(Result.SourceManager), parent->getEndLoc(), 0, "\n}\n");

                    llvm::Error err1 = Replace.add(RepCond1);
                    llvm::Error err2 = Replace.add(RepCond2);
                    llvm::Error err3 = Replace.add(RepCond3);
                }
            }
        }
        else // if(double d=0;d>0) ...
        {
            decl = Result.Nodes.getNodeAs<clang::DeclStmt>("init");
            if (decl != NULL)
            {
                auto filename = Result.SourceManager->getFilename(decl->getBeginLoc()).str();
                Replacements &Replace = ReplaceMap[filename];
                std::ostringstream out;
                const Stmt *parent = Result.Nodes.getNodeAs<clang::Stmt>("parent");
                out << "{"
                    << std::endl
                    << flatten_declStmt(decl)
                    << std::endl;

                Replacement RepCond1(*(Result.SourceManager), parent->getBeginLoc(), 0, out.str());
                Replacement RepCond2(*(Result.SourceManager), decl, "");
                Replacement RepCond3(*(Result.SourceManager), parent->getEndLoc(), 0, "\n}\n");

                llvm::Error err1 = Replace.add(RepCond1);
                llvm::Error err2 = Replace.add(RepCond2);
                llvm::Error err3 = Replace.add(RepCond3);
            }
        }
    }
};

int main(int argc, const char **argv)
{
    CodeTransformationTool tool(argc, argv, ScDebugTool);
    
    tool.add<decltype(IfStmtCondPat), IfStmtDeclPatHandler>(IfStmtCondPat);

    // tool.add<decltype(ForStmtPat_DeclInInit), DeclPatHandler>(ForStmtPat_DeclInInit);
    // tool.add<decltype(MultiDeclPat_DeclInBlock), DeclPatHandler>(MultiDeclPat_DeclInBlock);

    tool.run();

    return 0;
}