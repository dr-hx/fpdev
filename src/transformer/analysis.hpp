#ifndef ANALYSIS_HPP
#define ANALYSIS_HPP
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

namespace ustb
{
    namespace analysis
    {
        struct RangeInFile
        {
            clang::SourceRange rangeInFile;
            std::vector<RangeInFile*> subRanges;
            RangeInFile* parentRange;

            RangeInFile(const clang::SourceRange &r) : rangeInFile(r), parentRange(NULL) {}
            virtual ~RangeInFile() {
                for(auto r : subRanges) {
                    delete r;
                }
            }
            virtual RangeInFile* addSubRange(RangeInFile* s, bool rootsOnly=false) {
                auto size = subRanges.size();
                for(decltype(size) i =0; i<size; i++) {
                    if(s->cover(*subRanges[i])) {
                        if(size != i+1) {
                            RangeInFile* ei = subRanges[i];
                            subRanges[i] = subRanges[size-1];
                            subRanges[size-1] = ei;
                        }
                        size --;
                    } else if(subRanges[i]->cover(*s)) {
                        if(rootsOnly) return NULL;
                        else return subRanges[i]->addSubRange(s, rootsOnly);
                    }
                }
                while(size < subRanges.size()) {
                    auto last = subRanges.back();
                    if(!rootsOnly) s->addSubRange(last, true); //short-cut
                    subRanges.pop_back();
                }
                subRanges.push_back(s);
                s->parentRange = this;
                return this;
            }
            bool cover(const RangeInFile &r)
            {
                return rangeInFile.fullyContains(r.rangeInFile);
            }
            bool operator<(const RangeInFile &r)
            {
                return rangeInFile.getBegin() < r.rangeInFile.getBegin();
            }
            bool operator==(const RangeInFile &r)
            {
                return rangeInFile == r.rangeInFile;
            }

            virtual void unique() {
                auto u = std::unique(subRanges.begin(),subRanges.end());
                subRanges.erase(u, subRanges.end());
                
                for(auto s : subRanges) {
                    s->unique();
                }
            }

            virtual void sort() {
                std::sort(subRanges.begin(), subRanges.end());
                for(auto s : subRanges) {
                    s->sort();
                }
            }

            virtual bool isVirtual() {return false;}
        };

        struct FlattenRanges : public RangeInFile{
            FlattenRanges():RangeInFile(clang::SourceRange()) {}

            RangeInFile* add(RangeInFile* range) {
                subRanges.push_back(range);
                return this;
            }

            virtual RangeInFile* addSubRange(RangeInFile* s, bool rootsOnly=false) {
                return add(s);
            }

            virtual bool isVirtual() {return true;}
        };

        struct RootBasedRanges : public RangeInFile {
            std::vector<RangeInFile*> allRanges;
            RootBasedRanges():RangeInFile(clang::SourceRange()) {}
            virtual ~RootBasedRanges() {
                for(auto r : allRanges) {
                    delete r;
                }
                subRanges.clear();
            }

            RangeInFile* add(RangeInFile* range) {
                allRanges.push_back(range);
                return addSubRange(range, true);
            }

            virtual void unique() {
                auto u = std::unique(allRanges.begin(),allRanges.end());
                allRanges.erase(u, allRanges.end());
                RangeInFile::unique();
            }

            virtual void sort() {
                std::sort(allRanges.begin(), allRanges.end());
                RangeInFile::sort();
            }

            virtual RangeInFile* addSubRange(RangeInFile* s, bool rootsOnly=false) {
                assert(rootsOnly);
                return RangeInFile::addSubRange(s,true);
            }

            virtual bool isVirtual() {return true;}
        };

        struct TreeBasedRanges : public RangeInFile {
            TreeBasedRanges():RangeInFile(clang::SourceRange()) {}
            
            RangeInFile* add(RangeInFile* range) {
                return addSubRange(range, false);
            }

            virtual RangeInFile* addSubRange(RangeInFile* s, bool rootsOnly=false) {
                assert(!rootsOnly);
                return RangeInFile::addSubRange(s,true);
            }

            virtual bool isVirtual() {return true;}
        };
        
    }; // namespace transformation
};     // namespace ustb

#endif