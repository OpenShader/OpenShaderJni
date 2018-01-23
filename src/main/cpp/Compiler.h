#ifndef _Included_Compiler
#define _Included_Compiler

#include "JniCommon.h"
#include "ShaderLang.h"
#include "ShHandle.h"
#include "Common.h"
#include "intermediate.h"
#include "localintermediate.h"
#include <map>
#include <list>
#include <string>

using namespace glslang;

class CompileJob;

typedef TVector<TString>* InterfaceBlock;

class Shader : public glslang::TShader
{
public:
	Shader(EShLanguage lang) : glslang::TShader(lang) {}
	glslang::TIntermediate* getIntermediate() { return intermediate; }
};

class ShaderCompiler
{
	friend class CompileJob;
public:
	ShaderCompiler(int _defaultVersion);
	~ShaderCompiler();
	CompileJob* createCompileJob(EShLanguage shaderType);

	InterfaceBlock createUniformBlock()
	{
		return new TVector<TString>();
	}

	TString newString(const char* str) // The default TString is created on the threaded pool.
	{
		return TString(str);
	}

	void setCapablitity(ShaderCapability capability)
	{
		ShaderCapabilitySet(capabilities, capability);
	}

	void setIncluder(TShader::Includer *inc)
	{
		if (includer != nullptr)
		{
			delete includer;
		}
		includer = inc;
	}

private:
	int defaultVersion;
	int capabilities = 0;
	glslang::TShader::Includer *includer = new TShader::ForbidIncluder();
};

class CompileJob
{
	friend class ShaderCompiler;
public:
	~CompileJob();
	TString compile();
	void setSource(const char* source, const char* filename = nullptr)
	{
		if (this->source != nullptr)
			delete this->source;
		if (this->filename != nullptr)
			delete this->filename;
		this->source = new std::string(source);
		auto s = this->source->c_str();
		auto pp = reinterpret_cast<char**>(GetThreadPoolAllocator().allocate(2 * sizeof(char*)));
		pp[0] = const_cast<char*>(s);
		pp[1] = nullptr;
		const char* const* fp = nullptr;
		if (filename != nullptr) 
		{
			this->filename = new std::string(filename);
			auto f = this->filename->c_str();
			pp[1] = const_cast<char*>(f);
		} 
		else
		{
			const auto DEFAULT_FILENAME = "<unknown>";
			pp[1] = const_cast<char*>(DEFAULT_FILENAME);
		}
		compile_shader->setStringsWithLengthsAndNames(pp, nullptr, pp+1, 1);
	}
	void addPreprocess(const std::vector<std::string>& p)
	{
		compile_shader->addProcesses(p);
	}
	void addUniformBlock(const char* name, InterfaceBlock block)
	{
		uniformBlocks->insert_or_assign(TString(name), block);
	}
private:
	CompileJob(ShaderCompiler *compiler, EShLanguage shaderType);
	ShaderCompiler *compiler;
	EShLanguage type;
	TString *input_source = nullptr;
	Shader *compile_shader = nullptr;
	std::string *source = nullptr, *filename = nullptr;
	std::map<TString, InterfaceBlock> *uniformBlocks;
	//std::stringstream *debugInfo = nullptr;
};

const TBuiltInResource DefaultTBuiltInResource = {
	/* .MaxLights = */ 32,
	/* .MaxClipPlanes = */ 6,
	/* .MaxTextureUnits = */ 32,
	/* .MaxTextureCoords = */ 32,
	/* .MaxVertexAttribs = */ 64,
	/* .MaxVertexUniformComponents = */ 4096,
	/* .MaxVaryingFloats = */ 64,
	/* .MaxVertexTextureImageUnits = */ 32,
	/* .MaxCombinedTextureImageUnits = */ 80,
	/* .MaxTextureImageUnits = */ 32,
	/* .MaxFragmentUniformComponents = */ 4096,
	/* .MaxDrawBuffers = */ 32,
	/* .MaxVertexUniformVectors = */ 128,
	/* .MaxVaryingVectors = */ 8,
	/* .MaxFragmentUniformVectors = */ 16,
	/* .MaxVertexOutputVectors = */ 16,
	/* .MaxFragmentInputVectors = */ 15,
	/* .MinProgramTexelOffset = */ -8,
	/* .MaxProgramTexelOffset = */ 7,
	/* .MaxClipDistances = */ 8,
	/* .MaxComputeWorkGroupCountX = */ 65535,
	/* .MaxComputeWorkGroupCountY = */ 65535,
	/* .MaxComputeWorkGroupCountZ = */ 65535,
	/* .MaxComputeWorkGroupSizeX = */ 1024,
	/* .MaxComputeWorkGroupSizeY = */ 1024,
	/* .MaxComputeWorkGroupSizeZ = */ 64,
	/* .MaxComputeUniformComponents = */ 1024,
	/* .MaxComputeTextureImageUnits = */ 16,
	/* .MaxComputeImageUniforms = */ 8,
	/* .MaxComputeAtomicCounters = */ 8,
	/* .MaxComputeAtomicCounterBuffers = */ 1,
	/* .MaxVaryingComponents = */ 60,
	/* .MaxVertexOutputComponents = */ 64,
	/* .MaxGeometryInputComponents = */ 64,
	/* .MaxGeometryOutputComponents = */ 128,
	/* .MaxFragmentInputComponents = */ 128,
	/* .MaxImageUnits = */ 8,
	/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
	/* .MaxCombinedShaderOutputResources = */ 8,
	/* .MaxImageSamples = */ 0,
	/* .MaxVertexImageUniforms = */ 0,
	/* .MaxTessControlImageUniforms = */ 0,
	/* .MaxTessEvaluationImageUniforms = */ 0,
	/* .MaxGeometryImageUniforms = */ 0,
	/* .MaxFragmentImageUniforms = */ 8,
	/* .MaxCombinedImageUniforms = */ 8,
	/* .MaxGeometryTextureImageUnits = */ 16,
	/* .MaxGeometryOutputVertices = */ 256,
	/* .MaxGeometryTotalOutputComponents = */ 1024,
	/* .MaxGeometryUniformComponents = */ 1024,
	/* .MaxGeometryVaryingComponents = */ 64,
	/* .MaxTessControlInputComponents = */ 128,
	/* .MaxTessControlOutputComponents = */ 128,
	/* .MaxTessControlTextureImageUnits = */ 16,
	/* .MaxTessControlUniformComponents = */ 1024,
	/* .MaxTessControlTotalOutputComponents = */ 4096,
	/* .MaxTessEvaluationInputComponents = */ 128,
	/* .MaxTessEvaluationOutputComponents = */ 128,
	/* .MaxTessEvaluationTextureImageUnits = */ 16,
	/* .MaxTessEvaluationUniformComponents = */ 1024,
	/* .MaxTessPatchComponents = */ 120,
	/* .MaxPatchVertices = */ 32,
	/* .MaxTessGenLevel = */ 64,
	/* .MaxViewports = */ 16,
	/* .MaxVertexAtomicCounters = */ 0,
	/* .MaxTessControlAtomicCounters = */ 0,
	/* .MaxTessEvaluationAtomicCounters = */ 0,
	/* .MaxGeometryAtomicCounters = */ 0,
	/* .MaxFragmentAtomicCounters = */ 8,
	/* .MaxCombinedAtomicCounters = */ 8,
	/* .MaxAtomicCounterBindings = */ 1,
	/* .MaxVertexAtomicCounterBuffers = */ 0,
	/* .MaxTessControlAtomicCounterBuffers = */ 0,
	/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
	/* .MaxGeometryAtomicCounterBuffers = */ 0,
	/* .MaxFragmentAtomicCounterBuffers = */ 1,
	/* .MaxCombinedAtomicCounterBuffers = */ 1,
	/* .MaxAtomicCounterBufferSize = */ 16384,
	/* .MaxTransformFeedbackBuffers = */ 4,
	/* .MaxTransformFeedbackInterleavedComponents = */ 64,
	/* .MaxCullDistances = */ 8,
	/* .MaxCombinedClipAndCullDistances = */ 8,
	/* .MaxSamples = */ 4,
	/* .limits = */{
		/* .nonInductiveForLoops = */ 1,
		/* .whileLoops = */ 1,
		/* .doWhileLoops = */ 1,
		/* .generalUniformIndexing = */ 1,
		/* .generalAttributeMatrixVectorIndexing = */ 1,
		/* .generalVaryingIndexing = */ 1,
		/* .generalSamplerIndexing = */ 1,
		/* .generalVariableIndexing = */ 1,
		/* .generalConstantMatrixVectorIndexing = */ 1,
	} };

#endif
