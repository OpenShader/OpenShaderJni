#ifndef _Included_ShaderReconstructor
#define _Included_ShaderReconstructor

#include "ShaderLang.h"
#include "intermediate.h"
#include "Types.h"
#include <vector>
#include <string>
#include <map>

using namespace std;
using namespace glslang;

class ReconstructedShader
{
private:
};

class ShaderReconstructor
{
public:
	ShaderReconstructor(glslang::TIntermediate *interm) { intermediate = interm; };
	void setVersion(int ver) { version = ver; }
	void setCompatibility(bool compatibility) { isCompatibility = compatibility; }
	ReconstructedShader* emitCode();
private:
	glslang::TIntermediate *intermediate;
	int version = 450;
	bool isCompatibility = true;
	std::vector<std::string> extensions = std::vector<string>();
	std::map<glslang::TString, std::string> uboMapping = std::map<glslang::TString, std::string>();
	void emitVarDeclaration(glslang::TIntermSymbol*, std::stringstream&);
};

#endif