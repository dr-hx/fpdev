#include <string>
#include <set>
#include "util/random.h"
#include "transformer/transformer.hpp"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ScDebugTool("ScDebug Tool");

#define PREFIX_LOCAL "__LOCAL_"
#define PREFIX_SHARED "__SHARED_"
#define PREFIX_GLOBAL "__GLOBAL_"
// Turn FpArith
// 1. collect local fp variables -> variables that are not passed by reference nor pointer.
//      local variables have static mappings to reals
// 2. var ref => varMap[&address]
// 3. fp operators => real operators
// 4. structs? classes? unions? => constructors

auto fpType = realFloatingPointType(); // may be extended
auto fpVarDecl = varDecl(hasType(fpType), anyOf(hasLocalStorage(), isStaticLocal()));
auto fpVarDeclInFunc = functionDecl(forEachDescendant(fpVarDecl.bind("varDecl")));

auto sharedAddressVar = unaryOperator(hasOperatorName("&"), hasUnaryOperand(declRefExpr(to(fpVarDecl.bind("refVar")))));
auto referencedVar = varDecl(hasType(lValueReferenceType(pointee(fpType))), hasInitializer(declRefExpr(to(fpVarDecl.bind("refVar")))));

auto fpRefType = qualType(anyOf(lValueReferenceType(pointee(fpType)), pointerType(pointee(fpType))));
auto referencedParmVar = callExpr(forEachArgumentWithParam(declRefExpr(to(fpVarDecl.bind("refVar"))), parmVarDecl(hasType(fpRefType))));

auto retFpVarStmt = returnStmt(hasReturnValue(declRefExpr(hasType(fpType))));

auto fpAssign = binaryOperator(isAssignmentOperator(), hasType(fpType));
auto fpInc = unaryOperator(hasAnyOperatorName("++", "--"), hasUnaryOperand(hasType(fpType)));
auto fpChange = expr(anyOf(fpInc, fpAssign));
auto fpFunc = functionDecl(anyOf(returns(fpType), hasAnyParameter(hasType(fpType))));
auto callFpFunc = callExpr(callee(fpFunc));
auto fpDeclStmt = declStmt(has(varDecl(hasType(fpType))));

// FIXME: handle long pointer and integer point
// handle default branch in switch
auto stmtToBeConverted = compoundStmt(forEach(stmt(anyOf(retFpVarStmt, fpChange, callFpFunc, fpDeclStmt)).bind("stmt")));

auto fpParm = parmVarDecl(hasType(fpType)).bind("parm");
// local fp var = fpVarDeclInFunc - sharedAddressVar - referencedVar - referencedParmVar

// a stmt is closed, if it is a return or a closed ifStmt/compundStmt/switchStmt
// a compound stmt is closed, if it contains a closed child stmt
// a ifStmt is closed if both then and else are closed
// a switchStmt is generally equal to compound stmt if it is normalized
// break is a semi-closed stmt that closes the nearest switch and for/while/do

auto funcDecl = functionDecl().bind("func-scope");
auto scope = stmt(anyOf(compoundStmt(), ifStmt(), forStmt(), whileStmt(), doStmt(), switchStmt())).bind("stmt-scope");
auto outStmt = stmt(anyOf(breakStmt(), returnStmt())).bind("out-scope");

// root: parent = NULL, statement = NULL
// function decl: parent != NULL, statement = NULL

struct Scope;
struct ScopeBreak;

struct VarDeclInFile
{
    const VarDecl *decl;
    SourceRange rangeInFile;
    Scope *declaredScope;

    VarDeclInFile(const VarDecl *f, const SourceManager *m) : decl(f),
                                                              rangeInFile(m->getFileLoc(f->getBeginLoc()), m->getFileLoc(f->getEndLoc()))
    {
    }
};

struct ScopeItem : public StmtInFile
{
    ScopeItem *parent;
    ScopeItem() : StmtInFile(NULL, SourceRange()), parent(NULL) {}
    ScopeItem(const FunctionDecl *f, const SourceManager *m) : StmtInFile(NULL, SourceRange(m->getFileLoc(f->getBeginLoc()), m->getFileLoc(f->getEndLoc()))),
                                                               parent(NULL) {}
    ScopeItem(const Stmt *s, const SourceManager *m) : StmtInFile(s, m), parent(NULL) {}

    virtual bool insert(ScopeItem *scope) { 
        return false; 
    }

    virtual bool isRoot() {return false;}

    bool cover(const StmtInFile &r)
    {
        if (isRoot())
            return true; // root covers all
        return StmtInFile::cover(r);
    }
    bool cover(const VarDeclInFile &r)
    {
        if (isRoot())
            return true; // root covers all
        return rangeInFile.fullyContains(r.rangeInFile);
    }

    // bool isRoot()
    // {
    //     return parent == NULL && statement == NULL;
    // }
    bool isFuncDecl()
    {
        return parent != NULL && statement == NULL;
    }
    virtual bool isScope()
    {
        return false;
    }

    virtual void dump(const SourceManager& Manager, unsigned int indent) {
        if(parent==NULL) {
            llvm::outs() << "root\n";
        } else {
            if(statement==NULL) {
                llvm::outs().indent(indent) << "function decl at ";
            } else {
                llvm::outs().indent(indent) << statement->getStmtClassName() << " at ";
            }
                rangeInFile.print(llvm::outs(), Manager);
                llvm::outs()<<"\n";
        }
    }
};

struct ScopeBreak : public ScopeItem
{
    bool isSemiBreak;
    ScopeBreak(const Stmt *s, const SourceManager *m) : ScopeItem(s, m)
    {
        if (isa<BreakStmt>(s))
            isSemiBreak = true;
        else
            isSemiBreak = false;
    }
};
struct Scope : public ScopeItem
{
    bool reactToSemiBreak;
    std::vector<VarDeclInFile *> varDecls;
    std::vector<ScopeItem *> subItems;

    Scope() : ScopeItem()
    {
        reactToSemiBreak = false;
    }
    Scope(const FunctionDecl *f, const SourceManager *m) : ScopeItem(f, m)
    {
        reactToSemiBreak = false;
    }
    Scope(const Stmt *s, const SourceManager *m) : ScopeItem(s, m)
    {
        if (isa<CompoundStmt>(s))
            reactToSemiBreak = false;
        else if (isa<IfStmt>(s))
            reactToSemiBreak = false;
        else
            reactToSemiBreak = true;
    }

    ~Scope()
    {
        for (auto s : subItems)
        {
            delete s;
        }
        for (auto v : varDecls)
        {
            delete v;
        }
        subItems.clear();
    }

    virtual bool isScope()
    {
        return true;
    }

    // addDecl must be called after the scope tree is built
    bool addDecl(VarDeclInFile *decl)
    {
        if (!isRoot() && !cover(*decl))
            return false;

        for (auto sub : subItems)
        {
            if (sub->isScope())
            {
                Scope *sc = (Scope *)sub;
                if (sc->addDecl(decl))
                    return true;
            }
        }

        if (isFuncDecl())
        {
            Scope *sc = (Scope *)subItems[0];
            sc->varDecls.push_back(decl);
            decl->declaredScope = sc;
        }
        else
        {
            if(isRoot()) {
                decl->decl->print(llvm::outs());
                llvm::outs()<<"\n";
            }
            varDecls.push_back(decl);
            decl->declaredScope = this;
        }

        // varDeclMap[decl->decl] = decl;

        return true;
    }

    virtual bool insert(ScopeItem *scope)
    {
        if (!isRoot() && !cover(*scope)) {
            return false;
        }

        int size = subItems.size();
        for(int i=0;i<size;) {
            if(subItems[i]->insert(scope)) {
                return true;
            }
            if(scope->cover(*subItems[i])) {
                if(size-1>i) {
                    ScopeItem* tmp = subItems[size-1];
                    subItems[size-1] = subItems[i];
                    subItems[i] = tmp;
                }
                size--;
            } else i++;
        }

        for(int i=subItems.size();i>size;i--) {
            assert(scope->insert(subItems.back())!=false);
            subItems.pop_back();
        }
        // add as child
        subItems.push_back(scope);
        scope->parent = this;

        // if(scope->statement!=NULL) {
        //     stmtMap[scope->statement] = scope;
        // }
        return true;
    }

    bool check(const SourceManager& Manager) {
        if(parent==NULL) {
            if(varDecls.size()!=0)
                {
                    llvm::outs() << "root has var decls\n";
                    for(auto v : varDecls) {
                        v->rangeInFile.dump(Manager);
                    }
                    return false;
                }
        }

        if(parent!=NULL && statement==NULL) {
            if(subItems.size()>1)
            {
                llvm::outs() << "function decl has multiple bodies\n";
                return false;
            }
        }

        if(statement!=NULL) {
            if(!isa<CompoundStmt>(statement)) {
                if(varDecls.size()!=0)
                {
                    llvm::outs() << "non-compound statement has var decls after normalization\n";
                    return false;
                }
            }
        }
        for(auto sub : subItems) {
            if(sub->isScope()) {
                Scope * s = (Scope*)sub;
                if(s->check(Manager)==false) return false;
            }
        }

        return true;
    }

    virtual void dump(const SourceManager& Manager, unsigned int indent) {
        ScopeItem::dump(Manager, indent);
        for(auto s : subItems) {
            s->dump(Manager, indent+4);
        }
    }
};

struct ScopeTree : public Scope
{
    std::map<const VarDecl*, VarDeclInFile*> varDeclMap;
    std::map<const Stmt*, ScopeItem*> stmtMap;
    ScopeTree() : Scope() {}
    virtual bool isRoot() {return true;}

    virtual bool insert(ScopeItem *scope) {
        if(Scope::insert(scope)==false) {
            llvm::outs() << "failed to insert\n";
            return false;
        } else {
            if(scope->statement!=NULL)
                stmtMap[scope->statement] = scope;
            return true;
        }
    }

    virtual bool addDecl(VarDeclInFile *decl) {
        if(Scope::addDecl(decl)==false) {
            llvm::outs() << "failed to insert\n";
            return false;
        } else {
            varDeclMap[decl->decl] = decl;
            return true;
        }
    }
};

struct VarUseAnalysis
{
    std::set<const VarDecl *> localDeclaredVar;
    std::set<const VarDecl *> sharedVar;

    void declare(const VarDecl *v)
    {
        localDeclaredVar.insert(v);
    }
    void share(const VarDecl *v)
    {
        sharedVar.insert(v);
    }

    bool isInteresting(const VarDecl *v)
    {
        return localDeclaredVar.count(v) != 0;
    }

    bool isShared(const VarDecl *v)
    {
        return sharedVar.count(v) != 0;
    }

    bool isSharedInteresting(const VarDecl *v) {
        return isInteresting(v) && isShared(v);
    }

    bool isLocalInteresting(const VarDecl *v) {
        return isInteresting(v) && !isShared(v);
    }

    struct iterator {
        decltype(localDeclaredVar.begin()) mainIter;
        decltype(localDeclaredVar.end()) mainEnd;
        VarUseAnalysis& host;
        bool shared;

        bool operator==(const iterator& r) {
            return mainIter==r.mainIter && mainEnd==r.mainEnd;
        }
        const VarDecl* operator*() {
            return *mainIter;
        }

        iterator& operator++() {
            mainIter++;
            return *this;
        }

        iterator(VarUseAnalysis& host, bool shared) : 
            mainIter(host.localDeclaredVar.begin()), 
            host(host), shared(shared), 
            mainEnd(host.localDeclaredVar.end()) 
        {
                moveToValid();
        }

        iterator(VarUseAnalysis& host) : mainIter(host.localDeclaredVar.end()),
            mainEnd(host.localDeclaredVar.end()),
            host(host)
        {
        }
    private:
        void moveToValid() {
            while(mainIter!=mainEnd && host.isShared(*mainIter) != shared) 
                mainIter++;
        }
        void moveToNextValid() {
            if(mainIter!=mainEnd) {
                mainIter ++;
                moveToValid();
            }
        }
    };

    struct shared_range {
        shared_range(VarUseAnalysis& h) : host(h) {}
        VarUseAnalysis& host;

        iterator begin() {
            return host.beginShared();
        }
        iterator end() {
            return host.end();
        }

    };

    struct nonshared_range {
        nonshared_range(VarUseAnalysis& h) : host(h) {}
        VarUseAnalysis& host;

        iterator begin() {
            return host.beginNonShared();
        }
        iterator end() {
            return host.end();
        }
    };

    iterator beginNonShared() {
        return iterator(*this, false);
    }

    iterator end() {
        return iterator(*this);
    }

    iterator beginShared() {
        return iterator(*this, true);
    }

    shared_range shared() {
        return shared_range(*this);
    }

    nonshared_range nonshared() {
        return nonshared_range(*this);
    }

    void clear()
    {
        localDeclaredVar.clear();
        sharedVar.clear();
    }
};
class FpArithInstrumentation : public MatchHandler
{
protected:
    VarUseAnalysis varUse;
    ScopeTree *scopeTree;

    const SourceManager *manager;
    Replacements *replace;
    std::string filename;
    std::vector<const Stmt *> fpStatements;
    std::vector<const ParmVarDecl *> fpParameters;

public:
    FpArithInstrumentation(std::map<std::string, Replacements> &r) : MatchHandler(r)
    {
        manager = NULL;
        scopeTree = NULL;
    }
    virtual void run(const MatchFinder::MatchResult &Result)
    {
        const VarDecl *decl = Result.Nodes.getNodeAs<VarDecl>("varDecl");
        if (decl != NULL)
        {
            recordDeclaredFpVar(decl, Result);
        }
        else
        {
            const VarDecl *sharedVar = Result.Nodes.getNodeAs<VarDecl>("refVar");
            if (sharedVar != NULL)
            {
                recordSharedFpVar(sharedVar, Result);
            }
            else
            {
                const Stmt *stmt = Result.Nodes.getNodeAs<Stmt>("stmt");
                if (stmt != NULL)
                {
                    fpStatements.push_back(stmt);
                }
                else
                {
                    const ParmVarDecl *parm = Result.Nodes.getNodeAs<ParmVarDecl>("parm");
                    if (parm != NULL)
                    {
                        fpParameters.push_back(parm);
                    }
                    else
                    {
                        // scope analysis
                        const FunctionDecl *funcScope = Result.Nodes.getNodeAs<FunctionDecl>("func-scope");
                        const Stmt *stmtScope = Result.Nodes.getNodeAs<Stmt>("stmt-scope");
                        const Stmt *outScope = Result.Nodes.getNodeAs<Stmt>("out-scope");

                        if (funcScope != NULL)
                        {
                            // llvm::outs() << "funcScope\n";
                            scopeTree->insert(new Scope(funcScope, Result.SourceManager));
                        }
                        if (stmtScope != NULL)
                        {
                            // llvm::outs() << stmtScope->getStmtClassName() <<"\t";
                            // stmtScope->getSourceRange().print(llvm::outs(), *Result.SourceManager);
                            // llvm::outs() << "\n";
                            scopeTree->insert(new Scope(stmtScope, Result.SourceManager));
                        }
                        if (outScope != NULL)
                        {
                            // llvm::outs() << outScope->getStmtClassName() <<"\n";
                            // outScope->getSourceRange().print(llvm::outs(), *Result.SourceManager);
                            // llvm::outs() << "\n";
                            scopeTree->insert(new ScopeBreak(outScope, Result.SourceManager));
                        }
                    }
                }
            }
        }
    }

    template <typename T>
    void fillReplace(const T *d, const MatchFinder::MatchResult &Result)
    {
        if (manager == NULL)
        {
            manager = Result.SourceManager;
            filename = manager->getFilename(d->getBeginLoc()).str();
            replace = &ReplaceMap[filename];
        }
    }

    void recordDeclaredFpVar(const VarDecl *d, const MatchFinder::MatchResult &Result)
    {
        varUse.declare(d);
        fillReplace(d, Result);
    }
    void recordSharedFpVar(const VarDecl *d, const MatchFinder::MatchResult &Result)
    {
        varUse.share(d);
        fillReplace(d, Result);
    }

    virtual void onStartOfTranslationUnit()
    {
        manager = NULL;
        replace = NULL;
        varUse.clear();

        scopeTree = new ScopeTree();
    }

    virtual void onEndOfTranslationUnit()
    {
        RealVarPrinterHelper helper;
        // std::set<const VarDecl*> staticReal;

        if (manager != NULL)
        {

            std::string header;
            llvm::raw_string_ostream header_stream(header);

            { // add hearder
                header_stream << "#include <ShadowExecution.hpp> // you must put ShadowExecution.hpp into a library path\n";
            }
            
            for (auto var : varUse.localDeclaredVar)
            {
                scopeTree->addDecl(new VarDeclInFile(var, manager));

                if (varUse.isShared(var))
                {
                    std::string varName = PREFIX_LOCAL + var->getNameAsString();
                    helper.staticVarMap[var] = varName;
                }
            }

            scopeTree->dump(*manager, 0);
            if(!scopeTree->check(*manager)) {
                llvm::outs() << "scope tree checks failed\n";
            } else {
                llvm::outs() << "scope tree checks passed\n";
            }

            {
                header_stream.flush();
                Replacement rep = ReplacementBuilder::create(filename, 0, 0, header);
                llvm::Error err = replace->add(rep);
            }

            for (auto stmt : fpStatements)
            {
                if (isa<DeclStmt>(stmt))
                {
                    DeclStmt *decl = (DeclStmt *)stmt;
                    std::string originalCode = print(stmt);
                    std::string code;
                    llvm::raw_string_ostream stream(code);
                    auto it = decl->decl_begin();
                    while (it != decl->decl_end())
                    {
                        VarDecl *vd = (VarDecl *)(*it);
                        {
                            if(varUse.isLocalInteresting(vd)) {
                                auto it = helper.staticVarMap.find(vd);
                                stream << "static SVal " << it->second << "= 0; // initialize once\n";
                                if (vd->hasInit())
                                {
                                    Expr *init = vd->getInit();
                                    stream << it->second << "=" << print(init, &helper) << ";\n";
                                }
                            }
                            else if(varUse.isSharedInteresting(vd))
                            { // shared case
                                std::string varName = vd->getNameAsString();
                                stream << "SVal &" << PREFIX_SHARED << varName << " = DEF(" << varName << ")"; // map a var to real\n";
                            }
                        }
                        it++;
                    }
                    stream.flush();
                    Replacement App(*manager, stmt, originalCode + ";\n /*\n" + code + "*/");
                    llvm::Error err = replace->add(App);
                }
                else
                {
                    std::string originalCode = print(stmt);
                    std::string code = print(stmt, &helper);
                    Replacement App(*manager, stmt, originalCode + ";\n //" + code + "\n");
                    llvm::Error err = replace->add(App);
                }
            }
        }
        if (scopeTree != NULL)
            delete scopeTree;
    }

    struct RealVarPrinterHelper : public PrinterHelper
    {
        std::map<const VarDecl *, std::string> staticVarMap;

        virtual bool handledStmt(Stmt *E, raw_ostream &OS)
        {
            if (isa<DeclRefExpr>(E))
            {
                DeclRefExpr *expr = (DeclRefExpr *)E;
                if (isa<VarDecl>(expr->getDecl()))
                {
                    VarDecl *varDecl = (VarDecl *)expr->getDecl();
                    auto it = staticVarMap.find(varDecl);
                    if (it != staticVarMap.end())
                    {
                        OS << it->second;
                    }
                    else
                    {
                        OS << PREFIX_LOCAL << varDecl->getNameAsString();
                    }
                    return true;
                }
            }
            else if (isa<UnaryOperator>(E))
            {
                UnaryOperator *unary = (UnaryOperator *)E;
                if (unary->getOpcode() == UnaryOperator::Opcode::UO_AddrOf)
                {
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

    // var use
    tool.add(fpVarDeclInFunc, handler);
    tool.add(sharedAddressVar, handler);
    tool.add(referencedVar, handler);
    tool.add(referencedParmVar, handler);

    // scope analysis
    tool.add(funcDecl, handler);
    tool.add(scope, handler);
    tool.add(outStmt, handler);

    tool.add(stmtToBeConverted, handler);
    // tool.add<decltype(target), SingleStmtPatHandler>(target);

    tool.run();

    return 0;
}