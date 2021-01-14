#ifndef TRANSFORMER_HPP
#define TRANSFORMER_HPP

#include <unordered_map>
#include <vector>
#include "clang/AST/AST.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

class ExprReplacementHelper : public PrinterHelper {
protected:
    std::unordered_map<const Stmt*, std::string> exprMap;
public:
    void addShortcut(const Expr* e, const std::string& s) {
        exprMap[e] = s;
    }

    virtual bool handledStmt(Stmt* E, raw_ostream& OS) {
        auto r = exprMap.find(E);
        if(r==exprMap.end()) return false;
        else {
            OS << r->second;
            return true;
        }
    }
};

struct ExpressionExtractionRequest
{
    const Expr *expression;
    QualType type;
    const Stmt *statement;
    const SourceManager *manager;

    ExpressionExtractionRequest(const Expr *a, const QualType t, const Stmt *s, const SourceManager *m) : expression(a), type(t), statement(s), manager(m) {}

    friend bool operator==(const ExpressionExtractionRequest &l, const ExpressionExtractionRequest &r) {
        return l.statement == r.statement && l.expression->getSourceRange() == r.expression->getSourceRange();
    }
    friend bool operator<(const ExpressionExtractionRequest &l, const ExpressionExtractionRequest &r)
    {
        if (l.statement == r.statement)
        {
            if (r.expression->getSourceRange().fullyContains(l.expression->getSourceRange()))
            {
                return true;
            }
            else if (l.expression->getSourceRange().fullyContains(r.expression->getSourceRange()))
            {
                return false;
            }
            else
                return l.expression->getBeginLoc() < r.expression->getBeginLoc();
        }
        else
        {
            return l.statement->getBeginLoc() < r.statement->getBeginLoc();
        }
    }
};

class MatchHandler : public MatchFinder::MatchCallback
{
protected:
    std::map<std::string, Replacements> &ReplaceMap;

public:
    MatchHandler(std::map<std::string, Replacements> &r) : ReplaceMap(r) {}

    static std::string print(const Stmt* stmt, PrinterHelper *helper=NULL) {
        std::string str;
        llvm::raw_string_ostream stream(str);
        stmt->printPretty(stream, helper, PrintingPolicy(LangOptions()));
        stream.flush();
        return str;
    }

    static std::string flatten_declStmt(const DeclStmt *decl)
    {
        if (decl->isSingleDecl())
        {
            return MatchHandler::print(decl);
        }
        else
        {
            std::ostringstream out;
            auto decls = decl->decl_begin();
            while(decls!=decl->decl_end())
            {
                VarDecl* vd = (VarDecl*)(*decls);
                out << vd->getType().getAsString()
                    << " "
                    << vd->getNameAsString();

                if(vd->hasInit()) {
                    out << "=" << MatchHandler::print(vd->getInit());
                }
                out <<";" <<std::endl;
                decls ++;
            }
            out.flush();
            return out.str();
        }
    }
};

class CodeTransformationTool
{
protected:
    CommonOptionsParser Options;
    RefactoringTool Tool;
    MatchFinder Finder;
    std::vector<MatchHandler*> handlerVector;

public:
    CodeTransformationTool(int argc, const char **argv, llvm::cl::OptionCategory category) : Options(argc, argv, category),
                                                                                             Tool(Options.getCompilations(), Options.getSourcePathList()),
                                                                                             Finder()
    {
    }

    ~CodeTransformationTool() {
        int size = handlerVector.size();
        for(int i=0;i<size;i++) {
            delete handlerVector[i];
        }
    }

    std::map<std::string, Replacements> &GetReplacements() {
        return Tool.getReplacements();
    }


    template <typename ASTMatcherType,typename HandlerType>
    void add(const ASTMatcherType &matcher)
    {
        HandlerType* h = new HandlerType(Tool.getReplacements());
        handlerVector.push_back(h);
        Finder.addMatcher(matcher, h);
    }
    
    template <typename ASTMatcherType,typename HandlerType>
    void add(const ASTMatcherType &matcher, HandlerType& handler)
    {
        Finder.addMatcher(matcher, &handler);
    }

    int run()
    {
        if (int Result = Tool.run(newFrontendActionFactory(&Finder).get()))
        {
            return Result;
        }

        // We need a SourceManager to set up the Rewriter.
        IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
        DiagnosticsEngine Diagnostics(
            IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs()), &*DiagOpts,
            new TextDiagnosticPrinter(llvm::errs(), &*DiagOpts), true);
        
        SourceManager Sources(Diagnostics, Tool.getFiles());


        // Apply all replacements to a rewriter.
        Rewriter Rewrite(Sources, LangOptions());
        Tool.applyAllReplacements(Rewrite);

        // Query the rewriter for all the files it has rewritten, dumping their new
        // contents to stdout.
        for (Rewriter::buffer_iterator I = Rewrite.buffer_begin(),
                                       E = Rewrite.buffer_end();
             I != E; ++I)
        {
            const FileEntry *Entry = Sources.getFileEntryForID(I->first);
            std::error_code err;
            llvm::raw_fd_ostream out(Entry->getName(), err);
            out << "// Rewrite " << Entry->getName() << "\n";
            I->second.write(out);
            out.flush();
            out.close();
        }
        return 0;
    }
};

#endif