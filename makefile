vpath %.cpp src src/normalization src/transformer src/instrumentation
vpath %h src/util
vpath %hpp src/util src/transformer


MAKE_DIR:="$(shell pwd)"
SRC_DIR:="$(shell pwd)/src"
TEST_BASE=./test
TEST_DERIVED_BASE = ${TEST_BASE}/derived
fn=test.cpp


CC = /usr/bin/clang++
CLANG_LIBS = -lLLVM -lLLVMCodeGen -llldYAML -lclang -lclangAST -lclangASTMatchers -lclangAnalysis -lclangApplyReplacements -lclangBasic -lclangChangeNamespace -lclangCodeGen -lclangCrossTU -lclangDependencyScanning -lclangDirectoryWatcher -lclangDoc -lclangDriver -lclangDynamicASTMatchers -lclangEdit -lclangFormat -lclangFrontend -lclangFrontendTool -lclangHandleCXX -lclangHandleLLVM -lclangIndex -lclangLex -lclangMove -lclangParse -lclangQuery -lclangReorderFields -lclangRewrite -lclangRewriteFrontend -lclangSema -lclangSerialization -lclangStaticAnalyzerCheckers -lclangStaticAnalyzerCore -lclangStaticAnalyzerFrontend -lclangTooling -lclangToolingASTDiff -lclangToolingCore -lclangToolingInclusions -lclangToolingRefactoring -lclangToolingSyntax -lclangTransformer
INCL_PATHS = -I/usr/local/opt/llvm/include -I${SRC_DIR}
LIB_PATHS = -L/usr/local/opt/llvm/lib

COMPILER_FLAGS = -std=c++17
LIBS = -lstdc++ -lm -lgmp -lmpfr ${CLANG_LIBS}

TOOL_ARGS = -- ${COMPILER_FLAGS} ${INCL_PATHS} -I/usr/local/include -I/Library/Developer/CommandLineTools/usr/include/c++/v1

info:
	@echo ${MAKE_DIR}

passZero: normalization/passZero.cpp
	cd src && ${CC} ${COMPILER_FLAGS} ${INCL_PATHS} -g normalization/passZero.cpp ${LIB_PATHS} ${LIBS} -o ../bin/passZero

passOne: normalization/passOne.cpp
	cd src && ${CC} ${COMPILER_FLAGS} ${INCL_PATHS} -g normalization/passOne.cpp ${LIB_PATHS} ${LIBS} -o ../bin/passOne

passTwo: normalization/passTwo.cpp
	cd src && ${CC} ${COMPILER_FLAGS} ${INCL_PATHS} -g normalization/passTwo.cpp ${LIB_PATHS} ${LIBS} -o ../bin/passTwo

passThree: normalization/passThree.cpp
	cd src && ${CC} ${COMPILER_FLAGS} ${INCL_PATHS} -g normalization/passThree.cpp ${LIB_PATHS} ${LIBS} -o ../bin/passThree

passClean: normalization/passClean.cpp
	cd src && ${CC} ${COMPILER_FLAGS} ${INCL_PATHS} -g normalization/passClean.cpp ${LIB_PATHS} ${LIBS} -o ../bin/passClean


normalization : passZero passOne passTwo passThree

turnFpArith: instrumentation/turnFpArith.cpp
	cd src && ${CC} ${COMPILER_FLAGS} ${INCL_PATHS} -g instrumentation/turnFpArith.cpp ${LIB_PATHS} ${LIBS} -o ../bin/turnFpArith

instrumentation : turnFpArith



testnorm: passZero passOne passTwo passThree
	rm -r ${TEST_DERIVED_BASE}; mkdir ${TEST_DERIVED_BASE}; cp ${TEST_BASE}/${fn} ${TEST_DERIVED_BASE}/${fn}
	./bin/passZero ${TEST_DERIVED_BASE}/${fn} ${TOOL_ARGS}
	./bin/passOne ${TEST_DERIVED_BASE}/${fn} ${TOOL_ARGS}
	./bin/passTwo ${TEST_DERIVED_BASE}/${fn} ${TOOL_ARGS}
	./bin/passThree ${TEST_DERIVED_BASE}/${fn} ${TOOL_ARGS}

testins: instrumentation testnorm
	./bin/turnFpArith ${TEST_DERIVED_BASE}/${fn} ${TOOL_ARGS}

testnormonly:
	rm -r ${TEST_DERIVED_BASE}; mkdir ${TEST_DERIVED_BASE}; cp ${TEST_BASE}/${fn} ${TEST_DERIVED_BASE}/${fn}
	./bin/passZero ${TEST_DERIVED_BASE}/${fn} ${TOOL_ARGS}
	./bin/passOne ${TEST_DERIVED_BASE}/${fn} ${TOOL_ARGS}
	./bin/passTwo ${TEST_DERIVED_BASE}/${fn} ${TOOL_ARGS}
	./bin/passThree ${TEST_DERIVED_BASE}/${fn} ${TOOL_ARGS}
	./bin/passClean ${TEST_DERIVED_BASE}/${fn} ${TOOL_ARGS}

testinsonly: testnormonly
	./bin/turnFpArith ${TEST_DERIVED_BASE}/${fn} ${TOOL_ARGS}
	# ./bin/passClean ${TEST_DERIVED_BASE}/${fn} ${TOOL_ARGS}
