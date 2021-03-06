vpath %.cpp src src/normalization src/transformer src/instrumentation src/annotation
vpath %h src/util
vpath %hpp src/util src/transformer


MAKE_DIR:="$(shell pwd)"
SRC_DIR:="$(shell pwd)/src"
TEST_BASE=./test
TEST_DERIVED_BASE = ${TEST_BASE}/derived
fn=test.cpp


CC = /usr/bin/clang++
LIB_PATHS = -L/usr/local/opt/llvm/lib

# COMPILER_FLAGS = -std=c++17
# LIBS = -lstdc++ -lm -lgmp -lmpfr ${CLANG_LIBS}

CXXFLAGS := -std=c++17 -fno-rtti -O0 -g
LLVM_BIN_PATH := /usr/local/opt/llvm/bin
LLVM_CXXFLAGS := `$(LLVM_BIN_PATH)/llvm-config --cxxflags`
LLVM_LDFLAGS := `$(LLVM_BIN_PATH)/llvm-config --ldflags --libs --system-libs`

EXTRA_FLAGS := --  -xc++ -I/usr/local/include -I/usr/local/Cellar/llvm/11.0.0_1/include/c++/v1 -I/usr/local/Cellar/llvm/11.0.0_1/lib/clang/11.0.0/include/ -Isrc -Isrc/qd/include  -std=c++17

CLANG_LIBS := \
	-lclangAST \
	-lclangASTMatchers \
	-lclangAnalysis \
	-lclangBasic \
	-lclangDriver \
	-lclangEdit \
	-lclangFrontend \
	-lclangFrontendTool \
	-lclangLex \
	-lclangParse \
	-lclangSema \
	-lclangEdit \
	-lclangRewrite \
	-lclangRewriteFrontend \
	-lclangStaticAnalyzerFrontend \
	-lclangStaticAnalyzerCheckers \
	-lclangStaticAnalyzerCore \
	-lclangCrossTU \
	-lclangIndex \
	-lclangSerialization \
	-lclangToolingCore \
	-lclangTooling \
	-lclangFormat \
	-lclangToolingRefactoring\
	-lclangApplyReplacements\
	-lclangTransformer\
	-lclangToolingInclusions\
	-lgmp\
	-lmpfr\


info:
	@echo ${MAKE_DIR}

clean: 
	rm -rf bin; mkdir bin


pass = passZero passOne passTwo passThree passClean
trans = turnFpArith turnFpStruct
annots = sourceAnnotation

passObjects = $(foreach n, $(pass), bin/$(n))
$(passObjects) : bin/% : src/normalization/%.cpp src/transformer/transformer.hpp
	${CC} $(CXXFLAGS) $(LLVM_CXXFLAGS)  $< $(CLANG_LIBS) $(LLVM_LDFLAGS) -o $@

transObjects = $(foreach n, $(trans), bin/$(n))
$(transObjects) : bin/% : src/instrumentation/%.cpp src/transformer/transformer.hpp src/instrumentation/functionTranslation.hpp
	${CC} $(CXXFLAGS) $(LLVM_CXXFLAGS)  $< $(CLANG_LIBS) $(LLVM_LDFLAGS) -o $@

annotationObjects = $(foreach n, $(annots), bin/$(n))
$(annotationObjects) : bin/% : src/annotation/%.cpp src/transformer/transformer.hpp
	${CC} $(CXXFLAGS) $(LLVM_CXXFLAGS)  $< $(CLANG_LIBS) $(LLVM_LDFLAGS) -o $@

normalization : $(annotationObjects) $(passObjects)
instrumentation : normalization $(transObjects)


testnorm : normalization
	rm -r ${TEST_DERIVED_BASE}; mkdir ${TEST_DERIVED_BASE}; cp ${TEST_BASE}/${fn} ${TEST_DERIVED_BASE}/${fn}
	${LLVM_BIN_PATH}/clang-tidy ${TEST_DERIVED_BASE}/${fn} -fix -checks="readability-braces-around-statements" $(EXTRA_FLAGS)
	for n in $(annotationObjects); \
	do \
		$$n ${TEST_DERIVED_BASE}/${fn} $(EXTRA_FLAGS);\
	done
	cp ./RunConfigure.h ${TEST_DERIVED_BASE}/RunConfigure.h
	for n in $(passObjects); \
	do \
		$$n ${TEST_DERIVED_BASE}/${fn} $(EXTRA_FLAGS);\
	done

testins : instrumentation testnorm
	for name in $(transObjects); \
	do \
		$$name ${TEST_DERIVED_BASE}/${fn} $(EXTRA_FLAGS);\
	done
	./bin/passClean ${TEST_DERIVED_BASE}/${fn} $(EXTRA_FLAGS);

.PHONY : normalization
.PHONY : instrumentation
.PHONY : testnorm
.PHONY : testins

%.png : %.dot
	dot -Tpng $< -o $@

vis : $(subst .dot,.png,$(wildcard *.dot))

sample: src/main.cpp
	${CC} $(CXXFLAGS) $(LLVM_CXXFLAGS) $^ $(CLANG_LIBS) $(LLVM_LDFLAGS) -o bin/main -v


bin/tooltest : ustbtool/tooltest.cpp
	${CC} $(CXXFLAGS) $(LLVM_CXXFLAGS)  $< $(CLANG_LIBS) $(LLVM_LDFLAGS) -o $@

tooltest : bin/tooltest
	bin/tooltest ${TEST_BASE}/test_tool.cpp $(EXTRA_FLAGS)


.PHONY : tooltest

