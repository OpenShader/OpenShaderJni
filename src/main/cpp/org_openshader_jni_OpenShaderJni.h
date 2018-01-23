#ifndef _Included_org_openshader_jni_OpenShaderJni
#define _Included_org_openshader_jni_OpenShaderJni

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

	JNIEXPORT void		 JNICALL Java_org_openshader_jni_OpenShaderJni_initJni(JNIEnv *, jclass);
	JNIEXPORT void		 JNICALL Java_org_openshader_jni_OpenShaderJni_assertOperators(JNIEnv *, jclass, int);
	JNIEXPORT jlong		 JNICALL Java_org_openshader_jni_compiler_ShaderCompilerManager_createShaderCompiler_1do(JNIEnv *, jclass, jint);
	JNIEXPORT void		 JNICALL Java_org_openshader_jni_compiler_ShaderCompilerManager_registerWorkThread_1do(JNIEnv *, jclass);
	JNIEXPORT void		 JNICALL Java_org_openshader_jni_compiler_ShaderCompilerManager_unregisterWorkThread_1do(JNIEnv *, jclass);
	JNIEXPORT void		 JNICALL Java_org_openshader_jni_compiler_ShaderCompilerManager_pushMemoryPool_do_1do(JNIEnv *, jclass);
	JNIEXPORT void		 JNICALL Java_org_openshader_jni_compiler_ShaderCompilerManager_popMemoryPool_do_1do(JNIEnv *, jclass);
	JNIEXPORT void		 JNICALL Java_org_openshader_jni_compiler_ShaderCompiler_delete_1do(JNIEnv *, jclass, jlong);
	JNIEXPORT jlong		 JNICALL Java_org_openshader_jni_compiler_ShaderCompiler_createCompileJob_1do(JNIEnv *, jclass, jlong, jint);
	JNIEXPORT jlong		 JNICALL Java_org_openshader_jni_compiler_ShaderCompiler_createUniformBlock_1do(JNIEnv *, jclass, jlong);
	JNIEXPORT void		 JNICALL Java_org_openshader_jni_compiler_ShaderCompiler_addMember_1do(JNIEnv *, jclass, jlong, jlong, jstring);
	JNIEXPORT void		 JNICALL Java_org_openshader_jni_compiler_ShaderCompiler_setShaderCapability_1do(JNIEnv *, jclass, jlong, jint);
	JNIEXPORT void		 JNICALL Java_org_openshader_jni_compiler_ShaderCompiler_setIncluder_1do(JNIEnv *, jclass, jlong, jobject);
	JNIEXPORT void		 JNICALL Java_org_openshader_jni_compiler_CompileJob_delete_1do(JNIEnv *, jclass, jlong);
	JNIEXPORT void		 JNICALL Java_org_openshader_jni_compiler_CompileJob_setSource_1do(JNIEnv *, jclass, jlong, jstring, jstring);
	JNIEXPORT void		 JNICALL Java_org_openshader_jni_compiler_CompileJob_addUniformBlock_1do(JNIEnv *, jclass, jlong, jlong, jstring);
	JNIEXPORT void		 JNICALL Java_org_openshader_jni_compiler_CompileJob_addPreprocess_1do(JNIEnv *, jclass, jlong, jobjectArray);
	JNIEXPORT jstring	 JNICALL Java_org_openshader_jni_compiler_CompileJob_compile_1do(JNIEnv *, jclass, jlong);

#ifdef __cplusplus
}
#endif

#endif