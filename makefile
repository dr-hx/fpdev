vpath %.cpp src src/normalization src/transformer
vpath %h src/util

CC = /usr/bin/clang++
CLANG_LIBS = -lLLVM -lLLVMCodeGen -llldYAML -lclang -lclangARCMigrate -lclangAST -lclangASTMatchers -lclangAnalysis -lclangApplyReplacements -lclangBasic -lclangChangeNamespace -lclangCodeGen -lclangCrossTU -lclangDaemon -lclangDaemonTweaks -lclangDependencyScanning -lclangDirectoryWatcher -lclangDoc -lclangDriver -lclangDynamicASTMatchers -lclangEdit -lclangFormat -lclangFrontend -lclangFrontendTool -lclangHandleCXX -lclangHandleLLVM -lclangIncludeFixer -lclangIncludeFixerPlugin -lclangIndex -lclangLex -lclangMove -lclangParse -lclangQuery -lclangReorderFields -lclangRewrite -lclangRewriteFrontend -lclangSema -lclangSerialization -lclangStaticAnalyzerCheckers -lclangStaticAnalyzerCore -lclangStaticAnalyzerFrontend -lclangTesting -lclangTidy -lclangTidyAbseilModule -lclangTidyAndroidModule -lclangTidyBoostModule -lclangTidyBugproneModule -lclangTidyCERTModule -lclangTidyCppCoreGuidelinesModule -lclangTidyDarwinModule -lclangTidyFuchsiaModule -lclangTidyGoogleModule -lclangTidyHICPPModule -lclangTidyLLVMLibcModule -lclangTidyLLVMModule -lclangTidyLinuxKernelModule -lclangTidyMPIModule -lclangTidyMain -lclangTidyMiscModule -lclangTidyModernizeModule -lclangTidyObjCModule -lclangTidyOpenMPModule -lclangTidyPerformanceModule -lclangTidyPlugin -lclangTidyPortabilityModule -lclangTidyReadabilityModule -lclangTidyUtils -lclangTidyZirconModule -lclangTooling -lclangToolingASTDiff -lclangToolingCore -lclangToolingInclusions -lclangToolingRefactoring -lclangToolingSyntax -lclangTransformer -lclangdRemoteIndex -lclangdSupport -lclangdXpcJsonConversions -lclangdXpcTransport
INCL_PATHS = -I/usr/local/Cellar/llvm/11.0.0/include/ -I.
LIB_PATHS = -L/usr/local/Cellar/llvm/11.0.0/lib

COMPILER_FLAGS = -std=c++17
LIBS = -lstdc++ -lm -lgmp -lmpfr ${CLANG_LIBS}

passOne: normalization/passOne.cpp
	cd src && ${CC} ${COMPILER_FLAGS} ${INCL_PATHS} -g normalization/passOne.cpp ${LIB_PATHS} ${LIBS} -o ../bin/passOne

passTwo: normalization/passTwo.cpp
	cd src && ${CC} ${COMPILER_FLAGS} ${INCL_PATHS} -g normalization/passTwo.cpp ${LIB_PATHS} ${LIBS} -o ../bin/passTwo


test: passOne
	./bin/passOne ./test/test.cpp