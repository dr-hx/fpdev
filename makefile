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

# SYS_ROOT = -internal-isystem /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/../include/c++/v1 -internal-isystem /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/local/include -internal-isystem /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/12.0.0/include -internal-externc-isystem /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include -internal-externc-isystem /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include

# TOOL_ARGS = -- ${COMPILER_FLAGS}  -m64 -isysroot "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk" ${SYS_ROOT} -mmacosx-version-min=11.0.0  -I/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include -stdlib=libc++

# EXTRA_ARG = -- --target=x86_64-apple-macosx11.0.0 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -I/usr/local/include -stdlib=libc++ -I/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include

# EXT = --extra-arg-before="--target=x86_64-apple-macosx11.0.0 -nostdinc -I/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include"

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
	./bin/passClean ${TEST_DERIVED_BASE}/${fn} ${TOOL_ARGS}
