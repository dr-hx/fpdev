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


auto record = recordDecl(
    isExpansionInMainFile(), 
    forEach(
        fieldDecl(
            anyOf(
                hasType(realFloatingPointType()),
                hasType(constantArrayType(hasElementType(realFloatingPointType())))
            )
        ).bind("field")
    )
).bind("record"); // scan record and field


// we may ignore the case that when a record inherits another, but it does call super constructors
auto cons = cxxConstructorDecl(isExpansionInMainFile(), hasBody(compoundStmt().bind("body")), hasDeclContext(recordDecl().bind("record"))).bind("cons"); // scan constructors


auto anyRecord = recordDecl(isExpansionInMainFile()).bind("record");

struct RecordInfo
{
    struct ConstructorInfo
    {
        const CXXConstructorDecl* consDecl;
        const CompoundStmt* body;
        bool delegation;
    };
    bool interesting;
    const RecordDecl* record;
    std::map<const CXXConstructorDecl*, ConstructorInfo> consInfoMap;
    std::vector<const FieldDecl*> fields;

    ConstructorInfo& operator[](const CXXConstructorDecl* r)
    {
        return consInfoMap[r];
    }
};

struct RecordMap
{
    std::map<const RecordDecl*, RecordInfo> recordInfoMap;

    RecordInfo& operator[](const RecordDecl* r)
    {
        return recordInfoMap[r];
    }

    void dump()
    {
        llvm::outs() << "dump begin...\n";
        for(auto p : recordInfoMap)
        {
            llvm::outs() << "dump pair\t";
            p.first->dump();
            // llvm::outs() << p.first->getID() <<"\n";
        }
    }
};

class RecordInfoCollector : public MatchFinder::MatchCallback
{
protected:
    RecordMap* recordMap;
    const std::set<std::string> *targets;
public:
    RecordInfoCollector(RecordMap *map) : recordMap(map), targets(NULL) {}
    void setTargets(const std::set<std::string> *t)
    {
        targets = t;
    }
    virtual void run(const MatchFinder::MatchResult &Result)
    {
        auto field = Result.Nodes.getNodeAs<FieldDecl>("field");
        auto cons = Result.Nodes.getNodeAs<CXXConstructorDecl>("cons");
        auto record = Result.Nodes.getNodeAs<RecordDecl>("record");

        llvm::outs() << "record " << record->getID() <<"\n";

        if(field!=NULL)
        {
            auto &recordInfo = (*recordMap)[record];
            recordInfo.record = record;
            recordInfo.interesting = true;
            recordInfo.fields.push_back(field);
            return;
        }

        if(cons!=NULL)
        {
            auto body = Result.Nodes.getNodeAs<CompoundStmt>("body");
            auto &consInfo = (*recordMap)[record][cons];
            consInfo.consDecl = cons;
            consInfo.body = body;
            consInfo.delegation = cons->isDelegatingConstructor();
            return;
        }
    } 
};

class FpStructInstrumentation : public MatchHandler
{
protected:
    RecordMap* recordMap;
    const std::set<std::string> *targets;
public:
    FpStructInstrumentation(std::map<std::string, Replacements> &r, RecordMap* m) : MatchHandler(r), recordMap(m)
    {}

    virtual void run(const MatchFinder::MatchResult &Result)
    {
        auto record = Result.Nodes.getNodeAs<RecordDecl>("record");
        
        recordMap->dump();

        auto it = recordMap->recordInfoMap.find(record);
        if(it!=recordMap->recordInfoMap.end())
        {
            llvm::outs() << "in\n";
            auto &rec = it->second;
            auto &consMap = it->second.consInfoMap;
            bool hasCopy = false;
            bool hasMove = false;

            for(auto cInfo : consMap)
            {
                hasCopy = cInfo.second.consDecl->isCopyConstructor();
                hasMove = cInfo.second.consDecl->isMoveConstructor();
                if(cInfo.second.delegation) continue; // skip delegation

                // for each fp field, generate a def at the beginning of body
                auto body = cInfo.second.body;

                std::string code;
                llvm::raw_string_ostream stream(code);
                stream << "{\n";
                for(auto field : rec.fields)
                {
                    auto type = field->getType();
                    if(type.getTypePtrOrNull()->isConstantArrayType())
                    {
                        auto arrayType = (const ConstantArrayType*) type.getTypePtrOrNull();
                        auto size = arrayType->getSize();
                        stream << "ARRDEF(" << field->getNameAsString() << ", ";
                        size.print(stream, false);
                        stream << ");\n";
                    }
                    else
                    {
                        stream << "DEF(" << field->getNameAsString() << ");\n";
                    }
                }
                stream.flush();
                auto Rep = ReplacementBuilder::create(*Result.SourceManager, body->getBeginLoc(), 1, code);
                addReplacement(Rep);
            }
        }
    }
};

int main(int argc, const char **argv)
{
    CommonOptionsParser Options(argc, argv, ScDebugTool);
    RecordMap recordMap;

    CodeAnalysisTool recordCollector(Options);
    RecordInfoCollector collector(&recordMap);
    recordCollector.add(record, collector);
    recordCollector.add(cons, collector);
    recordCollector.run();

    CodeTransformationTool tool(Options, "Instrumentation: FpStruct");
    FpStructInstrumentation handler(tool.GetReplacements(), &recordMap);
    tool.add(anyRecord, handler);
    tool.run();

    return 0;
}