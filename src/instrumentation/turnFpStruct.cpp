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

auto des = cxxDestructorDecl(isExpansionInMainFile(), hasBody(compoundStmt().bind("body")), hasDeclContext(recordDecl().bind("record"))).bind("des"); 

auto anyRecord = recordDecl(isExpansionInMainFile()).bind("record");

struct RecordInfo
{
    struct ConstructorInfo
    {
        uint64_t consDeclID;
        SourceRange bodyRange;
        bool delegation;
        bool isCopy;
        bool isMove;
    };
    struct FieldInfo
    {
        std::string fieldName;
        bool isArray;
        int64_t size;
    };
    bool interesting;
    uint64_t recordID;
    std::map<uint64_t, ConstructorInfo> consInfoMap;
    std::vector<FieldInfo> fields;
    
    SourceRange destructorRange;
    SourceLocation endLoc;

    ConstructorInfo& operator[](const CXXConstructorDecl* r)
    {
        return consInfoMap[r->getID()];
    }

    void addConstructor(const CXXConstructorDecl* r, const CompoundStmt *body, const SourceManager* manager)
    {
        auto &info = consInfoMap[r->getID()];
        info.consDeclID = r->getID();
        info.bodyRange = SourceRange(manager->getFileLoc(body->getBeginLoc()), manager->getFileLoc(body->getEndLoc()));
        info.delegation = r->isDelegatingConstructor();
        info.isCopy = r->isCopyConstructor();
        info.isMove = r->isMoveConstructor();
    }

    void addField(const FieldDecl* field)
    {
        FieldInfo info;
        info.fieldName = field->getNameAsString();
        auto typePtr = field->getType().getTypePtrOrNull();
        info.isArray = typePtr->isConstantArrayType();
        if(info.isArray)
        {
            info.size = ((const ConstantArrayType*) typePtr)->getSize().getSExtValue();
        }
        fields.push_back(info);
    }
};

struct RecordMap
{
    std::map<uint64_t, RecordInfo> recordInfoMap;

    RecordInfo& operator[](const RecordDecl* r)
    {
        return recordInfoMap[r->getID()];
    }

    void dump()
    {
        llvm::outs() << "dump begin...\n";
        for(auto p : recordInfoMap)
        {
            llvm::outs() << "dump pair\t";
            llvm::outs() << p.first << "\n";
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
        auto des = Result.Nodes.getNodeAs<CXXDestructorDecl>("des");
        auto record = Result.Nodes.getNodeAs<RecordDecl>("record");

        if(field!=NULL)
        {
            auto &recordInfo = (*recordMap)[record];
            recordInfo.recordID = record->getID();
            recordInfo.interesting = true;
            recordInfo.endLoc = record->getEndLoc();
            recordInfo.addField(field);
            return;
        }

        if(cons!=NULL)
        {
            auto body = Result.Nodes.getNodeAs<CompoundStmt>("body");
            auto &consInfo = (*recordMap)[record];
            consInfo.addConstructor(cons, body, Result.SourceManager);
            return;
        }

        if(des!=NULL)
        {
            auto body = Result.Nodes.getNodeAs<CompoundStmt>("body");
            auto &consInfo = (*recordMap)[record];
            consInfo.destructorRange = SourceRange(Result.SourceManager->getFileLoc(des->getBeginLoc()), Result.SourceManager->getFileLoc(des->getEndLoc()));
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
        auto it = recordMap->recordInfoMap.find(record->getID());
        if(it!=recordMap->recordInfoMap.end() && it->second.interesting)
        {
            auto &rec = it->second;
            auto &consMap = it->second.consInfoMap;
            bool hasCopy = false;
            bool hasMove = false;

            for(auto cInfo : consMap)
            {
                hasCopy = cInfo.second.isCopy;
                hasMove = cInfo.second.isMove;

                if(cInfo.second.delegation) continue; // skip delegation

                // for each fp field, generate a def at the beginning of body
                auto &bodyRange = cInfo.second.bodyRange;

                std::string code;
                llvm::raw_string_ostream stream(code);
                stream << "{\n";
                for(auto field : rec.fields)
                {
                    if(field.isArray)
                    {
                        stream << "ARRDEF(" << field.fieldName << ", " << field.size <<");\n";
                    }
                    else
                    {
                        stream << "DEF(" << field.fieldName << ");\n";
                    }
                }
                stream.flush();
                auto Rep = ReplacementBuilder::create(*Result.SourceManager, bodyRange.getBegin(), 1, code);
                addReplacement(Rep);
            }

            if(!hasCopy)
            {
                record->getSourceRange().dump(*Result.SourceManager);
                llvm::outs() << "\t No copy constructor\n";
            }
            if(!hasCopy)
            {
                record->getSourceRange().dump(*Result.SourceManager);
                llvm::outs() << "\t No move constructor\n";
            }

            if(it->second.destructorRange.isValid())
            {
                std::string code;
                llvm::raw_string_ostream stream(code);
                stream << "{\n";
                for(auto field : rec.fields)
                {
                    if(field.isArray)
                    {
                        stream << "ARRUNDEF(" << field.fieldName << ", " << field.size <<");\n";
                    }
                    else
                    {
                        stream << "UNDEF(" << field.fieldName << ");\n";
                    }
                }
                stream.flush();
                auto Rep = ReplacementBuilder::create(*Result.SourceManager, it->second.destructorRange.getBegin(), 1, code);
                addReplacement(Rep);
            }
            else
            {
                std::string code;
                llvm::raw_string_ostream stream(code);
                stream << "virtual ~" << record->getNameAsString() << "() { // generated\n";
                for(auto field : rec.fields)
                {
                    if(field.isArray)
                    {
                        stream << "ARRUNDEF(" << field.fieldName << ", " << field.size <<");\n";
                    }
                    else
                    {
                        stream << "UNDEF(" << field.fieldName << ");\n";
                    }
                }
                stream.flush();
                stream << "}\n}";
                auto Rep = ReplacementBuilder::create(*Result.SourceManager, it->second.endLoc, 1, code);
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
    recordCollector.add(des, collector);
    recordCollector.run();

    CodeTransformationTool tool(Options, "Instrumentation: FpStruct");
    FpStructInstrumentation handler(tool.GetReplacements(), &recordMap);
    tool.add(anyRecord, handler);
    tool.run();

    return 0;
}