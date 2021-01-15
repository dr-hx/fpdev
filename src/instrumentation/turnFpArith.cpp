#include <string>
#include <set>
#include "util/random.h"
#include "transformer/transformer.hpp"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ScDebugTool("ScDebug Tool");

// Turn FpArith
// 1. collect local fp variables -> variables that are not passed by reference nor pointer. 
//      local variables have static mappings to reals
// 2. var ref => varMap[&address]
// 3. fp operators => real operators
// 4. structs? classes? unions? => constructors

auto fpType = realFloatingPointType(); // may be extended
auto fpVarDeclInFunc = functionDecl(forEachDescendant(varDecl(hasType(fpType)).bind("varDecl")));
auto sharedAddressVar = unaryOperator(hasOperatorName("&"), hasUnaryOperand(declRefExpr(to(fpVarDeclInFunc.bind("refVar")))));
auto referencedVar = varDecl(hasType(lValueReferenceType(pointee(fpType))), hasInitializer(declRefExpr(to(fpVarDeclInFunc.bind("refVar")))));
auto referencedParmVar = callExpr(forEachArgumentWithParam(declRefExpr(to(fpVarDeclInFunc.bind("refVar"))), parmVarDecl(hasType(fpType))));

auto retFpVarStmt = returnStmt(hasReturnValue(declRefExpr(hasType(fpType))));

auto fpAssign = binaryOperator(isAssignmentOperator(), hasType(realFloatingPointType()));
auto fpInc = unaryOperator(hasAnyOperatorName("++", "--"), hasUnaryOperand(hasType(realFloatingPointType())));
auto fpChange = expr(anyOf(fpInc, fpAssign));
auto fpFunc = functionDecl(anyOf(hasType(realFloatingPointType()), hasAnyParameter(hasType(realFloatingPointType()))));
auto callFpFunc = callExpr(callee(fpFunc));
auto fpDeclStmt = declStmt(hasSingleDecl(varDecl(hasType(fpType))));

// FIXME: handle long pointer and integer point
auto stmtToBeConverted = compoundStmt(forEach(stmt(anyOf(retFpVarStmt, fpChange, callFpFunc, fpDeclStmt)).bind("stmt")));

// local fp var = fpVarDeclInFunc - sharedAddressVar - referencedVar - referencedParmVar

class FpArithInstrumentation : public MatchHandler {
protected:
    std::set<const VarDecl*> localVar;
    std::set<const VarDecl*> sharedVar;
    const SourceManager *manager;
    Replacements *replace;
    std::string filename;
    std::vector<const Stmt*> fpStatements;
public:
    FpArithInstrumentation(std::map<std::string, Replacements> &r) : MatchHandler(r) {
        manager = NULL;
    }
    virtual void run(const MatchFinder::MatchResult &Result) {
        const VarDecl *decl = Result.Nodes.getNodeAs<VarDecl>("varDecl");
        if(decl!=NULL) {
            recordLocalFpVar(decl, Result);
        } else {
            const VarDecl *sharedVar = Result.Nodes.getNodeAs<VarDecl>("refVar");
            if(sharedVar!=NULL) {
                recordSharedFpVar(sharedVar, Result);
            } else {
                const Stmt* stmt = Result.Nodes.getNodeAs<Stmt>("stmt");
                if(stmt!=NULL) {
                    fpStatements.push_back(stmt);
                }
            }
        }
    }

    template<typename T>
    void fillReplace(const T* d,const MatchFinder::MatchResult &Result) {
        if(manager==NULL) {
            manager = Result.SourceManager;
            filename = manager->getFilename(d->getBeginLoc()).str();
            replace = &ReplaceMap[filename];
        }
    }

    void recordLocalFpVar(const VarDecl* d, const MatchFinder::MatchResult &Result) {
        localVar.insert(d);
        fillReplace(d, Result);
    }
    void recordSharedFpVar(const VarDecl* d, const MatchFinder::MatchResult &Result) {
        sharedVar.insert(d);
        fillReplace(d, Result);
    }

    virtual void onStartOfTranslationUnit() {
        manager = NULL;
        replace = NULL;
    }

    virtual void onEndOfTranslationUnit() {
        RealVarPrinterHelper helper;

        if(manager==NULL) return;
        { // add hearder
            Replacement rep = ReplacementBuilder::create(filename, 0, 0, "#include <Real.hpp> // you must put Real.hpp into a library path\n");
            llvm::Error err = replace->add(rep);
        }
        for(auto var : localVar) {
            if(sharedVar.count(var)==0) {
                Replacement rep = ReplacementBuilder::create(*manager, var->getBeginLoc(), 0, "/* local */");
                llvm::Error err = replace->add(rep);
                helper.staticVarMap[var] = "STATIC("+var->getNameAsString()+")";
            }
        }

        for(auto stmt : fpStatements) {
            if(isa<DeclStmt>(stmt)) {
                DeclStmt* decl = (DeclStmt*)stmt;
                std::string originalCode = print(stmt);
                std::string str;
                llvm::raw_string_ostream stream(str);
                auto it = decl->decl_begin();
                while(it!=decl->decl_end()) {
                    QualType* type;
                    it++;
                }
            } else {
                std::string originalCode = print(stmt);
                std::string code = print(stmt, &helper);
                Replacement App(*manager, stmt, originalCode + ";\n //" +code);
                llvm::Error err = replace->add(App);
            }
        }
    }

    struct RealVarPrinterHelper : public PrinterHelper {
        std::map<const VarDecl*,std::string> staticVarMap;

        virtual bool handledStmt(Stmt* E, raw_ostream& OS) {
            if(isa<DeclRefExpr>(E)) {
                DeclRefExpr* expr = (DeclRefExpr*)E;
                if(isa<VarDecl>(expr->getDecl())) {
                    VarDecl* varDecl = (VarDecl*)expr->getDecl();
                    auto it = staticVarMap.find(varDecl);
                    if(it!=staticVarMap.end()) {
                        OS << it->second;
                    } else {
                        OS << "DYNAMIC("<<varDecl->getNameAsString()<<")";
                    }
                    return true;
                }
            } else if(isa<UnaryOperator>(E)){
                UnaryOperator* unary = (UnaryOperator*)E;
                if(unary->getOpcode()==UnaryOperator::Opcode::UO_AddrOf) {
                    OS << print(unary);
                    return true;
                }
            }
            return false;
        }
    };
};

int main(int argc, const char **argv)
{
    CodeTransformationTool tool(argc, argv, ScDebugTool, "Instrumentation: FpArith");
    
    FpArithInstrumentation handler(tool.GetReplacements());
    
    tool.add(fpVarDeclInFunc,handler);
    tool.add(sharedAddressVar,handler);
    tool.add(referencedVar,handler);
    tool.add(referencedParmVar,handler);

    // tool.add<decltype(target), SingleStmtPatHandler>(target);

    tool.run();

    return 0;
}