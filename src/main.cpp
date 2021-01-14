#include <stdlib.h>
#include <stdio.h>
#include <iostream>


#include "real/Real.hpp"
#include "transformer/transformer.hpp"

#include <string>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolingSampleCategory("Tooling Sample");

class IfStmtHandler : public MatchFinder::MatchCallback {
public:
  IfStmtHandler(std::map<std::string, Replacements> &r) : ReplaceMap(r) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
    // The matched 'if' statement was bound to 'ifStmt'.
    if (const IfStmt *IfS = Result.Nodes.getNodeAs<clang::IfStmt>("ifStmt")) {
        auto filename = Result.SourceManager->getFilename(IfS->getBeginLoc());
      Replacements &Replace = ReplaceMap[filename.str()];

      const Stmt *Then = IfS->getThen();
      Replacement Rep(*(Result.SourceManager), Then->getBeginLoc(), 0,
                      "// the 'if' part\n");
      Replace.add(Rep);

      Rep = Replacement(*(Result.SourceManager), Then->getBeginLoc().getLocWithOffset(-1), 0,
                      "// the 'if' part again\n");
      Replace.add(Rep);

      if (const Stmt *Else = IfS->getElse()) {
        Replacement Rep(*(Result.SourceManager), Else->getBeginLoc(), 0,
                        "// the 'else' part\n");
        Replace.add(Rep);
      }
    }
  }

private:
  std::map<std::string, Replacements> &ReplaceMap;
};

int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, ToolingSampleCategory);
  RefactoringTool Tool(op.getCompilations(), op.getSourcePathList());

  // Set up AST matcher callbacks.
  IfStmtHandler HandlerForIf(Tool.getReplacements());

  MatchFinder Finder;
  Finder.addMatcher(ifStmt().bind("ifStmt"), &HandlerForIf);

  // Run the tool and collect a list of replacements. We could call runAndSave,
  // which would destructively overwrite the files with their new contents.
  // However, for demonstration purposes it's interesting to print out the
  // would-be contents of the rewritten files instead of actually rewriting
  // them.
  if (int Result = Tool.run(newFrontendActionFactory(&Finder).get())) {
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
       I != E; ++I) {
    const FileEntry *Entry = Sources.getFileEntryForID(I->first);
    llvm::outs() << "Rewrite buffer for file: " << Entry->getName() << "\n";
    I->second.write(llvm::outs());
  }

  return 0;
}
