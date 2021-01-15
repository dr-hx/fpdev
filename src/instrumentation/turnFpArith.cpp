#include <string>
#include <set>
#include "util/random.h"
#include "transformer/transformer.hpp"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ScDebugTool("ScDebug Instrumentation Tool");

// Turn FpArith
// 1. collect local fp variables -> variables that are not passed by reference nor pointer. 
//      local variables have static mappings to reals
// 2. var ref => varMap[&address]
// 3. fp operators => real operators
// 4. structs? classes? unions? => constructors



int main(int argc, const char **argv)
{
    CodeTransformationTool tool(argc, argv, ScDebugTool);

    // tool.add<decltype(target), SingleStmtPatHandler>(target);

    tool.run();

    return 0;
}