vpath %.cpp src src/normalization src/transformer src/instrumentation
vpath %h src/util
vpath %hpp src/util src/transformer

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_dir := $(dir $(mkfile_path))

CC = /usr/bin/clang++
CLANG_LIBS = -lLLVM -lLLVMCodeGen -llldYAML -lclang -lclangAST -lclangASTMatchers -lclangAnalysis -lclangApplyReplacements -lclangBasic -lclangChangeNamespace -lclangCodeGen -lclangCrossTU -lclangDependencyScanning -lclangDirectoryWatcher -lclangDoc -lclangDriver -lclangDynamicASTMatchers -lclangEdit -lclangFormat -lclangFrontend -lclangFrontendTool -lclangHandleCXX -lclangHandleLLVM -lclangIncludeFixer -lclangIncludeFixerPlugin -lclangIndex -lclangLex -lclangMove -lclangParse -lclangQuery -lclangReorderFields -lclangRewrite -lclangRewriteFrontend -lclangSema -lclangSerialization -lclangStaticAnalyzerCheckers -lclangStaticAnalyzerCore -lclangStaticAnalyzerFrontend -lclangTesting -lclangTidy -lclangTooling -lclangToolingASTDiff -lclangToolingCore -lclangToolingInclusions -lclangToolingRefactoring -lclangToolingSyntax -lclangTransformer -lclangdSupport
INCL_PATHS = -I/usr/local/Cellar/llvm/11.0.0/include -I.
LIB_PATHS = -L/usr/local/Cellar/llvm/11.0.0/lib

COMPILER_FLAGS = -std=c++17
LIBS = -lstdc++ -lm -lgmp -lmpfr ${CLANG_LIBS}

TOOL_ARGS = #-- ${COMPILER_FLAGS} ${INCL_PATHS}

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


normalization : passZero, passOne, passTwo, passThree

turnFpArith: instrumentation/turnFpArith.cpp
	cd src && ${CC} ${COMPILER_FLAGS} ${INCL_PATHS} -g instrumentation/turnFpArith.cpp ${LIB_PATHS} ${LIBS} -o ../bin/turnFpArith

instrumentation : turnFpArith

base=./test
fn=test.cpp
testnorm: passZero passOne passTwo passThree
	rm -r ${base}/derived; mkdir ${base}/derived; cp ${base}/${fn} ${base}/derived/${fn}
	./bin/passZero ${base}/derived/${fn} ${TOOL_ARGS}
	./bin/passOne ${base}/derived/${fn} ${TOOL_ARGS}
	./bin/passTwo ${base}/derived/${fn} ${TOOL_ARGS}
	./bin/passThree ${base}/derived/${fn} ${TOOL_ARGS}

testins: instrumentation testnorm
	./bin/turnFpArith ${base}/derived/${fn} ${TOOL_ARGS}

testnormonly:
	rm -r ${base}/derived; mkdir ${base}/derived; cp ${base}/${fn} ${base}/derived/${fn}
	./bin/passZero ${base}/derived/${fn} ${TOOL_ARGS}
	./bin/passOne ${base}/derived/${fn} ${TOOL_ARGS}
	./bin/passTwo ${base}/derived/${fn} ${TOOL_ARGS}
	./bin/passThree ${base}/derived/${fn} ${TOOL_ARGS}
	# ./bin/passClean ${base}/derived/${fn} ${TOOL_ARGS}

testinsonly: testnormonly
	./bin/turnFpArith ${base}/derived/${fn} ${TOOL_ARGS}
	# ./bin/passClean ${base}/derived/${fn} ${TOOL_ARGS}
