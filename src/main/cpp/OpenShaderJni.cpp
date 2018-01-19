#define IMPLEMENT_JNICOMMON

#include "org_openshader_jni_OpenShaderJni.h"
#include "ShaderLang.h"
#include "intermediate.h"
#include "ShaderReconstructor.h"
#include "Compiler.h"
#include <string>

using namespace std;
using namespace glslang;

JNIEnv* JAVAENV;
jclass JAVACLASS_EXCEPTION;
jclass JAVACLASS_GLSLANG;
jclass JAVACLASS_INCLUDER;
jmethodID JAVAMETHOD_GLSLANG_INCLUDELOCAL;
jmethodID JAVAMETHOD_GLSLANG_INCLUDESYSTEM;
bool glslangAvailable = false;

const size_t MAX_INCLUDE_DEPTH = 32;

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
	JNIEnv* env = NULL;
	if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK)
	{
		return JNI_ERR;
	}
	JAVAENV = env;
	glslangAvailable = glslang::InitializeProcess();
	return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved)
{
	glslang::FinalizeProcess();
}

class CallbackIncluder : public glslang::TShader::Includer {
private:
	jobject includer;

	IncludeResult* include(jmethodID method, const char* headerName, const char* includerName, size_t inclusionDepth)
	{
		if (inclusionDepth > MAX_INCLUDE_DEPTH)
		{
			return nullptr; // TODO: throw an exception
		}
		jstring jHeaderName = JAVAENV->NewStringUTF(headerName);
		jstring jIncluderName = JAVAENV->NewStringUTF(includerName);
		jobject result = JAVAENV->CallObjectMethod(includer, method, jHeaderName, jIncluderName);
		if (result == nullptr) // not found
		{
			return nullptr; // TODO: throw an exception
		}
		const char* pointer = JAVAENV->GetStringUTFChars((jstring)result, nullptr);
		string *data = new string(pointer);
		size_t length = data->length();
		JAVAENV->ReleaseStringUTFChars((jstring)result, pointer);
		IncludeResult *res = new IncludeResult(headerName, data->c_str(), length, data);
		return res;
	}

public:
	CallbackIncluder(jobject _includer) : includer(JAVAENV->NewGlobalRef(_includer))
	{
	}

	~CallbackIncluder()
	{
		JAVAENV->DeleteGlobalRef(includer);
	}

	virtual IncludeResult* includeLocal(const char* headerName,
		const char* includerName,
		size_t inclusionDepth) override
	{
		return include(JAVAMETHOD_GLSLANG_INCLUDELOCAL, headerName, includerName, inclusionDepth);
	}

	virtual IncludeResult* includeSystem(const char* headerName,
		const char* includerName,
		size_t inclusionDepth) override
	{
		return include(JAVAMETHOD_GLSLANG_INCLUDESYSTEM, headerName, includerName, inclusionDepth);
	}

	virtual void releaseInclude(IncludeResult* result) override
	{
		if (result != nullptr) {
			delete result->userData;
			delete result;
		}
	}
};


jclass findClass(JNIEnv *env, const char *classname)
{
	if (env->ExceptionCheck())
		return nullptr;
	jclass klass = env->FindClass(classname);
	if (klass == nullptr && env->ExceptionCheck())
	{
		jthrowable error = env->ExceptionOccurred();
		//env->ExceptionClear();
		env->Throw(error);
	}
	return static_cast<jclass>(env->NewGlobalRef(klass));
}

jmethodID findMethod(JNIEnv *env, jclass klass, const char *methodname, const char *sig)
{
	if (env->ExceptionCheck())
		return nullptr;
	jmethodID method = env->GetMethodID(klass, methodname, sig);
	if (method == nullptr && env->ExceptionCheck())
	{
		jthrowable error = env->ExceptionOccurred();
		//env->ExceptionClear();
		env->Throw(error);
	}
	return method;
}

JNIEXPORT void JNICALL Java_org_openshader_jni_OpenShaderJni_initJni(JNIEnv *env, jclass klass)
{
	JAVACLASS_EXCEPTION = env->FindClass("Ljava/lang/Exception;");
	if (!glslangAvailable)
	{
		env->ThrowNew(JAVACLASS_EXCEPTION, "Failed to initialize Glslang.");
		return;
	}
	env->ExceptionClear();
	JAVACLASS_EXCEPTION = static_cast<jclass>(env->NewGlobalRef(JAVACLASS_EXCEPTION));
	JAVACLASS_GLSLANG = findClass(env, "Lorg/openshader/jni/OpenShaderJni;");
	JAVACLASS_INCLUDER = findClass(env, "Lorg/openshader/jni/compiler/IHeaderIncluder;");
	JAVAMETHOD_GLSLANG_INCLUDELOCAL = findMethod(env, JAVACLASS_INCLUDER, "includeLocal", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
	/*Shader* shader = new Shader(lang);
	if (!shader->parse(&DefaultTBuiltInResource, 450, ECompatibilityProfile, true, false, EShMsgAST, CallbackIncluder()))
	{
		delete shader;
		env->ThrowNew(JAVACLASS_EXCEPTION, "");
		return;
	}
	//TIntermNode* root = shader->getIntermediate();
	ShaderReconstructor reconstructor = ShaderReconstructor(shader->getIntermediate());
	reconstructor.emitCode();
	delete shader;*/
}

JNIEXPORT void JNICALL Java_org_openshader_jni_OpenShaderJni_assertOperators(JNIEnv *env, jclass klass, int numOfEnumOperator)
{
	if (glslang::TOperator::EOpMatrixSwizzle + 1 != numOfEnumOperator)
	{
		env->ThrowNew(JAVACLASS_EXCEPTION, "Mismatched operator enum. This problem shouldn't happen!");
	}
}

JNIEXPORT jlong JNICALL Java_org_openshader_jni_compiler_ShaderCompilerManager_createShaderCompiler_1do(JNIEnv *env, jclass klass, jint defaultVersion)
{
	ShaderCompiler *compiler = new ShaderCompiler(defaultVersion);
	return reinterpret_cast<jlong>(compiler);
}

JNIEXPORT void JNICALL Java_org_openshader_jni_compiler_ShaderCompiler_delete_1do(JNIEnv *env, jclass klass, jlong ptr)
{
	ShaderCompiler *compiler = reinterpret_cast<ShaderCompiler*>(ptr);
	delete compiler;
}

JNIEXPORT jlong JNICALL Java_org_openshader_jni_compiler_ShaderCompiler_createCompileJob_1do(JNIEnv *env, jclass klass, jlong ptr, jint type)
{
	ShaderCompiler *compiler = reinterpret_cast<ShaderCompiler*>(ptr);
	CompileJob *job = compiler->createCompileJob(static_cast<EShLanguage>(type));
	return reinterpret_cast<jlong>(job);
}

JNIEXPORT void JNICALL Java_org_openshader_jni_compiler_ShaderCompiler_setShaderCapability_1do(JNIEnv *env, jclass klass, jlong ptr, jint capability)
{
	ShaderCompiler *compiler = reinterpret_cast<ShaderCompiler*>(ptr);
	compiler->setCapablitity(static_cast<ShaderCapability>(capability));
}

JNIEXPORT void JNICALL Java_org_openshader_jni_compiler_ShaderCompiler_setIncluder_1do(JNIEnv *env, jclass klass, jlong ptr, jobject includer)
{
	CallbackIncluder *callbackIncluder = new CallbackIncluder(includer);
	ShaderCompiler *compiler = reinterpret_cast<ShaderCompiler*>(ptr);
	compiler->setIncluder(callbackIncluder);
}

JNIEXPORT void JNICALL Java_org_openshader_jni_compiler_CompileJob_delete_1do(JNIEnv *env, jclass klass, jlong ptr)
{
	CompileJob *job = reinterpret_cast<CompileJob*>(ptr);
	delete job;
}

JNIEXPORT void JNICALL Java_org_openshader_jni_compiler_CompileJob_setSource_1do(JNIEnv *env, jclass klass, jlong ptr, jstring source, jstring filename)
{
	CompileJob *job = reinterpret_cast<CompileJob*>(ptr);
	const char* s = env->GetStringUTFChars(source, nullptr);
	const char* f = filename == nullptr ? nullptr : env->GetStringUTFChars(filename, nullptr);
	job->setSource(s, f);
	env->ReleaseStringUTFChars(source, s);
	if (f != nullptr)
		env->ReleaseStringUTFChars(filename, f);
}

JNIEXPORT void JNICALL Java_org_openshader_jni_compiler_CompileJob_addPreprocess_1do(JNIEnv *env, jclass klass, jlong ptr, jobjectArray strArray)
{
	jsize size = env->GetArrayLength(strArray);
	jobject *arr = static_cast<jobject*>(env->GetPrimitiveArrayCritical(strArray, nullptr));
	CompileJob *job = reinterpret_cast<CompileJob*>(ptr);
	job->setThreadedPool();
	std::vector<std::string> vec;
	for (int i = 0; i < size; i++)
	{
		jstring str = static_cast<jstring>(*arr);
		const char* s = env->GetStringUTFChars(str, nullptr);
		TString *preprocessString = NewPoolTString(s);
		env->ReleaseStringUTFChars(str, s);
		vec.push_back(*reinterpret_cast<string*>(preprocessString));
	}
	env->ReleasePrimitiveArrayCritical(strArray, arr, JNI_ABORT);
	job->addPreprocess(vec);
}

JNIEXPORT jstring JNICALL Java_org_openshader_jni_compiler_CompileJob_compile_1do(JNIEnv *env, jclass klass, jlong ptr)
{
	try
	{
		CompileJob *job = reinterpret_cast<CompileJob*>(ptr);
		TString result = job->compile();
		return env->NewStringUTF(result.c_str());
	}
	catch (std::exception &ex)
	{
		string cause = string("Failed to compile shader:") + ex.what();
		env->ThrowNew(JAVACLASS_EXCEPTION, cause.c_str());
		return env->NewStringUTF("");
	}
}