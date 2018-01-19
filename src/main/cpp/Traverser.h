#ifndef _Included_Traverser
#define _Included_Traverser

#include "JniCommon.h"
#include "ShaderLang.h"
#include "Common.h"
#include "intermediate.h"
#include <string>
#include <sstream>

using namespace glslang;

struct TraverseDepth
{
	unsigned int depth : 7;
	bool isStatement : 1;

	TraverseDepth(int _depth, bool statement)
	{
		depth = static_cast<unsigned int>(_depth);
		isStatement = statement;
	}

	TraverseDepth() : TraverseDepth(0, false) {}
};

class EmitterTraverser;

class ArrangingTraverser : public TIntermTraverser {
	friend class EmitterTraverser;
public:
	ArrangingTraverser()
	{
	};

	virtual bool visitBinary(TVisit, TIntermBinary* node);
	virtual bool visitUnary(TVisit, TIntermUnary* node);
	virtual bool visitAggregate(TVisit, TIntermAggregate* node);
	virtual bool visitSelection(TVisit, TIntermSelection* node);
	virtual void visitConstantUnion(TIntermConstantUnion* node);
	virtual void visitSymbol(TIntermSymbol* node);
	virtual bool visitLoop(TVisit, TIntermLoop* node);
	virtual bool visitBranch(TVisit, TIntermBranch* node);
	virtual bool visitSwitch(TVisit, TIntermSwitch* node);
	const TVector<TIntermSymbol*>* getSymbolTable() { return &symbolTable; }
	const TMap<TString, TTypeList*>* getStructTable() { return &structTable; }
private:
	TVector<TIntermSymbol*> symbolTable = TVector<TIntermSymbol*>();
	TMap<TString, TTypeList*> structTable = TMap<TString, TTypeList*>();
	TVector<TString> structEmittingOrder = TVector<TString>(); // "suckest" design in this project
	TIntermAggregate* initSeq = nullptr;
	void findStruct(TType& structRef);
	void process(TIntermediate *intermediate, TPoolAllocator *allocator);
};

class EmitterTraverser : public TIntermTraverser {
public:
	EmitterTraverser(int shaderCapabilities, TMap<TString, TVector<TString>*>* uniformBlocks)
	{
		this->shaderCapabilities = shaderCapabilities;
		this->uniformBlocks = uniformBlocks;
		traverseDepths[0] = TraverseDepth(0, false);
	};

	virtual bool visitBinary(TVisit, TIntermBinary* node);
	virtual bool visitUnary(TVisit, TIntermUnary* node);
	virtual bool visitAggregate(TVisit, TIntermAggregate* node);
	virtual bool visitSelection(TVisit, TIntermSelection* node);
	virtual void visitConstantUnion(TIntermConstantUnion* node);
	virtual void visitSymbol(TIntermSymbol* node);
	virtual bool visitLoop(TVisit, TIntermLoop* node);
	virtual bool visitBranch(TVisit, TIntermBranch* node);
	virtual bool visitSwitch(TVisit, TIntermSwitch* node);
	TString process(ArrangingTraverser& arranging_traverser, TIntermediate *intermediate, TPoolAllocator *allocator);
	TString printOut(TPoolAllocator *allocator)
	{
		return TString(out.str().c_str(), *allocator);
	}
private:
	TIntermediate *intermediate;
	int shaderCapabilities;
	TVector<TIntermSymbol*>* symbolTable;
	TMap<TString, TTypeList*>* structTable;
	TVector<TString>* structEmittingOrder;
	TIntermAggregate* initSeq = nullptr;
	TMap<TString, TVector<TString>*>* uniformBlocks;
	std::stringstream out = std::stringstream();
	void emitSymbol(const TType& type, bool isDeclaration, bool hasSemicolon, const char* symbolName = nullptr);
	void emitSymbol(TIntermSymbol* symbol, bool isDeclaration, bool hasSemicolon)
	{
		emitSymbol(symbol->getType(), isDeclaration, hasSemicolon, symbol->getName().c_str());
	}
	void emitQualifier(const TType &type);
	bool emitBinary(const char* op, TIntermNode* left, TIntermNode* right, bool brackets = true);
	bool emitStatements(TIntermNode *node);
	bool emitInvoking(const char* head, TVector<TIntermNode*> &seq);
	bool emitConstruction(TOperator& op, TType& type, TVector<TIntermNode*> &seq);
	bool emitEquation(const char* symbol, TVector<TIntermNode*> &seq);
	bool emitUnary(const char* op, TIntermNode* node);
	void emitConst(int &index, TConstUnionArray& constArray, TType& type, int maxArrayDim = INT16_MAX);
	void emitNonArrayConst(int &index, TConstUnionArray& constArray, TType& type);
	const char* getType(const TType &type);
	const char* getOriginTextureFunction(TOperator &op, TVector<TIntermNode*> &seq);
	const char* getTypeConstruction(TOperator &op);
#define MAX_TRAVERSE_DEPTH 256
	TraverseDepth traverseDepths[MAX_TRAVERSE_DEPTH];
	int traverseDepthsPointer = 0;
	TVector<TVector<TString>*> declaredVars = TVector<TVector<TString>*>();

	inline void pushDepth(bool isStatement)
	{
		if (++traverseDepthsPointer >= MAX_TRAVERSE_DEPTH)
		{
			// TODO: throw
		}
		else
		{
			traverseDepths[traverseDepthsPointer] = TraverseDepth(traverseDepthsPointer, isStatement);
			declaredVars.push_back(new TVector<TString>);
		}
	}

	inline void pushDepth()
	{
		pushDepth(false);
	}

	inline void popDepth()
	{
		if (traverseDepthsPointer == 0)
		{
			// TODO: throw
		}
		else
		{
			traverseDepthsPointer--;
			auto vars = declaredVars.back();
			if (vars)
			{
				delete vars;
				declaredVars.pop_back();
			}
		}
	}

	inline void printTabs()
	{
		int d = traverseDepths[traverseDepthsPointer].depth;
		for (int i = 0; i < d; i++)
		{
			out << "  ";
		}
	}

	inline bool isStatement()
	{
		return traverseDepths[traverseDepthsPointer].isStatement;
	}

	bool isVarDeclared(TString &name)
	{
		for (TVector<TVector<TString>*>::reverse_iterator it = declaredVars.rbegin(); it != declaredVars.rend(); it++)
		{
			auto vars = *it;
			for (TVector<TString>::iterator it2 = vars->begin(); it2 != vars->end(); it2++)
			{
				auto var = *it2;
				if (name == var)
				{
					return true;
				}
			}
		}
		return false;
	}

	void declareVar(TString &name)
	{
		auto vars = declaredVars.back();
		if (vars)
		{
			vars->push_back(name);
		}
		else
		{
			//TODO: throw
		}
	}

	inline bool hasCapability(ShaderCapability capability)
	{
		return ShaderCapabilityHas(shaderCapabilities, capability);
	}
};

#endif