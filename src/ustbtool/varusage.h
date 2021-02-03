#ifndef VAR_USAGE_H
#define VAR_USAGE_H

#include <unordered_map>
#include <set>

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
#include <llvm/Option/OptTable.h>
#include <llvm/Support/TargetSelect.h>

/**
 * Variable usage analyzes the usage of a variable of a certain type.
 * Given a specific type, the analyzer tracks all the variables of this type.
 */

#define DEBUG


using namespace clang;
using namespace clang::ast_matchers;

namespace ustb
{
    namespace clang
    {
        namespace varusage
        {
            // SECTION matchers
            template <typename TM>
            static auto makeSimpleVarDefMatcher(const TM &t, const char* matchID="var-def")
            {
                return traverse(TK_IgnoreUnlessSpelledInSource, 
                    varDecl(isExpansionInMainFile(), 
                        hasType(t)).bind(matchID)
                );
            }
            template <typename TM>
            static auto makeArrayVarDefMatcher(const TM &t, const char* matchID="var-def")
            {
                return traverse(TK_IgnoreUnlessSpelledInSource, 
                    varDecl(isExpansionInMainFile(), 
                        hasType(constantArrayType(hasElementType(t)))).bind(matchID)
                );
            }
            template <typename TM>
            static auto makePointerVarDefMatcher(const TM &t, const char* matchID="var-def")
            {
                return traverse(TK_IgnoreUnlessSpelledInSource, 
                    varDecl(isExpansionInMainFile(), 
                        hasType(pointerType(pointee(t)))).bind(matchID)
                );
            }

            template <typename TM>
            static void buildDefMatcher(const TM &t, const char* matchID, MatchFinder& finder, MatchFinder::MatchCallback& handler)
            {
                finder.addMatcher(ustb::clang::varusage::makeSimpleVarDefMatcher(t, matchID), &handler);
                finder.addMatcher(ustb::clang::varusage::makePointerVarDefMatcher(t, matchID), &handler);
                finder.addMatcher(ustb::clang::varusage::makeArrayVarDefMatcher(t, matchID), &handler);
            }

            
            // SECTION handlers
            static auto makeGetAddressOfSimpleVar(const char* matchID="var-ref")
            {
                return traverse(TK_IgnoreUnlessSpelledInSource, 
                    unaryOperator(isExpansionInMainFile(), 
                        hasOperatorName("&"), 
                        hasUnaryOperand(declRefExpr(to(varDecl().bind(matchID)))))
                );
            }
            static auto makeGetAddressOfArrayElem(const char* matchID="var-ref")
            {
                return traverse(TK_IgnoreUnlessSpelledInSource, 
                    unaryOperator(isExpansionInMainFile(), 
                        hasOperatorName("&"), 
                        hasUnaryOperand(
                            arraySubscriptExpr(
                                hasBase(declRefExpr(to(varDecl().bind(matchID))))
                            )
                        )
                ));
            }
            static auto makeGetAddressOfPointerStar(const char* matchID="var-ref")
            {
                return traverse(TK_IgnoreUnlessSpelledInSource, 
                    unaryOperator(isExpansionInMainFile(), 
                        hasOperatorName("&"), 
                        hasUnaryOperand(
                            unaryOperator(
                                hasOperatorName("*"),
                                hasUnaryOperand(declRefExpr(to(varDecl().bind(matchID))))
                            )
                        )
                ));
            }

            static auto makePointerToPointerAssignment(const char* matchID="var-ref")
            {
                return traverse(TK_IgnoreUnlessSpelledInSource, 
                    binaryOperator(isExpansionInMainFile(),
                        isAssignmentOperator(),
                        hasRHS(
                            declRefExpr(to(varDecl(hasType(type(anyOf(pointerType(), arrayType())))).bind(matchID)))
                        )
                    )
                );
            }

            static auto makePointerToPointerInit(const char* matchID="var-ref")
            {
                return traverse(TK_IgnoreUnlessSpelledInSource, 
                    varDecl(isExpansionInMainFile(),
                        hasInitializer(
                            declRefExpr(to(varDecl(hasType(type(anyOf(pointerType(), arrayType())))).bind(matchID)))
                        )
                    )
                );
            }

            static auto makeVarToReferenceInit(const char* matchID="var-ref")
            {
                return traverse(TK_IgnoreUnlessSpelledInSource, 
                    varDecl(isExpansionInMainFile(),
                        hasType(referenceType()),
                        hasInitializer(
                            declRefExpr(to(varDecl().bind(matchID)))
                        )
                    )
                );
            }

            static auto makeArrElemToReferenceInit(const char* matchID="var-ref")
            {
                return traverse(TK_IgnoreUnlessSpelledInSource, 
                    varDecl(isExpansionInMainFile(),
                        hasType(referenceType()),
                        hasInitializer(
                            arraySubscriptExpr(
                                hasBase(declRefExpr(to(varDecl().bind(matchID))))
                            )
                        )
                    )
                );
            }

            static auto makePointerStarToReferenceInit(const char* matchID="var-ref")
            {
                return traverse(TK_IgnoreUnlessSpelledInSource, 
                    varDecl(isExpansionInMainFile(),
                        hasType(referenceType()),
                        hasInitializer(
                            unaryOperator(
                                hasOperatorName("*"),
                                hasUnaryOperand(declRefExpr(to(varDecl().bind(matchID))))
                            )
                        )
                    )
                );
            }

            static auto makePointerToPointerParm(const char* matchID="var-ref")
            {
                return traverse(TK_IgnoreUnlessSpelledInSource, 
                    callExpr(isExpansionInMainFile(),
                        forEachArgumentWithParam(
                            declRefExpr(to(varDecl().bind(matchID))),
                            parmVarDecl(hasType(pointerType()))
                        )
                    )
                );
            }

            static auto makeVarToRefParm(const char* matchID="var-ref")
            {
                return traverse(TK_IgnoreUnlessSpelledInSource, 
                    callExpr(isExpansionInMainFile(),
                        forEachArgumentWithParam(
                            declRefExpr(to(varDecl().bind(matchID))),
                            parmVarDecl(hasType(referenceType()))
                        )
                    )
                );
            }

            static auto makeArrElmToRefParm(const char* matchID="var-ref")
            {
                return traverse(TK_IgnoreUnlessSpelledInSource, 
                    callExpr(isExpansionInMainFile(),
                        forEachArgumentWithParam(
                            arraySubscriptExpr(
                                hasBase(declRefExpr(to(varDecl().bind(matchID))))
                            ),
                            parmVarDecl(hasType(referenceType()))
                        )
                    )
                );
            }

            static auto makePointerStarToRefParm(const char* matchID="var-ref")
            {
                return traverse(TK_IgnoreUnlessSpelledInSource, 
                    callExpr(isExpansionInMainFile(),
                        forEachArgumentWithParam(
                            unaryOperator(
                                hasOperatorName("*"),
                                hasUnaryOperand(declRefExpr(to(varDecl().bind(matchID))))
                            ),
                            parmVarDecl(hasType(referenceType()))
                        )
                    )
                );
            }

            static void buildRefMatcher(const char* matchID, MatchFinder& finder, MatchFinder::MatchCallback& handler)
            {
                finder.addMatcher(ustb::clang::varusage::makeGetAddressOfSimpleVar(matchID), &handler);
                finder.addMatcher(ustb::clang::varusage::makeGetAddressOfArrayElem(matchID), &handler);
                finder.addMatcher(ustb::clang::varusage::makeGetAddressOfPointerStar(matchID), &handler);
                
                finder.addMatcher(ustb::clang::varusage::makePointerToPointerAssignment(matchID), &handler);
                finder.addMatcher(ustb::clang::varusage::makePointerToPointerInit(matchID), &handler);

                finder.addMatcher(ustb::clang::varusage::makeVarToReferenceInit(matchID), &handler);
                finder.addMatcher(ustb::clang::varusage::makePointerStarToReferenceInit(matchID), &handler);
                finder.addMatcher(ustb::clang::varusage::makeArrElemToReferenceInit(matchID), &handler);

                finder.addMatcher(ustb::clang::varusage::makeVarToRefParm(matchID), &handler);
                finder.addMatcher(ustb::clang::varusage::makeArrElmToRefParm(matchID), &handler);
                finder.addMatcher(ustb::clang::varusage::makePointerStarToRefParm(matchID), &handler);
                finder.addMatcher(ustb::clang::varusage::makePointerToPointerParm(matchID), &handler);
            }
            

            struct VarDef
            {
                enum Kind
                {
                    SIMPLE,
                    ARRAY,
                    POINTER, // dyn arr
                    PARM_SIMPLE,
                    PARM_ARRAY
                };
                uint64_t varDeclID;
                Kind kind;
                size_t size;
                SourceRange declRange;
            };

            class IVarReferenceTable
            {
            public:
                virtual void def(const VarDef &def) = 0;
                virtual void referenced(uint64_t varDeclID) = 0;
                virtual bool isTracked(uint64_t id) = 0;
                virtual bool isEscaped(uint64_t id) = 0;
                virtual void clear() = 0;
                virtual ~IVarReferenceTable() {}

                virtual void forEachVarDef(std::function<void(VarDef& x)> action) = 0;

            };

            struct VarReferenceTable : public IVarReferenceTable
            {
                std::unordered_map<uint64_t, VarDef> varDefs;
                std::set<uint64_t> varReferenced;

                virtual bool isTracked(uint64_t id)
                {
                    auto it = varDefs.find(id);
                    return it==varDefs.end();
                }

                virtual void forEachVarDef(std::function<void(VarDef& x)> action)
                {
                    auto it = varDefs.begin();
                    while(it!=varDefs.end())
                    {
                        action(it->second);
                        it++;
                    }
                }

                virtual bool isEscaped(uint64_t id)
                {
                    return (varReferenced.count(id)!=0);
                }
                virtual void clear()
                {
                    varDefs.clear();
                    varReferenced.clear();
                }

                virtual void def(const VarDef &def)
                {
                    varDefs[def.varDeclID] = def;
                }
                virtual void referenced(uint64_t varDeclID)
                {
                    varReferenced.insert(varDeclID);
                }
            };

            class VarDefRefHandler : public MatchFinder::MatchCallback
            {
            private:
                IVarReferenceTable *table;
                const char* defMatchID;
                const char* refMatchID;

            public:
                VarDefRefHandler(IVarReferenceTable* t, const char* d, const char* r) : table(t), defMatchID(d), refMatchID(r) {}

                virtual void run(const MatchFinder::MatchResult &result)
                {
                    const VarDecl* vd = result.Nodes.getNodeAs<VarDecl>(defMatchID);
                    if(vd!=nullptr)
                    {
                        uint64_t varDeclID = vd->getID();
                        auto type = vd->getType();
                        auto type_ptr = type.getTypePtrOrNull();
                        
                        VarDef::Kind kind = VarDef::Kind::SIMPLE;
                        size_t size = 1;

                        if(isa<ParmVarDecl>(vd))
                        {
                            kind = VarDef::Kind::PARM_SIMPLE;
                            if(isa<ConstantArrayType>(type_ptr))
                            {
                                kind = VarDef::Kind::PARM_ARRAY;
                                size = ((const ConstantArrayType*)type_ptr)->getSize().getZExtValue();
                            }
                            else if(isa<PointerType>(type_ptr))
                            {
                                return;
                            }
                        }
                        else
                        {
                            if(isa<ConstantArrayType>(type_ptr))
                            {
                                kind = VarDef::Kind::ARRAY;
                                size = ((const ConstantArrayType*)type_ptr)->getSize().getZExtValue();
                            }
                            else if(isa<PointerType>(type_ptr))
                            {
                                kind = VarDef::Kind::POINTER;
                                size = 0;
                            }
                        }

                        SourceRange rangeInFile = SourceRange(
                            result.SourceManager->getFileLoc(vd->getBeginLoc()), 
                            result.SourceManager->getFileLoc(vd->getEndLoc()));

                        VarDef def = {varDeclID, kind, size, rangeInFile};
                        table->def(def);

#ifdef DEBUG
                        llvm::outs()<<varDeclID<<" : ";
                        def.declRange.dump(*result.SourceManager);
#endif
                        return;
                    }

                    const VarDecl* vr = result.Nodes.getNodeAs<VarDecl>(refMatchID);
                    if(vr!=nullptr)
                    {
                        uint64_t id = vr->getID();
                        table->referenced(id);
                        return;
                    }
                }
            

                virtual void onEndOfTranslationUnit()
                {
#ifdef DEBUG
                    table->forEachVarDef(
                        [&](VarDef& v){
                            llvm::outs() << v.varDeclID << " is ";
                            if(table->isEscaped(v.varDeclID))
                            {
                                llvm::outs() << "escaped\n";
                            }
                            else
                            {
                                llvm::outs() << "local\n";
                            }
                        }
                    );
#endif
                }
            };
        }; // namespace varusage
    };     // namespace clang
};         // namespace ustb

#endif