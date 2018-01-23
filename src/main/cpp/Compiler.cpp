#include "Compiler.h"
#include "Traverser.h"
#include "PoolAlloc.h"

//#define __DEBUG__

#ifdef __DEBUG__
#include <chrono>

#define GET_NANOTIME() std::chrono::high_resolution_clock::now()
#define GET_DELTATIME(begin, end) std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count()
#else
#define GET_NANOTIME() 0
#define GET_DELTATIME(begin, end) 0
#endif

using namespace glslang;

ShaderCompiler::ShaderCompiler(int _defaultVersion) : defaultVersion(_defaultVersion)
{
}

ShaderCompiler::~ShaderCompiler()
{
	setIncluder(nullptr);
}

CompileJob * ShaderCompiler::createCompileJob(EShLanguage shaderType)
{
	CompileJob *job = new CompileJob(this, shaderType);
	return job;
}

CompileJob::CompileJob(ShaderCompiler *compiler, EShLanguage shaderType)
{
	this->compiler = compiler;
	this->type = shaderType;
	compile_shader = new Shader(type);
	uniformBlocks = new std::map<TString, InterfaceBlock>();
}

CompileJob::~CompileJob()
{
	if (compile_shader != nullptr)
		delete compile_shader;
	if (source != nullptr)
		delete source;
	if (filename != nullptr)
		delete filename;
	if (uniformBlocks != nullptr)
		delete uniformBlocks;
}

TString CompileJob::compile()
{
	auto _debug_startTime = GET_NANOTIME();
	compile_shader->setPreamble("\
		#extension GL_GOOGLE_include_directive : enable\n \
		#extension GL_ARB_shading_language_420pack : enable\n \
		#extension GL_ARB_shader_texture_lod : enable\n \
		");
	auto prePool = &GetThreadPoolAllocator();
	auto result = compile_shader->parse(&DefaultTBuiltInResource, compiler->defaultVersion, ECompatibilityProfile, false, false,
		(EShMessages)(EShMsgDebugInfo), *compiler->includer);
	SetThreadPoolAllocator(prePool);
	if (!result)
	{
		return compile_shader->getInfoLog();
	}
	
#ifdef __DEBUG__
	auto _debug_parseFinishTime = GET_NANOTIME();
	auto _debug_delta = GET_DELTATIME(_debug_startTime, _debug_parseFinishTime);
#endif
	_debug_startTime = GET_NANOTIME();
	auto shaderCapabilities = compiler->capabilities;
	auto interm = compile_shader->getIntermediate();
	interm->setVersion(compiler->defaultVersion);
	if (compiler->defaultVersion >= 320)
		interm->setProfile(ECompatibilityProfile);
	if (compiler->defaultVersion < 300)
		ShaderCapabilitySet(shaderCapabilities, GLSL130, false);
	if (compiler->defaultVersion < 320)
		ShaderCapabilitySet(shaderCapabilities, GLSL150, false);
	if (compiler->defaultVersion < 400)
		ShaderCapabilitySet(shaderCapabilities, GLSL400, false);
	ArrangingTraverser arranging_traverser;
	EmitterTraverser traverser(shaderCapabilities, uniformBlocks);
	auto newSource = traverser.process(arranging_traverser, interm);
#ifdef __DEBUG__
	_debug_parseFinishTime = GET_NANOTIME();
	auto _debug_delta2 = GET_DELTATIME(_debug_startTime, _debug_parseFinishTime);
	std::stringstream debugResult;
	debugResult << "time1: " << _debug_delta << " time2:" << _debug_delta2;
#endif
	//return TString(debugResult.str().c_str());
	return newSource;
	//return compile_shader->getInfoDebugLog();
}
