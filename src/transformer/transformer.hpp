#ifndef TRANSFORMER_HPP
#define TRANSFORMER_HPP

#include <unordered_map>
#include <vector>
#include <clang/AST/AST.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Refactoring.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>
#include <clang/Driver/Options.h>

// #include "clang/StaticAnalyzer/Frontend/FrontendActions.h"

// #include "llvm/ADT/STLExtras.h"
#include <llvm/Option/OptTable.h>
#include <llvm/Support/TargetSelect.h>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

struct ReplacementBuilder
{
    static Replacement create(StringRef FilePath, unsigned Offset, unsigned Length, StringRef ReplacementText)
    {
        return Replacement(FilePath, Offset, Length, ReplacementText);
    }

    static Replacement create(const SourceManager &Sources, SourceLocation Start,
                       unsigned Length, StringRef ReplacementText)
    {
        SourceLocation startLoc = Sources.getFileLoc(Start);
        return Replacement(Sources, startLoc, Length, ReplacementText);
    }

    template <typename Node>
    static Replacement create(const SourceManager &Sources, const Node &NodeToReplace,
                StringRef ReplacementText, const LangOptions &LangOpts = LangOptions())
    {
        SourceLocation startLoc = Sources.getFileLoc(NodeToReplace->getBeginLoc());
        SourceLocation endLoc = Sources.getFileLoc(NodeToReplace->getEndLoc());
        return Replacement(Sources, CharSourceRange::getTokenRange(startLoc, endLoc), ReplacementText, LangOpts);
    }
};

class ExtractionPrinterHelper : public PrinterHelper
{
protected:
    std::unordered_map<const Stmt *, std::string> extractionMap;

public:
    void addShortcut(const Expr *e, const std::string &s)
    {
        extractionMap[e] = s;
    }

    virtual bool handledStmt(Stmt *E, raw_ostream &OS)
    {
        auto r = extractionMap.find(E);
        if (r == extractionMap.end())
            return false;
        else
        {
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
    SourceRange actualExpressionRange;
    SourceLocation actualStatementStartLoc;

    ExpressionExtractionRequest(const Expr *a, const QualType t, const Stmt *s, const SourceManager *m) : expression(a), type(t), statement(s), manager(m) {
        SourceLocation startLoc = m->getFileLoc(a->getBeginLoc());
        SourceLocation endLoc = m->getFileLoc(a->getEndLoc());
        actualExpressionRange = SourceRange(startLoc, endLoc);
        actualStatementStartLoc = m->getFileLoc(s->getBeginLoc());
    }

    bool cover(const ExpressionExtractionRequest &r) {
        return this->actualExpressionRange.fullyContains(r.actualExpressionRange);
    }

    friend bool operator==(const ExpressionExtractionRequest &l, const ExpressionExtractionRequest &r)
    {
        return l.statement == r.statement && l.actualExpressionRange == r.actualExpressionRange;
    }
    friend bool operator<(const ExpressionExtractionRequest &l, const ExpressionExtractionRequest &r)
    {
        if (l.statement == r.statement)
        {
            if (r.actualExpressionRange.fullyContains(l.actualExpressionRange))
            {
                return true;
            }
            else if (l.actualExpressionRange.fullyContains(r.actualExpressionRange))
            {
                return false;
            }
            else
                return l.actualExpressionRange.getBegin() < r.actualExpressionRange.getBegin();
        }
        else
        {
            return l.actualStatementStartLoc < r.actualStatementStartLoc;
        }
    }
};

struct StmtInFile {
    const Stmt* statement;
    SourceRange rangeInFile;

    StmtInFile(const Stmt* s, SourceRange r) : statement(s), rangeInFile(r) {}

    StmtInFile(const Stmt* s, const SourceManager* m) : statement(s), rangeInFile(m->getFileLoc(s->getBeginLoc()),m->getFileLoc(s->getEndLoc())) {
    }
    bool cover(const StmtInFile &r) {
        if(rangeInFile.isInvalid()) return true;
        return this->rangeInFile.fullyContains(r.rangeInFile);
    }
};

struct NonOverlappedStmts {
    std::vector<StmtInFile> roots;

    void add(StmtInFile stmtInFile) {
        int size = roots.size();
        bool cover = false;
        for(int i=0;i<size;) {
            if(stmtInFile.cover(roots[i])) {
                if(size-1>i) {
                    roots[i] = roots[size-1];
                }
                size --;
            } else if(roots[i].cover(stmtInFile)) {
                cover = true;
                assert(size==roots.size());
                break;
            } else i++;
        }
        
        for(int i=roots.size(); i>size; i--) {
            roots.pop_back();
        }

        if(!cover) {
            roots.push_back(stmtInFile);
        }
    }

    void clear() {
        roots.clear();
    }

    void add(const Stmt* s, SourceRange rangeInFile) {
        add(StmtInFile(s,rangeInFile));
    }

    bool contain(const Stmt* stmt) {
        for(auto s : roots) {
            if(s.statement==stmt) return true;
        }
        return false;
    }

    const std::vector<StmtInFile>& operator()() {
        return roots;
    }
};

class MatchHandler : public MatchFinder::MatchCallback
{
protected:
    std::map<std::string, Replacements> &ReplaceMap;
protected:
    NonOverlappedStmts nonOverlappedStmts;
    void addAsNonOverlappedStmt(const Stmt *stmt, const SourceRange& range)
    {
        nonOverlappedStmts.add(StmtInFile(stmt, range));
    }
    void addAsNonOverlappedStmt(const Stmt *stmt, const SourceManager* manager)
    {
        nonOverlappedStmts.add(StmtInFile(stmt, manager));
    }
    bool isNonOverlappedStmt(const Stmt *stmt)
    {
        return nonOverlappedStmts.contain(stmt);
    }
public:
    MatchHandler(std::map<std::string, Replacements> &r) : ReplaceMap(r) {}

    static std::string print(const Stmt *stmt, PrinterHelper *helper = NULL)
    {
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
            while (decls != decl->decl_end())
            {
                VarDecl *vd = (VarDecl *)(*decls);
                out << vd->getType().getAsString()
                    << " "
                    << vd->getNameAsString();

                if (vd->hasInit())
                {
                    out << "=" << MatchHandler::print(vd->getInit());
                }
                out << ";" << std::endl;
                decls++;
            }
            out.flush();
            return out.str();
        }
    }
};

class RecursiveRewriteMatchHandler : public MatchHandler
{
protected:
    std::vector<ExpressionExtractionRequest> extractionRequests;
public:
    RecursiveRewriteMatchHandler(std::map<std::string, Replacements> &r) : MatchHandler(r) {}

    virtual void run(const MatchFinder::MatchResult &Result)
    {
        const Expr *actual = Result.Nodes.getNodeAs<Expr>("expression"); // the expression that is going to be extracted out
        const Stmt *stmt = Result.Nodes.getNodeAs<Stmt>("statement");    // the statement that contains the expression
        QualType type = actual->getType();
        extractionRequests.push_back(ExpressionExtractionRequest(actual, type, stmt, Result.SourceManager));
        
        addAsNonOverlappedStmt(actual, extractionRequests.back().actualExpressionRange);
    }

    virtual void onEndOfTranslationUnit()
    {
        ExtractionPrinterHelper helper;
        std::sort(extractionRequests.begin(), extractionRequests.end());
        auto unique = std::unique(extractionRequests.begin(), extractionRequests.end());
        extractionRequests.erase(unique, extractionRequests.end());

        std::ostringstream out;
        const ExpressionExtractionRequest *lastExtractionRequest = NULL;

        std::string filename;
        Replacements *Replace;
        auto requestIterator = extractionRequests.begin();

        while (lastExtractionRequest != NULL || requestIterator != extractionRequests.end())
        {
            if (Replace == NULL)
            {
                filename = requestIterator->manager->getFilename(requestIterator->statement->getBeginLoc()).str();
                Replace = &ReplaceMap[filename];
            }

            if (lastExtractionRequest != NULL && (requestIterator == extractionRequests.end() || lastExtractionRequest->statement != requestIterator->statement))
            {
                out.flush();
                std::string extractedStmts = out.str();
                convertStatement(Replace, lastExtractionRequest->manager, extractedStmts, lastExtractionRequest->statement, helper);
                lastExtractionRequest = NULL;
                out.str("");
            }
            else
            {
                convertSubExpression(Replace, *requestIterator, helper);
                if (isNonOverlappedStmt(requestIterator->expression))
                {
                    convertRootExpression(Replace, requestIterator->manager, requestIterator->expression, helper);
                }
                lastExtractionRequest = &*requestIterator;
                requestIterator++;
            }
        }
        extractionRequests.clear();
        nonOverlappedStmts.clear();
    }
    virtual void convertStatement(Replacements *Replace, const SourceManager *manager, const std::string &extracted, const Stmt *statement, ExtractionPrinterHelper &helper)
    {
        Replacement Rep1 = ReplacementBuilder::create(*manager, statement->getBeginLoc(), 0, extracted);
        llvm::Error err1 = Replace->add(Rep1);
    }
    virtual void convertRootExpression(Replacements *Replace, const SourceManager *manager, const Stmt *rootExpression, ExtractionPrinterHelper &helper)
    {
        std::string stmt_Str = print(rootExpression, &helper);
        Replacement Rep2 = ReplacementBuilder::create(*manager, rootExpression, stmt_Str);
        llvm::Error err2 = Replace->add(Rep2);
    }
    virtual void convertSubExpression(Replacements *Replace, const ExpressionExtractionRequest &request, ExtractionPrinterHelper &helper) = 0;
};

class CodeTransformationTool
{
protected:
    CommonOptionsParser Options;
    RefactoringTool Tool;
    MatchFinder Finder;
    std::vector<MatchHandler *> handlerVector;
    std::string toolName;

public:
    CodeTransformationTool(int argc, const char **argv, llvm::cl::OptionCategory category, std::string name = "Unnamed Tool") : Tool(Options.getCompilations(), Options.getSourcePathList()),
                                                                                                                                Options(argc, argv, category),
                                                                                                                                Finder(),
                                                                                                                                toolName(name)
    {
    }

    ~CodeTransformationTool()
    {
        int size = handlerVector.size();
        for (int i = 0; i < size; i++)
        {
            delete handlerVector[i];
        }
    }

    std::map<std::string, Replacements> &GetReplacements()
    {
        return Tool.getReplacements();
    }

    template <typename ASTMatcherType, typename HandlerType>
    void add(const ASTMatcherType &matcher)
    {
        HandlerType *h = new HandlerType(Tool.getReplacements());
        handlerVector.push_back(h);
        add(matcher, *h);
    }

    template <typename ASTMatcherType, typename HandlerType>
    void add(const ASTMatcherType &matcher, HandlerType &handler)
    {
        // TK_IgnoreImplicitCastsAndParentheses
        Finder.addMatcher(traverse(TK_IgnoreUnlessSpelledInSource, matcher), &handler);
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
            out << "// Rewrite by " << toolName << "\n";
            I->second.write(out);
            out.flush();
            out.close();
        }
        return 0;
    }
};

#endif