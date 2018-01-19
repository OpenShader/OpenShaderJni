#include "ShaderReconstructor.h"
#include "localintermediate.h"
#include <sstream>
#include "Types.h"

class EmitterTraverser : public TIntermTraverser {
public:
	EmitterTraverser(int ver) { version = ver; };

	virtual bool visitBinary(TVisit, TIntermBinary* node);
	virtual bool visitUnary(TVisit, TIntermUnary* node);
	virtual bool visitAggregate(TVisit, TIntermAggregate* node);
	virtual bool visitSelection(TVisit, TIntermSelection* node);
	virtual void visitConstantUnion(TIntermConstantUnion* node);
	virtual void visitSymbol(TIntermSymbol* node);
	virtual bool visitLoop(TVisit, TIntermLoop* node);
	virtual bool visitBranch(TVisit, TIntermBranch* node);
	virtual bool visitSwitch(TVisit, TIntermSwitch* node);
protected:
	EmitterTraverser(EmitterTraverser&);
	EmitterTraverser& operator=(EmitterTraverser&);
	int version;
};

ReconstructedShader* ShaderReconstructor::emitCode()
{
	stringstream out;
	out << "#version " << version << (isCompatibility ? " compatibility" : "") << endl;
	for (auto ext = extensions.begin(); ext != extensions.end(); ++ext)
	{
		out << "#extension " << *ext << " : enable" << endl;
	}
	auto rootNode = intermediate->getTreeRoot()->getAsAggregate();
	auto rootSeq = rootNode->getSequence();
	auto linkage = rootSeq.back()->getAsAggregate();
	auto linkSeq = linkage->getSequence();
	map<string, vector<TIntermSymbol*>> uboBlocks = map<string, vector<TIntermSymbol*>>();
	for (auto elem = linkSeq.begin(); elem != linkSeq.end(); ++elem)
	{
		TIntermSymbol* symbol = (*elem)->getAsSymbolNode();
		TString name = symbol->getName();
		auto getter = uboMapping.find(name);
		if (getter != uboMapping.end()) // if it is a member of ubo block
		{
			string uboBlockName = getter->second;
			auto getter2 = uboBlocks.find(uboBlockName);
			if (getter2 != uboBlocks.end())
			{
				auto vec = getter2->second;
				vec.push_back(symbol);
			}
			else
			{
				vector<TIntermSymbol*> vec = vector<TIntermSymbol*>();
				vec.push_back(symbol);
				uboBlocks.insert_or_assign(uboBlockName, vec);
			}
		}
		else
		{
			emitVarDeclaration(symbol, out);
		}
	}
	return nullptr;
}

void ShaderReconstructor::emitVarDeclaration(glslang::TIntermSymbol *node, std::stringstream &out)
{
	const TType* type = &(node->getType());
	//type->getPrecisionQualifierString()
}
