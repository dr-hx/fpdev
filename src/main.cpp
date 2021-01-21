#include <stdlib.h>
#include <stdio.h>
#include <iostream>


#include "real/Real.hpp"
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

#include <string>

#include "real/ShadowExecution.hpp"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolingSampleCategory("ScDebug Tool");

class IfStmtHandler : public MatchFinder::MatchCallback {
public:
  IfStmtHandler(std::map<std::string, Replacements> &r) : ReplaceMap(r) {}

  virtual void onStartOfTranslationUnit() {
    printf("hello\n");
  }

  virtual void run(const MatchFinder::MatchResult &Result) {
    // The matched 'if' statement was bound to 'ifStmt'.
    const Stmt *stmt;
    stmt = Result.Nodes.getNodeAs<Stmt>("ifStmt1");
    if(stmt!=NULL) {
      auto fn = Result.SourceManager->getFilename(stmt->getBeginLoc()).str();
      stmt->getSourceRange().dump(*Result.SourceManager);
      llvm::outs() << stmt->getBeginLoc().isFileID() << "\n" << stmt->getBeginLoc().isValid() << "\n";
      // auto Rep = Replacement(*Result.SourceManager, stmt->getBeginLoc(), 0, "hello");
      // auto err = ReplaceMap[fn].add(Rep);
      // if(err.success()) {
      //   llvm::outs() << "error happened\n";
      // }
    }

    stmt = Result.Nodes.getNodeAs<Stmt>("switchStmt2");
    if(stmt!=NULL) {
      llvm::outs()<<"switchStmt\t2: \n";
      stmt->dumpColor();
    }
  }

private:
  std::map<std::string, Replacements> &ReplaceMap;
};

int main(int argc, const char **argv) {
  double x,y,z;
  SVal &r = DEF(x);
  SVal &s = DEF(y);
  SVal &t = DEF(z);


  r = 1; s = 2;

  t = r + s;

  int i;

  mpfr_printf("%Re\n", t.shadow->shadowValue);

  // CommonOptionsParser op(argc, argv, ToolingSampleCategory);
  // RefactoringTool Tool(op.getCompilations(), op.getSourcePathList());

  // for(auto fn : op.getSourcePathList()) {
  //   auto f = Tool.getFiles().getFile(fn);
  //   printf("%s\n", f.get()->tryGetRealPathName().str().c_str());
  // }

  // // Set up AST matcher callbacks.
  // IfStmtHandler HandlerForIf(Tool.getReplacements());

  // MatchFinder Finder;
  // Finder.addMatcher(ifStmt().bind("ifStmt1"), &HandlerForIf);
  // Finder.addMatcher(switchStmt().bind("switchStmt2"), &HandlerForIf);

  // // Run the tool and collect a list of replacements. We could call runAndSave,
  // // which would destructively overwrite the files with their new contents.
  // // However, for demonstration purposes it's interesting to print out the
  // // would-be contents of the rewritten files instead of actually rewriting
  // // them.
  // if (int Result = Tool.runAndSave(newFrontendActionFactory(&Finder).get())) {
  //   return Result;
  // }

  // // We need a SourceManager to set up the Rewriter.
  // IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
  // DiagnosticsEngine Diagnostics(
  //     IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs()), &*DiagOpts,
  //     new TextDiagnosticPrinter(llvm::errs(), &*DiagOpts), true);
  // SourceManager Sources(Diagnostics, Tool.getFiles());

  // // Apply all replacements to a rewriter.
  // Rewriter Rewrite(Sources, LangOptions());
  // Tool.applyAllReplacements(Rewrite);

  // // Query the rewriter for all the files it has rewritten, dumping their new
  // // contents to stdout.
  // for (Rewriter::buffer_iterator I = Rewrite.buffer_begin(),
  //                                E = Rewrite.buffer_end();
  //      I != E; ++I) {
  //   const FileEntry *Entry = Sources.getFileEntryForID(I->first);
  //   llvm::outs() << "Rewrite buffer for file: " << Entry->getName() << "\n";
  //   I->second.write(llvm::outs());
  // }

  return 0;
}
