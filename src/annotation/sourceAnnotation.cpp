#include <string>
#include <set>
#include <fstream>
#include "../util/random.h"
#include "../transformer/transformer.hpp"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolingSampleCategory("ScDebug Tool");

auto target = compoundStmt(isExpansionInMainFile(), forEach(stmt(unless(compoundStmt())).bind("stmt")));

class CollectStmtPatHandler : public MatchHandler
{
public:
    std::vector<std::string> codePaths;
public:
    CollectStmtPatHandler(std::map<std::string, Replacements> &r) : MatchHandler(r) {}

    const SourceManager *pManager;
    llvm::StringRef filename;
    
    virtual void run(const MatchFinder::MatchResult &Result)
    {
        pManager = Result.SourceManager;
        
        int counter = codePaths.size();
        const SourceManager *Manager = Result.SourceManager;
        const Stmt *stmt = Result.Nodes.getNodeAs<Stmt>("stmt");
        SourceLocation loc = Manager->getFileLoc(stmt->getBeginLoc());
        if(loc.isInvalid()) return;

        codePaths.push_back(loc.printToString(*Manager));
        std::ostringstream stream;
        stream << "PC(" << counter <<");\n";
        stream.flush();
        Replacement rep = ReplacementBuilder::create(*Manager, loc, 0, stream.str());
        addReplacement(rep);
        filename = pManager->getFilename(loc);
    }

    virtual void onStartOfTranslationUnit()
    {
        pManager = NULL;
    }
    virtual void onEndOfTranslationUnit()
    {
        if(pManager)
        {
            std::string header;
            llvm::raw_string_ostream header_stream(header);
            header_stream << "#include \"RunConfigure.h\"  // you must copy RunConfigure.h into your source path\n"
                    << "#include <real/ShadowExecution.hpp> // you must put ShadowExecution.hpp into a library path\n";
            header_stream.flush();
            Replacement rep = ReplacementBuilder::create(filename, 0, 0, header);
            addReplacement(rep);
        }
    }
};

int main(int argc, const char **argv)
{
    CommonOptionsParser Options(argc, argv, ToolingSampleCategory);
    CodeTransformationTool tool(Options, "Source annotation: annotate PC and generate source path");
    CollectStmtPatHandler handler(tool.GetReplacements());
    tool.add(target,handler);
    tool.run();
    
    std::ofstream outfile;
    outfile.open("RunConfigure.h", std::ios::out | std::ios::trunc);
    
    outfile << "#ifndef USER_REAL_CONFIGURE\n";
    outfile << "#define USER_REAL_CONFIGURE\n";

    outfile << "#define PC_COUNT "<<handler.codePaths.size()<<"\n";
    outfile << "#define STRINGLIZE(str) #str\n";

    outfile <<"static const char *PATH_STRINGS[] = {";
    for(int id = 0 ; id<handler.codePaths.size() ; id++)
    {
        if(id!=0) outfile << ",\n";
        else outfile <<"\n";
        outfile <<"STRINGLIZE("<<handler.codePaths[id]<<")";
    }
    outfile <<"};\n";


    outfile << "#endif\n";
    outfile.flush();
    outfile.close();
    return 0;
}