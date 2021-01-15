#include <string>
#include "util/random.h"
#include "transformer/transformer.hpp"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ScDebugTool("ScDebug Normalization Tool");

//Pass Three - assignments in conditions

auto fpInc = unaryOperator(hasAnyOperatorName("++", "--"), hasUnaryOperand(hasType(realFloatingPointType())));
auto fpAssign = binaryOperator(isAssignmentOperator(), hasType(realFloatingPointType()));
auto topFpAssign = expr(
    fpAssign, unless(hasParent(fpAssign)) //, 
    // unless(hasParent(implicitCastExpr(hasParent(fpAssign))))
    );
auto fpChange = expr(anyOf(fpInc, topFpAssign));

auto innerChange = expr(
    fpChange.bind("change"), 
    unless(hasParent(compoundStmt())),
    hasAncestor(stmt(hasParent(compoundStmt())).bind("stmt"))
);



class AssignPatHandler : public MatchHandler
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
        std::sort(exts.begin(), exts.end());

        std::ostringstream out;
        const ExpressionExtractionRequest *lastExt = NULL;

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
                std::string parTmp;
                if(isa<UnaryOperator>(extIt->expression)) {
                    UnaryOperator* unary = (UnaryOperator*)extIt->expression;
                    parTmp = randomIdentifier("incTmp");
                    out << "const " << extIt->type.getAsString()
                        << " " << parTmp << " = " << print(extIt->expression, &helper) << ";\n";
                } else { // Binary Case
                    out << print(extIt->expression, &helper) << ";\n";
                    BinaryOperator* assign = (BinaryOperator*)extIt->expression;
                    parTmp = print(assign->getLHS());
                }
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
    }

    AssignPatHandler(std::map<std::string, Replacements> &r) : MatchHandler(r) {}

    std::vector<ExpressionExtractionRequest> extractions;
    virtual void run(const MatchFinder::MatchResult &Result)
    {
        const Expr *actual = Result.Nodes.getNodeAs<Expr>("change");
        const Stmt *stmt = Result.Nodes.getNodeAs<Stmt>("stmt");
        QualType type = actual->getType();
        extractions.push_back(ExpressionExtractionRequest(actual, type, stmt, Result.SourceManager));
        addAsRoot(actual);
    }
};

int main(int argc, const char **argv)
{
    CodeTransformationTool tool(argc, argv, ScDebugTool, "Pass Tree: normalize assignments and increments");

    AssignPatHandler handler(tool.GetReplacements());

    tool.add(innerChange, handler);
    // tool.add(callFpFunc,handler);

    tool.run();

    return 0;
}