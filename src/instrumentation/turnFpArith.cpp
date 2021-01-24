#include <string>
#include <set>
#include "../util/random.h"
#include "../transformer/transformer.hpp"
#include "../transformer/analysis.hpp"
#include "functionTranslation.hpp"

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

// for var useage analysis
auto fpType = realFloatingPointType(); // may be extended
auto fpVarDecl = varDecl(hasType(fpType), anyOf(hasLocalStorage(), isStaticLocal()));
auto fpVarDeclInFunc = functionDecl(isExpansionInMainFile(), forEachDescendant(fpVarDecl.bind("varDecl")));

auto fpFunc = functionDecl(anyOf(returns(fpType), hasAnyParameter(hasType(fpType))));
auto fpInc = unaryOperator(hasAnyOperatorName("++", "--"), hasUnaryOperand(expr(hasType(fpType))));
auto callFpFunc = callExpr(callee(fpFunc));
auto fpAssignFpCall = binaryOperator(isAssignmentOperator(), hasType(fpType), hasRHS(callFpFunc.bind("rhs")));
auto fpAssignFpExpr = binaryOperator(isAssignmentOperator(), hasType(fpType), hasRHS(expr(unless(callFpFunc)).bind("rhs")));

// how to handle struct/union
auto sharedAddressVar = unaryOperator(hasOperatorName("&"), hasUnaryOperand(declRefExpr(isExpansionInMainFile(), to(fpVarDecl.bind("refVar")))));
auto referencedVar = varDecl(isExpansionInMainFile(), hasType(lValueReferenceType(pointee(fpType))), hasInitializer(declRefExpr(to(fpVarDecl.bind("refVar")))));

auto fpRefType = qualType(anyOf(lValueReferenceType(pointee(fpType)), pointerType(pointee(fpType))));
auto referencedParmVar = callExpr(isExpansionInMainFile(), forEachArgumentWithParam(declRefExpr(to(fpVarDecl.bind("refVar"))), parmVarDecl(hasType(fpRefType))));

// from varDecl, we cannot navigate back to its declStmt. so we need the following patterns to get the def sites
// auto fpParmDef = functionDecl(forEach(parmVarDecl(hasType(fpType)).bind("def")), hasBody(compoundStmt().bind("def-site")));
auto fpParmDef = parmVarDecl(isExpansionInMainFile(), hasType(fpType), hasAncestor(functionDecl(hasBody(compoundStmt().bind("def-site"))))).bind("def");
auto fpVarDef = declStmt(isExpansionInMainFile(), forEach(varDecl(hasType(fpType), optionally(hasInitializer(callFpFunc.bind("rhs")))).bind("def"))).bind("def-site");

auto fpConsArrVarDef = declStmt(isExpansionInMainFile(), forEach(varDecl(hasType(constantArrayType(hasElementType(fpType)))).bind("arr-def"))).bind("def-site");

auto fpVarDefGlobal = varDecl(isExpansionInMainFile(), hasType(fpType), hasGlobalStorage(), unless(isStaticLocal())).bind("def");
auto fpConsArrVarDefGlobal = varDecl(isExpansionInMainFile(), hasType(constantArrayType(hasElementType(fpType))), hasGlobalStorage(), unless(isStaticLocal())).bind("def");


// non local var collection
auto fpAddrRes = unaryOperator(isExpansionInMainFile(), hasOperatorName("*"), hasType(fpType)).bind("lFpVal");
auto fpRefUse = declRefExpr(isExpansionInMainFile(), to(varDecl(hasType(lValueReferenceType(pointee(fpType)))))).bind("lFpVal");
auto fpArrElem = arraySubscriptExpr(isExpansionInMainFile(), hasType(fpType)).bind("lFpVal");
auto fpField = memberExpr(isExpansionInMainFile(), hasType(fpType)).bind("lFpVal");

// constant array type should be handled
// auto fpArrDef = varDecl(hasType(constantArrayType(hasElementType(fpType))))
// variable array type and pointer should be handled in the same way

auto fpArg = expr(anyOf(
    declRefExpr(to(fpVarDecl)),
    declRefExpr(to(varDecl(hasType(lValueReferenceType(pointee(fpType)))))),
    unaryOperator(hasOperatorName("*"), hasType(fpType)),
    arraySubscriptExpr(hasType(fpType)),
    memberExpr(hasType(fpType))));

auto fpCallArg = callExpr(isExpansionInMainFile(), callee(fpFunc.bind("callee")), forEachArgumentWithParam(fpArg.bind("arg"), parmVarDecl(hasType(fpType)).bind("parm"))).bind("call");

// four types:  assignment of arith expr, assignment of fpcall, fpcall, fpinc
auto stmtToBeConverted = compoundStmt(isExpansionInMainFile(), forEach(stmt(anyOf(fpInc, fpAssignFpCall, fpAssignFpExpr, callFpFunc)).bind("stmt")));
// auto fpVarDefWithCallInitializer = declStmt(forEach(varDecl(hasType(fpType), hasInitializer(callFpFunc.bind("init"))).bind("var"))).bind("declStmt");

auto fpDynArrVarDef_new = cxxNewExpr(isExpansionInMainFile(), hasType(pointerType(pointee(fpType))), hasArraySize(expr().bind("size"))).bind("alloc");
auto fpDynArrVarDef_malloc = explicitCastExpr(isExpansionInMainFile(), hasDestinationType(pointerType(pointee(fpType))), hasSourceExpression(callExpr(callee(functionDecl(hasName("malloc"))), hasArgument(0, expr().bind("size"))))).bind("alloc");
auto fpDynArrVarUndef_delete = cxxDeleteExpr(isExpansionInMainFile(), hasDescendant(declRefExpr(to(varDecl(hasType(pointerType(pointee(fpType)))))).bind("pointer"))).bind("free");
auto fpDynArrVarUndef_free = callExpr(isExpansionInMainFile(), callee(functionDecl(hasName("free"))), hasArgument(0, expr(hasType(pointerType(pointee(fpType)))).bind("pointer"))).bind("free");;

// local fp var = fpVarDeclInFunc - sharedAddressVar - referencedVar - referencedParmVar

// a stmt is closed, if it is a return or a closed ifStmt/compundStmt/switchStmt
// a compound stmt is closed, if it contains a closed child stmt
// a ifStmt is closed if both then and else are closed
// a switchStmt is generally equal to compound stmt if it is normalized
// break is a semi-closed stmt that closes the nearest switch and for/while/do

// for scope analysis
auto funcDecl = functionDecl(isExpansionInMainFile()).bind("func-scope");
auto scope = stmt(isExpansionInMainFile(), anyOf(compoundStmt(), ifStmt(), forStmt(), whileStmt(), doStmt(), switchStmt())).bind("stmt-scope");
auto outStmt = stmt(isExpansionInMainFile(), anyOf(breakStmt(), returnStmt(optionally(hasReturnValue(stmt().bind("retVal")))))).bind("out-scope");

// root: parent = NULL, statement = NULL
// function decl: parent != NULL, statement = NULL

// hook constructor and destructor
//  constructor: default+customized, copy, move

struct AllocSite
{
    const Expr* size;
    const Expr* alloc;
    SourceRange rangeInFile;
    bool isNew;

    AllocSite(const Expr* a, const Expr* s, SourceManager& manager) : alloc(a), size(s), rangeInFile(manager.getFileLoc(a->getBeginLoc()), manager.getFileLoc(a->getEndLoc())) {
        isNew = isa<CXXNewExpr>(a);
    }
};

struct FreeSite
{
    const Expr* pointer;
    const Expr* free;
    SourceRange rangeInFile;
    FreeSite(const Expr* f, const Expr* p, SourceManager& manager) : free(f), pointer(p), rangeInFile(manager.getFileLoc(f->getBeginLoc()), manager.getFileLoc(f->getEndLoc())) {}
};

struct DynArrRecord
{
    std::vector<AllocSite> allocSites;
    std::vector<FreeSite> freeSites;

    void logAlloc(const Expr* a, const Expr *s, SourceManager& manager)
    {
        allocSites.push_back(AllocSite(a, s, manager));
    }

    void logFree(const Expr* a, const Expr *s, SourceManager& manager)
    {
        freeSites.push_back(FreeSite(a, s, manager));
    }

    void clear()
    {
        allocSites.clear();
        freeSites.clear();
    }
};


struct VarDef
{
    const Stmt *defSite;
    const VarDecl *varDef;

    VarDef(const VarDecl *def, const Stmt *site) : defSite(site), varDef(def) {}
    virtual ~VarDef() {}
    
    bool isParm()
    {
        return isa<ParmVarDecl>(varDef);
    }

    virtual bool isDynamic() const  = 0;
    virtual bool isSingle() const = 0;
    // const CallExpr *callInitializer;
    // VarDef(const VarDecl *def, const Stmt *site, const CallExpr *init = NULL) : varDef(def), defSite(site), callInitializer(init) {}
    // bool isParm()
    // {
    //     return isa<ParmVarDecl>(varDef);
    // }
};

struct SingleVarDef : public VarDef
{
    const CallExpr *callInitializer;
    SingleVarDef(const VarDecl *def, const Stmt *site, const CallExpr *init = NULL) : VarDef(def,site), callInitializer(init) {}
    virtual bool isDynamic() const {return false;}
    virtual bool isSingle() const {return true;}
};

struct ArrVarDef : public VarDef
{
    ArrVarDef(const VarDecl *def, const Stmt *site, const Expr* len, int f, bool dy) : VarDef(def,site), lengthExpr(len), length(), factor(f), dynamic(dy) {}
    ArrVarDef(const VarDecl *def, const Stmt *site, llvm::APInt len, int f, bool dy) : VarDef(def,site), lengthExpr(NULL), length(len), factor(f), dynamic(dy) {}
    const Expr* lengthExpr;
    llvm::APInt length;
    int factor; // sizeof(double) or 1
    bool dynamic;
    virtual bool isDynamic() const {return dynamic;}
    virtual bool isSingle() const {return false;}
};

struct VarDefRecord
{
    std::vector<VarDef *> defs;
    std::vector<VarDef *> globalDefs; // FIXME
    std::map<const VarDecl*, VarDef*> index;
    ~VarDefRecord()
    {
        clear();
    }
    void clear()
    {
        for (auto vd : defs)
            delete vd;
        defs.clear();
        index.clear();
    }
    void insert(const VarDecl *def, const Stmt *site, const CallExpr *init)
    {
        VarDef *vd = new SingleVarDef(def, site, init);
        defs.push_back(vd);
        index[def] = vd;
    }
    void insert(const VarDecl *arrdef, const Stmt *site)
    {
        auto type = (const ConstantArrayType*)arrdef->getType().getTypePtr();
        VarDef *vd = NULL;
        if(type->getSizeExpr()!=NULL) vd = new ArrVarDef(arrdef, site, type->getSizeExpr(), 1, false);
        else vd = new ArrVarDef(arrdef, site, type->getSize(), 1, false);
        defs.push_back(vd);
        index[arrdef] = vd;
    }
    std::map<const Stmt *, std::vector<const VarDef *>> grouping()
    {
        std::map<const Stmt *, std::vector<const VarDef *>> result;
        for (auto def : defs)
        {
            // llvm::outs() << def->varDef->getNameAsString() << " @ " << def->defSite->getStmtClassName() <<"\n";
            result[def->defSite].push_back(def);
        }
        return result;
    }
};

struct CallSite
{
    struct CallArg
    {
        const Expr *arg;
        const ParmVarDecl *parm;
        CallArg(const Expr *a, const ParmVarDecl *p) : arg(a), parm(p) {}
    };
    const CallExpr *call;
    const FunctionDecl *callee;
    std::vector<CallArg> args;
    CallSite(const CallExpr *c, const FunctionDecl *l) : call(c), callee(l) {}
};

struct CallSites
{
    std::map<const CallExpr *, CallSite> sites;

    bool has(const CallExpr *c)
    {
        return sites.find(c) != sites.end();
    }

    CallSite &operator[](const CallExpr *e)
    {
        auto it = sites.find(e);
        assert(it!=sites.end());
        return it->second;
    }
    void insert(const CallExpr *call, const FunctionDecl *callee, const Expr *arg, const ParmVarDecl *parm)
    {
        if (sites.find(call) == sites.end())
        {
            sites.insert(std::make_pair(call, CallSite(call, callee)));
        }
        sites.find(call)->second.args.push_back(CallSite::CallArg(arg, parm));
    }
    void clear()
    {
        sites.clear();
    }
};

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

    virtual bool insert(ScopeItem *scope)
    {
        return false;
    }

    virtual bool isRoot() { return false; }
    virtual ~ScopeItem() {}

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

    bool isFuncDecl()
    {
        return parent != NULL && statement == NULL;
    }
    virtual bool isScope()
    {
        return false;
    }

    virtual void dump(const SourceManager &Manager, unsigned int indent)
    {
        if (parent == NULL)
        {
            llvm::outs() << "root\n";
        }
        else
        {
            if (statement == NULL)
            {
                llvm::outs().indent(indent) << "function decl at ";
            }
            else
            {
                llvm::outs().indent(indent) << statement->getStmtClassName() << " at ";
            }
            rangeInFile.print(llvm::outs(), Manager);
            llvm::outs() << "\n";
        }
    }

    virtual void collectVariablesFrom(ScopeBreak *breakPoint, std::vector<VarDeclInFile *> &results) = 0;
};

struct ScopeBreak : public ScopeItem
{
    bool isSemiBreak;
    const Expr *returnValue;
    ScopeBreak(const Stmt *s, const SourceManager *m, const Expr *r) : ScopeItem(s, m), returnValue(r)
    {
        if (isa<BreakStmt>(s))
            isSemiBreak = true;
        else
            isSemiBreak = false;
    }

    virtual bool isClosedScope()
    {
        return (isSemiBreak == false);
    }

    virtual void collectVariablesFrom(ScopeBreak *breakPoint, std::vector<VarDeclInFile *> &results) {}
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

    virtual ~Scope()
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
            if(subItems.size()==0) return false;
            Scope *sc = (Scope *)subItems[0];
            sc->varDecls.push_back(decl);
            decl->declaredScope = sc;
        }
        else
        {
            if (isRoot())
            {
                decl->decl->print(llvm::outs());
                llvm::outs() << " is intended to be added to root! ignored!\n";
                return true;
            }
            varDecls.push_back(decl);
            decl->declaredScope = this;
        }

        // varDeclMap[decl->decl] = decl;

        return true;
    }

    virtual bool insert(ScopeItem *scope)
    {
        if (!isRoot() && !cover(*scope))
        {
            return false;
        }

        int size = subItems.size();
        for (int i = 0; i < size;)
        {
            if (subItems[i]->insert(scope))
            {
                return true;
            }
            if (scope->cover(*subItems[i]))
            {
                if (size - 1 > i)
                {
                    ScopeItem *tmp = subItems[size - 1];
                    subItems[size - 1] = subItems[i];
                    subItems[i] = tmp;
                }
                size--;
            }
            else
                i++;
        }

        for (int i = subItems.size(); i > size; i--)
        {
            assert(scope->insert(subItems.back()) != false);
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

    bool check(const SourceManager &Manager)
    {
        if (parent == NULL)
        {
            if (varDecls.size() != 0)
            {
                llvm::outs() << "root has var decls\n";
                for (auto v : varDecls)
                {
                    v->rangeInFile.dump(Manager);
                }
                return false;
            }
        }

        if (parent != NULL && statement == NULL)
        {
            if (subItems.size() > 1)
            {
                llvm::outs() << "function decl has multiple bodies\n";
                return false;
            }
        }

        if (statement != NULL)
        {
            if (!isa<CompoundStmt>(statement))
            {
                if (varDecls.size() != 0)
                {
                    llvm::outs() << "non-compound statement has var decls after normalization\n";
                    return false;
                }
            }
        }
        for (auto sub : subItems)
        {
            if (sub->isScope())
            {
                Scope *s = (Scope *)sub;
                if (s->check(Manager) == false)
                    return false;
            }
        }

        return true;
    }

    virtual void dump(const SourceManager &Manager, unsigned int indent)
    {
        ScopeItem::dump(Manager, indent);
        for(auto v : varDecls)
        {
            llvm::outs().indent(indent + 4) << "$" << v->decl->getNameAsString() <<"\n";
        }
        for (auto s : subItems)
        {
            s->dump(Manager, indent + 4);
        }
    }

    virtual void collectVariablesFrom(ScopeBreak *breakPoint, std::vector<VarDeclInFile *> &results)
    {
        if (isRoot() || isFuncDecl())
            return;

        for (auto vd : varDecls)
        {
            if (vd->rangeInFile.getBegin() < breakPoint->rangeInFile.getBegin())
            {
                results.push_back(vd);
            }
        }

        if ((breakPoint->isSemiBreak && this->reactToSemiBreak))
        {
            return;
        }
        else
        {
            this->parent->collectVariablesFrom(breakPoint, results);
        }
    }
};

struct ScopeTree : public Scope
{
    std::map<const VarDecl *, VarDeclInFile *> varDeclMap;
    std::map<const Stmt *, ScopeItem *> stmtMap;

    const std::vector<std::string> *targets;
    ScopeTree() : Scope() {}
    virtual bool isRoot() { return true; }

    virtual bool insert(ScopeItem *scope)
    {
        if(scope->rangeInFile.isInvalid())
        {
            return true;
        }
        
        if (Scope::insert(scope) == false)
        {
            llvm::outs() << "failed to insert\n";
            return false;
        }
        else
        {
            if (scope->statement != NULL)
                stmtMap[scope->statement] = scope;
            return true;
        }
    }

    virtual bool addDecl(VarDeclInFile *decl)
    {
        if (Scope::addDecl(decl) == false)
        {
            llvm::outs() << "failed to insert\n";
            return false;
        }
        else
        {
            varDeclMap[decl->decl] = decl;
            return true;
        }
    }

    bool isClosedScope(ScopeItem *target)
    {
        if (target->statement == NULL)
            return true;

        if (isa<IfStmt>(target->statement))
        {
            Scope *t = (Scope *)target;
            for (auto s : t->subItems)
            {
                if (isClosedScope(s) == false)
                    return false;
            }
            return true;
        }
        else if (isa<CompoundStmt>(target->statement))
        {
            Scope *t = (Scope *)target;
            for (auto s : t->subItems)
            {
                if (isClosedScope(s))
                    return true;
            }
            return false;
        }
        else if (isa<ReturnStmt>(target->statement))
        {
            return true;
        }
        else
            return false;
    }
};

struct VarUseAnalysis
{
    std::set<const VarDecl *> localDeclaredVar;
    std::set<const VarDecl *> sharedVar;
    
    std::set<const VarDecl *> localConstantArrayVar;

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

    bool isConstantArray(const VarDecl *v)
    {
        return localConstantArrayVar.count(v)!=0;
    }

    bool isShared(const VarDecl *v)
    {
        return sharedVar.count(v) != 0;
    }

    bool isSharedInteresting(const VarDecl *v)
    {
        return isInteresting(v) && isShared(v);
    }

    bool isLocalInteresting(const VarDecl *v)
    {
        return isInteresting(v) && !isShared(v);
    }
    std::string getSValName(const VarDecl *v)
    {
        if (isLocalInteresting(v))
            return PREFIX_LOCAL + v->getNameAsString();
        else if (isSharedInteresting(v))
            return PREFIX_SHARED + v->getNameAsString();
        else
            return "<UNK:" + v->getNameAsString() + ">";
    }

    struct iterator
    {
        decltype(localDeclaredVar.begin()) mainIter;
        decltype(localDeclaredVar.end()) mainEnd;
        VarUseAnalysis &host;
        bool shared;

        bool operator==(const iterator &r)
        {
            return mainIter == r.mainIter && mainEnd == r.mainEnd;
        }
        const VarDecl *operator*()
        {
            return *mainIter;
        }

        iterator &operator++()
        {
            mainIter++;
            return *this;
        }

        iterator(VarUseAnalysis &host, bool shared) : mainIter(host.localDeclaredVar.begin()),
                                                      host(host), shared(shared),
                                                      mainEnd(host.localDeclaredVar.end())
        {
            moveToValid();
        }

        iterator(VarUseAnalysis &host) : mainIter(host.localDeclaredVar.end()),
                                         mainEnd(host.localDeclaredVar.end()),
                                         host(host)
        {
        }

    private:
        void moveToValid()
        {
            while (mainIter != mainEnd && host.isShared(*mainIter) != shared)
                mainIter++;
        }
        void moveToNextValid()
        {
            if (mainIter != mainEnd)
            {
                mainIter++;
                moveToValid();
            }
        }
    };

    struct shared_range
    {
        shared_range(VarUseAnalysis &h) : host(h) {}
        VarUseAnalysis &host;

        iterator begin()
        {
            return host.beginShared();
        }
        iterator end()
        {
            return host.end();
        }
    };

    struct nonshared_range
    {
        nonshared_range(VarUseAnalysis &h) : host(h) {}
        VarUseAnalysis &host;

        iterator begin()
        {
            return host.beginNonShared();
        }
        iterator end()
        {
            return host.end();
        }
    };

    iterator beginNonShared()
    {
        return iterator(*this, false);
    }

    iterator end()
    {
        return iterator(*this);
    }

    iterator beginShared()
    {
        return iterator(*this, true);
    }

    shared_range shared()
    {
        return shared_range(*this);
    }

    nonshared_range nonshared()
    {
        return nonshared_range(*this);
    }

    void clear()
    {
        localDeclaredVar.clear();
        sharedVar.clear();
        localConstantArrayVar.clear();
    }
};

struct FpStmt
{
public:
    enum Type
    {
        FP_CALL,
        FP_ASSIGNMENT,
        FP_CALL_ASSIGNMENT,
        FP_INC
        // FP_DECL_WITH_CALL
    };

private:
    FpStmt(Type t, const Stmt *s, const Stmt *r = NULL, const VarDecl *v = NULL) : type(t), fpStmt(s), rhs(r), var(v) {}

public:
    Type type;
    const Stmt *fpStmt;
    const Stmt *rhs;    // for assignment;
    const VarDecl *var; // for decl now

    static FpStmt createFpCall(const Stmt *callExp) { return FpStmt(Type::FP_CALL, callExp); }
    static FpStmt createFpAssignExpr(const Stmt *assign, const Stmt *expr) { return FpStmt(Type::FP_ASSIGNMENT, assign, expr); }
    static FpStmt createFpAssignFpCall(const Stmt *assign, const Stmt *call) { return FpStmt(Type::FP_CALL_ASSIGNMENT, assign, call); }
    static FpStmt createFpInc(const Stmt *inc) { return FpStmt(Type::FP_INC, inc); }
    // static FpStmt createFpVarDecl(const Stmt *decl, const Stmt *call, const VarDecl *var) { return FpStmt(Type::FP_DECL_WITH_CALL, decl, call, var); }
};
class FpArithInstrumentation : public MatchHandler
{
protected:
    VarUseAnalysis varUse;          // check local/shared local var
    ScopeTree *scopeTree;           // handle default local var undef
    VarDefRecord varDefs;           // handle local var def
    std::set<const Expr *> lFpVals; // handle left fp values
    CallSites callSites;
    DynArrRecord dynArrRecord;
    FunctionTranslationStrategy* funcStrategy;


    const SourceManager *manager;
    std::string filename;
    std::vector<FpStmt> fpStatements;
    // std::vector<const ParmVarDecl *> fpParameters;

    struct RealVarPrinterHelper : public PrinterHelper
    {
        std::map<const VarDecl *, std::string> staticVarMap;
        VarUseAnalysis &varUse;
        std::set<const Expr *> &lFpVals;

        RealVarPrinterHelper(VarUseAnalysis &v, std::set<const Expr *> &lv) : varUse(v), lFpVals(lv) {}

        virtual bool handledStmt(Stmt *E, raw_ostream &OS)
        {
            if (isa<Expr>(E))
            {
                if (lFpVals.count((const Expr *)E) != 0)
                {
                    OS << "SVAR(" << print(E) << ")";
                    return true;
                }
                if (isa<DeclRefExpr>(E))
                {
                    DeclRefExpr *expr = (DeclRefExpr *)E;
                    if (isa<VarDecl>(expr->getDecl())) //fixme
                    {
                        VarDecl *varDecl = (VarDecl *)expr->getDecl();
                        if (varUse.isLocalInteresting(varDecl))
                        {
                            OS << staticVarMap[varDecl];
                            return true;
                        }
                        else if (varUse.isSharedInteresting(varDecl))
                        {
                            OS << varUse.getSValName(varDecl);
                            return true;
                        }
                    }
                }
            }
            return false;
        }
    };

public:
    FpArithInstrumentation(std::map<std::string, Replacements> &r) : MatchHandler(r)
    {
        manager = NULL;
        scopeTree = NULL;
    }
    virtual void run(const MatchFinder::MatchResult &Result)
    {
        
        {
            const VarDecl *decl = Result.Nodes.getNodeAs<VarDecl>("varDecl"); // var decl, single double var only
            const VarDecl *sharedVar = Result.Nodes.getNodeAs<VarDecl>("refVar");
            const VarDecl *def = Result.Nodes.getNodeAs<VarDecl>("def"); // var def
            const VarDecl *arrDef = Result.Nodes.getNodeAs<VarDecl>("arr-def");

            if (decl != NULL)
            {
                varUse.declare(decl);
                fillReplace(decl, Result);
                return;
            }

            if (def != NULL)
            {
                const Stmt *site = Result.Nodes.getNodeAs<Stmt>("def-site");
                if(isa<ParmVarDecl>(def))
                {
                    if(site->getSourceRange().fullyContains(def->getSourceRange())) return;
                }
                const CallExpr *init = Result.Nodes.getNodeAs<CallExpr>("rhs");
                varDefs.insert(def, site, init);
                fillReplace(def, Result);
                return;
            }

            if(arrDef !=NULL)
            {
                const Stmt *site = Result.Nodes.getNodeAs<Stmt>("def-site");
                varDefs.insert(arrDef, site);
                varUse.localConstantArrayVar.insert(arrDef);
                fillReplace(arrDef, Result);
                return;
            }

            if (sharedVar != NULL)
            {
                varUse.share(sharedVar);
                fillReplace(sharedVar, Result);
                return;
            }
        }

        {
            const Expr *lFpVal = Result.Nodes.getNodeAs<Expr>("lFpVal");
            if (lFpVal != NULL)
            {
                lFpVals.insert(lFpVal);
                fillReplace(lFpVal, Result);
                return;
            }
        }

        {
            auto call = Result.Nodes.getNodeAs<CallExpr>("call");
            if (call != NULL)
            {
                auto callee = Result.Nodes.getNodeAs<FunctionDecl>("callee");
                auto arg = Result.Nodes.getNodeAs<Expr>("arg");
                auto parm = Result.Nodes.getNodeAs<ParmVarDecl>("parm");
                callSites.insert(call, callee, arg, parm);
                fillReplace(call, Result);
                return;
            }
        }

        {
            const Stmt *stmt = Result.Nodes.getNodeAs<Stmt>("stmt");
            if (stmt != NULL)
            {
                if (isa<UnaryOperator>(stmt))
                {
                    fpStatements.push_back(FpStmt::createFpInc(stmt));
                }
                else if (isa<CallExpr>(stmt))
                {
                    fpStatements.push_back(FpStmt::createFpCall(stmt));
                }
                else if (isa<BinaryOperator>(stmt))
                {
                    const Expr *rhs = Result.Nodes.getNodeAs<Expr>("rhs");
                    if (isa<CallExpr>(rhs))
                    {
                        fpStatements.push_back(FpStmt::createFpAssignFpCall(stmt, rhs));
                    }
                    else
                    {
                        fpStatements.push_back(FpStmt::createFpAssignExpr(stmt, rhs));
                    }
                }
                fillReplace(stmt, Result);
                return;
            }
        }

        {
            const Expr* alloc = Result.Nodes.getNodeAs<Expr>("alloc");
            const Expr* free = Result.Nodes.getNodeAs<Expr>("free");

            if(alloc!=NULL)
            {
                const Expr* size = Result.Nodes.getNodeAs<Expr>("size");
                dynArrRecord.logAlloc(alloc, size, *Result.SourceManager);
                fillReplace(alloc, Result);
            } 
            else if(free!=NULL)
            {
                const Expr* pt = Result.Nodes.getNodeAs<Expr>("pointer");
                dynArrRecord.logFree(free, pt, *Result.SourceManager);
                fillReplace(free, Result);
            }
        }

        {
            // scope analysis, must be the final part
            const FunctionDecl *funcScope = Result.Nodes.getNodeAs<FunctionDecl>("func-scope");
            const Stmt *stmtScope = Result.Nodes.getNodeAs<Stmt>("stmt-scope");
            const Stmt *outScope = Result.Nodes.getNodeAs<Stmt>("out-scope");

            if (funcScope != NULL)
            {
                scopeTree->insert(new Scope(funcScope, Result.SourceManager));
                fillReplace(funcScope, Result);
            }
            if (stmtScope != NULL)
            {
                scopeTree->insert(new Scope(stmtScope, Result.SourceManager));
                fillReplace(stmtScope, Result);
            }
            if (outScope != NULL)
            {
                const Expr *ret = Result.Nodes.getNodeAs<Expr>("retVal");
                scopeTree->insert(new ScopeBreak(outScope, Result.SourceManager, ret));
                fillReplace(outScope, Result);
            }

            return;
        }
    }

    template <typename T>
    void fillReplace(const T *d, const MatchFinder::MatchResult &Result)
    {
        if (manager == NULL)
        {
            manager = Result.SourceManager;
            filename = manager->getFilename(d->getBeginLoc()).str();
            if(this->targets->count(filename)==0)
            {
                manager = NULL;
            }
        }
    }

    void doConstructVarScope(RealVarPrinterHelper &helper)
    {
        for (auto var : varUse.localDeclaredVar)
        {
            scopeTree->addDecl(new VarDeclInFile(var, manager));

            if (!varUse.isShared(var))
            {
                helper.staticVarMap[var] = varUse.getSValName(var);
            }
        }
        for (auto var : varUse.localConstantArrayVar)
        {
            scopeTree->addDecl(new VarDeclInFile(var, manager));
        }
    }

    virtual void onStartOfTranslationUnit()
    {
        manager = NULL;
        scopeTree = new ScopeTree();
    }

    void setFunctionStrategy(FunctionTranslationStrategy* funcStrategy)
    {
        this->funcStrategy = funcStrategy;
    }

    virtual void onEndOfTranslationUnit()
    {
        // std::set<const VarDecl*> staticReal;
        if (manager != NULL)
        {
            RealVarPrinterHelper helper(varUse, lFpVals);
            std::string header;
            llvm::raw_string_ostream header_stream(header);

            { // add hearder
                header_stream << "#include <real/ShadowExecution.hpp> // you must put ShadowExecution.hpp into a library path\n";
            }

            // Var and scope construction
            doConstructVarScope(helper);

            // scopeTree->dump(*manager, 0);
            if (!scopeTree->check(*manager))
            {
                llvm::outs() << "scope tree checks failed\n";
            }
            else
            {
                llvm::outs() << "scope tree checks passed\n";
            }

            {
                header_stream.flush();
                Replacement rep = ReplacementBuilder::create(filename, 0, 0, header);
                addReplacement(rep);
            }

            doDefReals(helper);

            doTranslateDynArrDef();
            // parameter init
            doTranslateRealStatements(helper);
            // undef
            doUndefReals(scopeTree, helper);
        }
        clear();
    }

    void doTranslateDynArrDef()
    {
        for(auto s : dynArrRecord.allocSites)
        {
            int factor = s.isNew ? 1 : 8;
            std::string code;
            llvm::raw_string_ostream stream(code);
            stream << "DYNDEF(" << print(s.size) << "/" << factor <<")";
            stream.flush();
            Replacement App = ReplacementBuilder::create(*manager, s.alloc , code);
            addReplacement(App);
        }
        for(auto s : dynArrRecord.freeSites)
        {
            std::string code;
            llvm::raw_string_ostream stream(code);
            stream << "DYNUNDEF(" << print(s.pointer) << ")";
            stream.flush();
            Replacement App = ReplacementBuilder::create(*manager, s.free , code);
            addReplacement(App);
        }
    }

    void clear()
    {
        if (scopeTree != NULL)
            delete scopeTree;
        varUse.clear();
        varDefs.clear();
        lFpVals.clear();
        fpStatements.clear();
        callSites.clear();
        dynArrRecord.clear();
    }

protected:
    void doDefReals(RealVarPrinterHelper &helper)
    {
        auto groups = varDefs.grouping();

        for (auto group : groups)
        {
            SourceRange siteRangeInFile = SourceRange(manager->getFileLoc(group.first->getBeginLoc()), manager->getFileLoc(group.first->getEndLoc()));
            bool isParmDef = isa<CompoundStmt>(group.first);

            if (group.second.size() == 1 && group.second[0]->isSingle() && ((const SingleVarDef*)group.second[0])->callInitializer!=NULL)
            { // single def with fp call initialer
                std::string sValName = varUse.getSValName(group.second[0]->varDef);
                doTranslateCall(group.first, ((const SingleVarDef*)group.second[0])->callInitializer, sValName, group.second[0]->varDef->getNameAsString(), helper);
            }
            else
            {
                std::string code;
                llvm::raw_string_ostream stream(code);
                if (isParmDef)
                    stream << "{\n";
                // stream << "/*\n";
                for (auto v : group.second)
                {
                    if(v->isSingle())
                    {
                        assert(((const SingleVarDef*)v)->callInitializer == NULL);
                        if (varUse.isInteresting(v->varDef))
                        {
                            std::string varName = v->varDef->getNameAsString();
                            std::string sVarName = varUse.getSValName(v->varDef);
                            // decl shadow
                            if (varUse.isShared(v->varDef))
                            {
                                stream << "S_SVAL " << sVarName << " = DEF(" << varName << ");\n"; // map a parameter to real\n";
                            }
                            else
                            {
                                stream << "L_SVAL " << sVarName << " = 0;\n"; // map a parameter to real\n";
                            }

                            // init
                            if (isParmDef)
                            {
                                const ParmVarDecl *parm = (const ParmVarDecl *)v->varDef;
                                int idx = parm->getFunctionScopeIndex();
                                stream << "LOADPARM(" << idx << "," << sVarName << "," << varName << ");\n";
                            }
                            else
                            {
                                if (v->varDef->hasInit())
                                {
                                    const Expr *init = v->varDef->getInit();
                                    stream << sVarName << "=" << print(init, &helper) << ";\n";
                                }
                            }
                        }
                    }
                    else
                    { // var
                        auto arrDef = (ArrVarDef*)v;
                        std::string varName = v->varDef->getNameAsString();
                        // we do not generate alias for array var. instead, we declare map space for them. this may cause extra runtime overhead
                        // we expact that ARRDEF also initializes the array. BUT for parameters, we may also load them from call stack!
                        if(arrDef->lengthExpr!=NULL)
                        {
                            stream  << "ARRDEF(" << varName << ", " << print(arrDef->lengthExpr) << "/" << arrDef->factor << ");\n"; 
                        }
                        else
                        {
                            stream  << "ARRDEF(" << varName << ", ";
                            arrDef->length.print(stream,false);
                            stream << "/" << arrDef->factor << ");\n"; 
                        }
                        
                    }
                }
                // stream << "*/\n";
                stream.flush();
                if (isParmDef)
                {
                    Replacement App = ReplacementBuilder::create(*manager, siteRangeInFile.getBegin(), 1, code);
                    addReplacement(App);
                }
                else
                {
                    std::string originalCode = print(group.first);
                    Replacement App = ReplacementBuilder::create(*manager, group.first, originalCode + ";\n" + code);
                    addReplacement(App);
                }
            }
        }
    }

    void doUndefReals(ScopeItem *scopeItem, RealVarPrinterHelper &helper)
    {
        if (scopeItem->isScope())
        {
            Scope *scope = (Scope *)scopeItem;
            if (scopeItem->statement != NULL && isa<CompoundStmt>(scopeItem->statement))
            { // we only need to check compound statement
                if (!scopeTree->isClosedScope(scopeItem))
                {
                    // deallocate
                    std::string preCode;
                    llvm::raw_string_ostream stream(preCode);

                    // stream << "/*\n"; // for debugging
                    for (auto v : scope->varDecls)
                    {
                        if (varUse.isSharedInteresting(v->decl))
                        {
                            stream << "UNDEF(" << v->decl->getNameAsString() << ");\n";
                        }
                        else if(varUse.isConstantArray(v->decl))
                        {
                            auto def = (ArrVarDef*) varDefs.index[v->decl];
                            stream << "ARRUNDEF(" << v->decl->getNameAsString() << ", ";
                            def->length.print(stream, false);
                            stream << "/" << def->factor << ");\n";
                        }
                        else{
                            stream <<"// leave "<< v->decl->getNameAsString() << "\n";
                        }
                    }
                    // stream << "*/\n"; // for debugging
                    stream << "}\n";
                    stream.flush();

                    Replacement Rep = ReplacementBuilder::create(*manager, scopeItem->rangeInFile.getEnd(), 1, preCode);
                    addReplacement(Rep);
                }
            }
            for (auto sub : scope->subItems)
            {
                doUndefReals(sub, helper);
            }
        }
        else
        {
            ScopeBreak *sbreak = (ScopeBreak *)scopeItem;
            std::vector<VarDeclInFile *> vars;
            sbreak->parent->collectVariablesFrom(sbreak, vars);
            std::string preCode;
            llvm::raw_string_ostream stream(preCode);
            // stream << "/*\n"; // for debugging

            if (sbreak->isSemiBreak == false)
            {
                auto retStmt = (ReturnStmt *)sbreak->statement;
                auto retVal = sbreak->returnValue;
                if (retVal != NULL && isa<DeclRefExpr>(retVal))
                {
                    auto ref = (DeclRefExpr *)retVal;
                    auto valDecl = ref->getDecl();
                    if (isa<VarDecl>(valDecl))
                    {
                        VarDecl *var = (VarDecl *)valDecl;
                        if (varUse.isLocalInteresting(var))
                        {
                            // note that if it returns a structure/class, we must generate multiple PUSHRET statements
                            // it is safe to push this real on the stack
                            std::string retName = var->getNameAsString();
                            stream << "PUSHRET(0," << varUse.getSValName(var) << ");\n"; // use std::move here\n";
                        }
                        else if (varUse.isSharedInteresting(var))
                        {
                            // generally, we should generate a static var for this case. Or we can analyze var use in
                            auto retName = randomIdentifier("retTmp");
                            stream << "L_SVAL " << retName << " = 0; // initialize once\n";
                            stream << retName << " = std::move(" << varUse.getSValName(var) << ");// use std::move here\n";
                            stream << "PUSHRET(0," << retName << ");\n"; // use std::move here\n";
                        }
                    }
                }
            }
            // else
            // {
            //     llvm::outs() << sbreak->statement->getStmtClassName() << "\n";
            // }

            for (auto v : vars)
            {
                if (varUse.isSharedInteresting(v->decl))
                {
                    stream << "UNDEF(" << v->decl->getNameAsString() << ");\n";
                }
                else if (varUse.isConstantArray(v->decl))
                {
                    auto def = (ArrVarDef *)varDefs.index[v->decl];
                    stream << "ARRUNDEF(" << v->decl->getNameAsString() << ", ";
                    def->length.print(stream, false);
                    stream << "/" << def->factor << ");\n";
                }
                else
                {
                    stream <<"// leave "<< v->decl->getNameAsString() << "\n";
                }
            }
            // stream << "*/\n"; // for debugging
            stream.flush();

            Replacement Rep = ReplacementBuilder::create(*manager, scopeItem->rangeInFile.getBegin(), 0, preCode);
            addReplacement(Rep);
        }
    }

    void doTranslateCall(const Stmt *site, const CallExpr *call, std::string ret, std::string oRet, RealVarPrinterHelper &helper)
    {

        std::string repCode;
        llvm::raw_string_ostream stream(repCode);
        // stream << "//"; // for debugging

        auto callee = call->getDirectCallee();

        if(funcStrategy->isTranslated(callee, manager))
        {
            stream << "PUSHCALL("<< callee->getNumParams() << ");\n";
            if (callSites.has(call))
            {
                for (auto callArg : callSites[call].args)
                {
                    auto arg = callArg.arg;
                    bool isInterestingVar = false;
                    if (isa<DeclRefExpr>(arg))
                    {
                        isInterestingVar = varUse.isLocalInteresting((const VarDecl *)((const DeclRefExpr *)arg)->getDecl());
                    }
                    if (isInterestingVar || lFpVals.count(arg) != 0)
                    {
                        // stream << "//"; // for debugging
                        stream << "PUSHARG(" << callArg.parm->getFunctionScopeIndex() << "," << print(arg, &helper) << ");\n";
                    }
                }
            }
            stream << print(site);
            if (isa<DeclStmt>(site))
            {
                auto ds = (const DeclStmt *)site;
                auto vd = (const VarDecl *)ds->getSingleDecl();
                std::string varName = vd->getNameAsString();
                std::string sVarName = varUse.getSValName(vd);
                // decl shadow
                // stream << "//";
                if (varUse.isShared(vd))
                {
                    stream << "S_SVAL " << sVarName << " = DEF(" << varName << ");\n"; // map a parameter to real\n";
                }
                else
                {
                    stream << "L_SVAL " << sVarName << " = 0;\n"; // map a parameter to real\n";
                }
            }
            else
                stream << ";";

            // stream << "//"; // for debugging
            if (ret.size() == 0)
                stream << "POPCALL()";
            else
                stream << "POPRET(0, " << ret << "," << oRet << ")";
        } // end of call
        else if(auto abs = funcStrategy->hasAbstractedFunction(callee->getNameAsString()))
        {
            stream << print(site);
            if (isa<DeclStmt>(site))
            {
                auto ds = (const DeclStmt *)site;
                auto vd = (const VarDecl *)ds->getSingleDecl();
                std::string varName = vd->getNameAsString();
                std::string sVarName = varUse.getSValName(vd);
                
                if (varUse.isShared(vd))
                {
                    stream << "S_SVAL " << sVarName << " = DEF(" << varName << ");\n"; // map a parameter to real\n";
                }
                else
                {
                    stream << "L_SVAL " << sVarName << " = 0;\n"; // map a parameter to real\n";
                }
            }
            else stream << ";";

            if (ret.size() == 0) {}
            else
            {
                stream << ret << " = " << *abs << "(";
                for(int i=0, size = call->getNumArgs(); i<size; i++)
                {
                    stream << print(call->getArg(i), &helper);
                    if(i!=0) stream <<", ";
                }
                stream <<")";
            }
        }
        else if(funcStrategy->isPseudoFunction(callee->getNameAsString()))
        {
            doTranslatePseudoFunctionCall(stream, call, helper);
        }
        else
        { // use original function here, we use the original result for now
            stream << print(site);
            if (isa<DeclStmt>(site))
            {
                auto ds = (const DeclStmt *)site;
                auto vd = (const VarDecl *)ds->getSingleDecl();
                std::string varName = vd->getNameAsString();
                std::string sVarName = varUse.getSValName(vd);
                // decl shadow
                // stream << "//";
                if (varUse.isShared(vd))
                {
                    stream << "S_SVAL " << sVarName << " = DEF(" << varName << ");\n"; // map a parameter to real\n";
                }
                else
                {
                    stream << "L_SVAL " << sVarName << " = 0;\n"; // map a parameter to real\n";
                }
                // stream << "//"; // for debugging
            }
            else
                stream << ";";

            if (ret.size() == 0) {}
            else
                stream << ret << " = " << oRet <<"; // in";
        }

        stream.flush();
        Replacement App = ReplacementBuilder::create(*manager, site, repCode);
        addReplacement(App);
    }

    void doTranslatePseudoFunctionCall(llvm::raw_ostream& stream, const CallExpr* call, clang::PrinterHelper& helper)
    {
        auto funcName = call->getDirectCallee()->getNameAsString();
        
        if(funcName == "EAST_DUMP")
        {
            stream << print(call, &helper) << ";";
        }
        else if(funcName == "EAST_ANALYZE")
        {
            stream << "EAST_ANALYZE("
                   << print(call->getArg(0), &helper)
                   <<","
                   << print(call->getArg(1), &helper)
                   <<","
                   << print(call->getArg(1))
                   << ");";
        }
    }

    void doTranslateCall(const Stmt *site, const CallExpr *call, const Expr *ret, RealVarPrinterHelper &helper)
    {
        std::string retName = "";
        std::string oRetName = "";
        if (ret != NULL)
        {
            retName = print(ret, &helper);
            oRetName = print(ret);

        }
        doTranslateCall(site, call, retName, oRetName, helper);
    }

    void doTranslateRealStatements(RealVarPrinterHelper &helper)
    {
        for (auto stmt : fpStatements)
        {
            switch (stmt.type)
            {
            case FpStmt::Type::FP_INC:
            case FpStmt::Type::FP_ASSIGNMENT:
            {
                std::string originalCode = print(stmt.fpStmt);
                std::string code = print(stmt.fpStmt, &helper);
                Replacement App = ReplacementBuilder::create(*manager, stmt.fpStmt, originalCode + ";\n" + code + "\n");
                addReplacement(App);
            }
            break;
            case FpStmt::Type::FP_CALL:
            {
                const CallExpr *call = (const CallExpr *)stmt.fpStmt;
                doTranslateCall(stmt.fpStmt, call, NULL, helper);
            }
            break;
            case FpStmt::Type::FP_CALL_ASSIGNMENT:
            {
                const CallExpr *call = (const CallExpr *)stmt.rhs;
                doTranslateCall(stmt.fpStmt, call, ((const BinaryOperator *)stmt.fpStmt)->getLHS(), helper);
            }
            break;
                // case FpStmt::Type::FP_DECL_WITH_CALL:
                // {
                //     const CallExpr *call = (const CallExpr *)stmt.rhs;
                //     std::string sRet = varUse.getSValName(stmt.var);
                //     doTranslateCall(stmt.fpStmt, call, sRet, helper);
                // }
                // break;
            }
        }
    }
};


class FunctionInfoCollector : public MatchFinder::MatchCallback
{
protected:
    const std::set<std::string> *targets;
public:
    FunctionInfoCollector(FunctionTranslationStrategy * strategy) : strategy(strategy), targets(NULL) {}
    FunctionTranslationStrategy * strategy;
    

    void setTargets(const std::set<std::string> *t)
    {
        targets = t;
    }
    virtual void run(const MatchFinder::MatchResult &Result)
    {
        const FunctionDecl* decl = Result.Nodes.getNodeAs<FunctionDecl>("decl");
        SourceLocation loc = Result.SourceManager->getFileLoc(decl->getBeginLoc());
        auto filename = Result.SourceManager->getFilename(loc).str();
        if(targets->count(filename)!=0)
        {
            strategy->translatedFunctions.insert(loc);
        }
    }

};


int main(int argc, const char **argv)
{
    CommonOptionsParser Options(argc, argv, ScDebugTool);
    FunctionTranslationStrategy funcStrategy;

    CodeAnalysisTool funcCollector(Options);
    FunctionInfoCollector collector(&funcStrategy);
    funcCollector.add(fpFunc.bind("decl"), collector);
    funcCollector.run();


    CodeTransformationTool tool(Options, "Instrumentation: FpArith");

    FpArithInstrumentation handler(tool.GetReplacements());

    handler.setFunctionStrategy(&funcStrategy);

    // var use
    tool.add(fpVarDeclInFunc, handler);
    tool.add(sharedAddressVar, handler);
    tool.add(referencedVar, handler);
    tool.add(referencedParmVar, handler);

    // scope analysis
    tool.add(funcDecl, handler);
    tool.add(scope, handler);
    tool.add(outStmt, handler);

    // def record
    tool.add(fpParmDef, handler);
    tool.add(fpVarDef, handler);
    tool.add(fpConsArrVarDef, handler);

    // lvalue recording
    tool.add(fpAddrRes, handler);
    tool.add(fpArrElem, handler);
    tool.add(fpRefUse, handler);
    tool.add(fpField, handler);
    

    // dyn arr
    tool.add(fpDynArrVarDef_new, handler);
    tool.add(fpDynArrVarDef_malloc, handler);
    tool.add(fpDynArrVarUndef_delete, handler);
    tool.add(fpDynArrVarUndef_free, handler);

    // call sites
    tool.add(fpCallArg, handler);

    tool.add(stmtToBeConverted, handler);
    // tool.add(fpVarDefWithCallInitializer, handler);
    // tool.add(fpParm, handler);
    // tool.add<decltype(target), SingleStmtPatHandler>(target);

    tool.run();

    return 0;
}