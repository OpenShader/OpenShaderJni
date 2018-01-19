#include "Traverser.h"
#include "localintermediate.h"
#include "Types.h"
#include "dtoa/dtoa_milo.h"
#include "../../glslang/SPIRV/spirv.hpp"

#define ITERATE_BEGIN(type, seq, var) for (type::iterator sit = seq.begin(); sit != seq.end(); sit++) { auto var = *sit;
#define ITERATE_END }
#define endl '\n'

bool IsInfinity(double x) {
#ifdef _MSC_VER
	switch (_fpclass(x)) {
	case _FPCLASS_NINF:
	case _FPCLASS_PINF:
		return true;
	default:
		return false;
	}
#else
	return std::isinf(x);
#endif
}

bool IsNan(double x) {
#ifdef _MSC_VER
	switch (_fpclass(x)) {
	case _FPCLASS_SNAN:
	case _FPCLASS_QNAN:
		return true;
	default:
		return false;
	}
#else
	return std::isnan(x);
#endif
}

bool ArrangingTraverser::visitBinary(TVisit, TIntermBinary * node)
{
	return true;
}

bool ArrangingTraverser::visitUnary(TVisit, TIntermUnary* node)
{
	return true;
}

bool ArrangingTraverser::visitAggregate(TVisit, TIntermAggregate* node)
{
	switch (node->getOp())
	{
	case EOpSequence:
		if (depth == 1)
		{
			initSeq = node;
			return false;
		}
		break;
	case EOpLinkerObjects:
		auto seq = node->getSequence();
		for (TIntermSequence::iterator sit = seq.begin(); sit != seq.end(); sit++) 
		{
			auto n = *sit;
			TIntermSymbol *symbol = n->getAsSymbolNode();
			if (symbol != nullptr) // TODO: Throw an exception if it's not symbol.
			{
				symbolTable.push_back(symbol);
			}
		}
		return false; // Do not traverse linked objects because we have already added all symbols to the table.
	}
	return true;
}

bool ArrangingTraverser::visitSelection(TVisit, TIntermSelection* node)
{
	return true;
}

void ArrangingTraverser::visitConstantUnion(TIntermConstantUnion* node)
{
}

void ArrangingTraverser::findStruct(TType& structRef)
{
	if (structTable.find(structRef.getTypeName()) != structTable.end())
		return;
	auto seq = *const_cast<TTypeList*>(structRef.getStruct());
	ITERATE_BEGIN(TVector<TTypeLoc>, seq, member)
		if (member.type->getBasicType() == EbtStruct)
			findStruct(const_cast<TType&>(*member.type));
	ITERATE_END
	structTable[structRef.getTypeName()] = const_cast<TTypeList*>(structRef.getStruct());
	structEmittingOrder.push_back(structRef.getTypeName());
}

void ArrangingTraverser::visitSymbol(TIntermSymbol* node)
{
	if (node->getBasicType() == EbtStruct)
	{
		findStruct(const_cast<TType&>(node->getType()));
	}
}

bool ArrangingTraverser::visitLoop(TVisit, TIntermLoop* node)
{
	return true;
}

bool ArrangingTraverser::visitBranch(TVisit, TIntermBranch* node)
{
	return true;
}

bool ArrangingTraverser::visitSwitch(TVisit, TIntermSwitch* node)
{
	return true;
}

void ArrangingTraverser::process(TIntermediate * intermediate, TPoolAllocator * allocator)
{
	intermediate->getTreeRoot()->traverse(this);
}

#define QUALIFIER_DOT (num++ > 0 ? ", " : "")

TString EmitterTraverser::process(ArrangingTraverser &arranging_traverser, TIntermediate *intermediate, TPoolAllocator *allocator)
{
	// VERSION
	//auto shaderType = intermediate->getStage();
	arranging_traverser.process(intermediate, allocator);
	this->symbolTable = &arranging_traverser.symbolTable;
	this->structTable = &arranging_traverser.structTable;
	this->structEmittingOrder = &arranging_traverser.structEmittingOrder;
	this->initSeq = arranging_traverser.initSeq;
	this->intermediate = intermediate;
	auto version = intermediate->getVersion();
	auto compatibility = (intermediate->getProfile() == ECompatibilityProfile ? true : false);
	out << "#version " << version << (compatibility ? " compatibility" : "") << endl;

	// STRUCTS
	{
		auto order = *structEmittingOrder;
		ITERATE_BEGIN(TVector<TString>, order, key)
			if (key.find("gl_") == 0)
				continue;
			auto value = structTable->find(key);
			if (value != structTable->end())
			{
				out << "struct " << value->first << " {" << endl;
				pushDepth();
				auto seq = *value->second;
				ITERATE_BEGIN(TVector<TTypeLoc>, seq, member)
					printTabs();
					emitSymbol(*member.type, true, true, member.type->getFieldName().c_str());
				ITERATE_END
				popDepth();
				out << "};" << endl;
			}
		ITERATE_END
	}

	// SHADER QUALIFIER
	{
		// not supports numViews and layoutOverrideCoverage
		switch (intermediate->getStage())
		{
		case EShLangTessControl:
			out << "layout(vertices = " << intermediate->getVertices() << ") out;" << endl;
			break;
		case EShLangTessEvaluation:
			{
				auto num = 0;
				out << "layout(";
				if (intermediate->getInputPrimitive() != ElgNone)
					out << QUALIFIER_DOT << TQualifier::getGeometryString(intermediate->getInputPrimitive());
				if (intermediate->getVertexSpacing() != EvsNone)
					out << QUALIFIER_DOT << TQualifier::getVertexSpacingString(intermediate->getVertexSpacing());
				if (intermediate->getVertexOrder() != EvoNone)
					out << QUALIFIER_DOT << TQualifier::getVertexOrderString(intermediate->getVertexOrder());
				if (intermediate->getPointMode())
					out << QUALIFIER_DOT << "point_mode";
				out << ") in;" << endl;
			}
			break;
		case EShLangGeometry:
			out << "layout(" << TQualifier::getGeometryString(intermediate->getInputPrimitive()) << ") in;" << endl;
			out << "layout(" << TQualifier::getGeometryString(intermediate->getOutputPrimitive())
				<< ", max_vertices = " << intermediate->getVertices() << ") out;" << endl;
			if (intermediate->getInvocations() != TQualifier::layoutNotSet)
				out << "layout(invocations = " << intermediate->getInvocations() << ") in;" << endl;
			break;
		case EShLangFragment:
			if (intermediate->getPostDepthCoverage())
				out << "layout(early_fragment_tests, post_depth_coverage) in;" << endl;
			else if (intermediate->getEarlyFragmentTests())
				out << "layout(early_fragment_tests) in;" << endl;

			if (intermediate->getBlendEquations() != 0)
			{
				auto enums = intermediate->getBlendEquations();
				auto num = 0;
				out << "layout(";
				for (int i = 0; i < EBlendCount; i++)
				{
					if ((enums & (1 << i)) > 0)
						out << QUALIFIER_DOT << TQualifier::getBlendEquationString(static_cast<TBlendEquationShift>(i));
				}
				out << ") out;" << endl;
			}

			if (intermediate->getOriginUpperLeft() && !intermediate->getPixelCenterInteger())
				out << "layout(origin_upper_left) in vec4 gl_FragCoord;" << endl;
			else if (!intermediate->getOriginUpperLeft() && intermediate->getPixelCenterInteger())
				out << "layout(pixel_center_integer) in vec4 gl_FragCoord;" << endl;
			else if (intermediate->getOriginUpperLeft() && intermediate->getPixelCenterInteger())
				out << "layout(origin_upper_left, pixel_center_integer) in vec4 gl_FragCoord;" << endl;

			if (intermediate->getDepth() != EldNone)
				out << "layout (" << TQualifier::getLayoutDepthString(intermediate->getDepth()) 
					<< ") out float gl_FragDepth;" << endl;

			break;
		case EShLangCompute:
			out << "layout(local_size_x = " << intermediate->getLocalSize(0)
				<< ", local_size_y = " << intermediate->getLocalSize(1)
				<< ", local_size_z = " << intermediate->getLocalSize(2);
			if (intermediate->getLocalSizeSpecId(0) != TQualifier::layoutNotSet)
				out << ", local_size_x_id = " << intermediate->getLocalSizeSpecId(0);
			if (intermediate->getLocalSizeSpecId(1) != TQualifier::layoutNotSet)
				out << ", local_size_y_id = " << intermediate->getLocalSizeSpecId(1);
			if (intermediate->getLocalSizeSpecId(2) != TQualifier::layoutNotSet)
				out << ", local_size_z_id = " << intermediate->getLocalSizeSpecId(2);
			out << ") in;" << endl;
			break;
		default:
			break;
		}
	}

	// SYMBOLS
	{
		auto seq = symbolTable;
		for (TVector<TIntermSymbol*>::iterator sit = seq->begin(); sit != seq->end(); sit++)
		{
			auto n = *sit;
			auto qualifier = n->getQualifier();
			auto storage = qualifier.storage;
			if ((storage == EvqUniform && hasCapability(UNIFORM_BLOCK))
				/* || (storage == EvqVaryingIn && shaderType == EShLangVertex)*/
				|| storage == EvqConst)
				continue;
			emitSymbol(n, true, true);
		}
	}
	if (hasCapability(UNIFORM_BLOCK))
	{
		auto map = uniformBlocks;
		for (TMap<TString, TVector<TString>*>::iterator sit = map->begin(); sit != map->end(); sit++)
		{
			auto entry = *sit;
			out << "layout (std140) uniform " << entry.first << endl;
			out << "{" << endl;
			{
				auto seq = entry.second;
				for (TVector<TString>::iterator sit = seq->begin(); sit != seq->end(); sit++)
				{
					auto n = *sit;
					out << "	" << n << ";" << endl;
				}
			}
			out << "};" << endl;
		}
	}
	intermediate->getTreeRoot()->traverse(this);
	return printOut(allocator);
}

void EmitterTraverser::emitSymbol(const TType& type, bool isDeclaration, bool hasSemicolon, const char* symbolName)
{
	if (isDeclaration)
	{
		emitQualifier(type);
		TOperator op = intermediate->mapTypeToConstructorOp(type);
		switch (op)
		{
		case EOpConstructStruct:
			out << (type.getTypeName()) << " ";
			break;
		case EOpConstructTextureSampler:
			out << (type.getSampler().getString()) << " ";
			break;
		case EOpNull:
			if (type.getBasicType() == EbtBlock)
			{
				/*switch (symbol->getQualifier().storage)
				{
				case EvqVaryingIn: out << "in "; break;
				case EvqVaryingOut: out << "out "; break;
				case EvqUniform: out << "uniform "; break;
				case EvqBuffer: out << "buffer "; break;
				}*/
				out << type.getTypeName() << " {" << endl;
				pushDepth();
				auto blockMembers = const_cast<TTypeList*>(type.getStruct());
				ITERATE_BEGIN(TVector<TTypeLoc>, (*blockMembers), member)
					printTabs();
					out << getType(*member.type) << " ";
					out << member.type->getFieldName();
					out << ";" << endl;
				ITERATE_END
				popDepth();
				out << "} ";
			}
			else if (type.getBasicType() == EbtVoid) // should not happen. TODO: consider it a bug?
			{
				out << "void ";
			}
			break; // TODO: throw
		default:
			out << getTypeConstruction(op) << " ";
			break;
		}
	}
	out << ((symbolName != nullptr) ? symbolName : type.getFieldName());
	if (isDeclaration && type.isArray())
	{
		auto sizes = type.getArraySizes();
		int dims = sizes->getNumDims();
		for (int i = 0; i < dims; i++)
		{
			int size = sizes->getDimSize(i);
			if (size == 0)
				out << "[]";
			else
				out << "[" << size << "]";
		}
	}
	if (hasSemicolon)
	{
		out << ";" << endl;
	}
}

void EmitterTraverser::emitQualifier(const TType & type)
{
	auto qualifier = type.getQualifier();
	int num = 0;
	if (qualifier.hasLayout()) {
		TQualifier noXfbBuffer = qualifier;
		noXfbBuffer.layoutXfbBuffer = TQualifier::layoutXfbBufferEnd;
		if (noXfbBuffer.hasLayout()) {
			out << "layout(";
			if (qualifier.hasAnyLocation()) {
				out << QUALIFIER_DOT <<  "location=" << qualifier.layoutLocation;
				if (qualifier.hasComponent()) {
					out << QUALIFIER_DOT << "component=" << qualifier.layoutComponent;
				}
				if (qualifier.hasIndex()) {
					out << QUALIFIER_DOT << "index=" << qualifier.layoutIndex;
				}
			}
			if (qualifier.hasSet()) {
				out << QUALIFIER_DOT << "set=" << qualifier.layoutSet;
			}
			if (qualifier.hasBinding()) {
				out << QUALIFIER_DOT << "binding=" << qualifier.layoutBinding;
			}
			if (qualifier.hasStream()) {
				out << QUALIFIER_DOT << "stream=" << qualifier.layoutStream;
			}
			if (qualifier.hasMatrix()) {
				out << QUALIFIER_DOT << TQualifier::getLayoutMatrixString(qualifier.layoutMatrix);
			}
			if (qualifier.hasPacking()) {
				out << QUALIFIER_DOT << TQualifier::getLayoutPackingString(qualifier.layoutPacking);
			}
			if (qualifier.hasOffset()) {
				out << QUALIFIER_DOT << "offset=" << qualifier.layoutOffset;
			}
			if (qualifier.hasAlign()) {
				out << QUALIFIER_DOT << "align=" << qualifier.layoutAlign;
			}
			if (qualifier.hasFormat()) {
				out << QUALIFIER_DOT << TQualifier::getLayoutFormatString(qualifier.layoutFormat);
			}
			if (qualifier.hasXfbBuffer() && qualifier.hasXfbOffset()) {
				out << QUALIFIER_DOT << "xfb_buffer=" << qualifier.layoutXfbBuffer;
			}
			if (qualifier.hasXfbOffset()) {
				out << QUALIFIER_DOT << "xfb_offset=" << qualifier.layoutXfbOffset;
			}
			if (qualifier.hasXfbStride()) {
				out << QUALIFIER_DOT << "xfb_stride=" << qualifier.layoutXfbStride;
			}
			if (qualifier.hasAttachment()) {
				out << QUALIFIER_DOT << "input_attachment_index=" << qualifier.layoutAttachment;
			}
			if (qualifier.hasSpecConstantId()) {
				out << QUALIFIER_DOT << "constant_id=" << qualifier.layoutSpecConstantId;
			}
			if (qualifier.layoutPushConstant)
				out << QUALIFIER_DOT << "push_constant";

#ifdef NV_EXTENSIONS
			if (qualifier.layoutPassthrough)
				out << QUALIFIER_DOT << "passthrough";
			if (qualifier.layoutViewportRelative)
				out << QUALIFIER_DOT << "layoutViewportRelative";
			if (qualifier.layoutSecondaryViewportRelativeOffset != -2048) {
				out << QUALIFIER_DOT << "layoutSecondaryViewportRelativeOffset=" << qualifier.layoutSecondaryViewportRelativeOffset;
			}
#endif
			out << ") ";
		}
	}
	if (qualifier.invariant)
		out << "invariant ";
	if (qualifier.noContraction) // should be spiv-r only...
		out << "noContraction ";
	if (qualifier.centroid)
		out << "centroid ";
	if (qualifier.smooth && hasCapability(GLSL130))
		out << "smooth ";
	if (qualifier.flat && hasCapability(GLSL130))
		out << "flat ";
	if (qualifier.nopersp && hasCapability(GLSL130))
		out << "noperspective ";
#ifdef AMD_EXTENSIONS
	if (qualifier.explicitInterp)
		out << "__explicitInterpAMD ";
#endif
	if (qualifier.patch)
		out << "patch ";
	if (qualifier.sample)
		out << "sample ";
	if (qualifier.coherent)
		out << "coherent ";
	if (qualifier.volatil)
		out << "volatile ";
	if (qualifier.restrict)
		out << "restrict ";
	if (qualifier.readonly)
		out << "readonly ";
	if (qualifier.writeonly)
		out << "writeonly ";
	if (qualifier.specConstant)
		out << "specialization-constant ";
	switch (qualifier.storage)
	{
	case EvqConst: out << "const "; break;
	case EvqVaryingIn: 
	{
		if (hasCapability(GLSL150))
		{
			out << "in ";
		}
		else
		{
			if (intermediate->getStage() == EShLangVertex)
				out << "attribute ";
			else
				out << "varying ";
		}
		break;
	}
	case EvqVaryingOut:
	{
		if (hasCapability(GLSL150))
		{
			out << "out ";
		}
		else
		{
			out << "varying ";
		}
		break;
	}
	case EvqUniform: out << "uniform "; break;
	case EvqBuffer: out << "buffer "; break;
	case EvqShared: out << "shared "; break; // not sure
	case EvqIn: out << "in "; break;
	case EvqOut: out << "out "; break;
	case EvqInOut: out << "inout "; break;
	case EvqConstReadOnly: out << "const "; break;
	default: break;
	}
}

#define EMIT_BINARY(op) emitBinary(op, left, right)

bool EmitterTraverser::emitBinary(const char * op, TIntermNode * left, TIntermNode * right, bool brackets)
{
	if (brackets)
		out << "(";
	left->traverse(this);
	out << " " << op << " ";
	right->traverse(this);
	if (brackets)
		out << ")";
	return false;
}

bool isSingleAssignmentSequence(TIntermAggregate * node)
{
	auto seq = node->getSequence();
	if (seq.size() == 1)
	{
		auto assignNode = seq.front()->getAsBinaryNode();
		if (assignNode != nullptr)
		{
			return assignNode->getOp() == EOpAssign;
		}
	}
	return false;
}

bool isForLoop(TIntermAggregate * node)
{
	// check if this sequence is a for-loop
	auto seq = node->getSequence();
	if (seq.size() <= 2 && seq.back() && seq.back()->getAsLoopNode() && seq.back()->getAsLoopNode()->getTerminal())
	{
		return true;
	}
	return false;
}

bool EmitterTraverser::emitStatements(TIntermNode * node)
{
	if (node->getAsAggregate() && node->getAsAggregate()->getOp() == EOpSequence)
	{
		// don't check if the root sequence is SAS. we don't care.
		auto seq = node->getAsAggregate()->getSequence();
		ITERATE_BEGIN(TIntermSequence, seq, child)
			auto sequencedChild = child->getAsAggregate();
			if (sequencedChild != nullptr && sequencedChild->getOp() == EOpSequence)
			{
				if (isForLoop(sequencedChild))
				{
					auto childSeq = sequencedChild->getSequence();
					printTabs();
					out << "for (";
					/*
					* if a for-loop looks like:
					* int i = 0;
					* for (; i < 10; i++) { ... }
					* then it won't have the "init" block.
					*/
					if (!childSeq.front()->getAsLoopNode()) // if it has "init"
					{
						childSeq.front()->traverse(this); // emit head
					}
					out << "; ";
					auto looper = childSeq.back()->getAsLoopNode();
					if (looper->getTest())
						looper->getTest()->traverse(this);
					out << "; ";
					if (looper->getTerminal())
						looper->getTerminal()->traverse(this);
					out << ") {" << endl;
					pushDepth();
					if (looper->getBody())
						emitStatements(looper->getBody());
					popDepth();
					printTabs();
					out << "}" << endl;
					continue;
				}
				// if a sequence is SAS. we know that it actually is a declaration statement
				// if it isn't, then it is a true sequence.
				if (!isSingleAssignmentSequence(sequencedChild))
				{
					printTabs();
					out << "{" << endl;
					pushDepth();
					emitStatements(child);
					popDepth();
					printTabs();
					out << "}" << endl;
					continue;;
				}
			}
			printTabs();
			child->traverse(this);
			if (!child->getAsBranchNode() && !child->getAsSelectionNode())
				out << ";" << endl;
		ITERATE_END
	}
	else
	{
		printTabs();
		node->traverse(this);
		if (!node->getAsBranchNode() && !node->getAsSelectionNode())
			out << ";" << endl;
	}
	return false;
}

bool EmitterTraverser::emitInvoking(const char* head, TVector<TIntermNode*>& seq)
{
	out << head << "(";
	int i = 0;
	ITERATE_BEGIN(TIntermSequence, seq, child)
		if (i++ > 0)
			out << ", ";
		child->traverse(this);
	ITERATE_END
	out << ")";
	return false;
}

bool EmitterTraverser::emitConstruction(TOperator& op, TType & type, TVector<TIntermNode*>& seq)
{
	// XXX: replace with emitSymbol ?
	switch (op)
	{
	case EOpConstructStruct:
		out << type.getTypeName().c_str();
		break;
	case EOpConstructTextureSampler:
		out << type.getSampler().getString().c_str();
		break;
	default:
		out << getTypeConstruction(op);
		break;
	}
	if (type.isArray())
	{
		auto sizes = &type.getArraySizes();
		int dims = sizes->getNumDims();
		for (int i = 0; i < dims; i++)
		{
			int size = sizes->getDimSize(i);
			if (size == 0)
				out << "[]";
			else
				out << "[" << size << "]";
		}
	}
	return emitInvoking("", seq);
}

bool EmitterTraverser::emitEquation(const char * symbol, TVector<TIntermNode*>& seq)
{
	out << "(";
	seq.at(0)->traverse(this);
	out << " " << symbol << " ";
	seq.at(1)->traverse(this);
	out << ")";
	return false;
}

bool EmitterTraverser::emitUnary(const char * op, TIntermNode * node)
{
	out << op << "(";
	node->traverse(this);
	out << ")";
	return false;
}

void EmitterTraverser::emitConst(int & index, TConstUnionArray & constArray, TType & type, int maxArrayDim)
{
	if (type.isArray())
	{
		auto op = intermediate->mapTypeToConstructorOp(type);
		switch (op)
		{
		case EOpConstructStruct:
			out << type.getTypeName().c_str();
			break;
		case EOpConstructTextureSampler:
			out << type.getSampler().getString().c_str();
			break;
		default:
			out << getTypeConstruction(op);
			break;
		}
		auto sizes = &type.getArraySizes();
		int dims = sizes->getNumDims();
		int outputDims = std::min(dims, maxArrayDim);
		for (int i = (dims - outputDims); i < dims; i++)
		{
			int size = sizes->getDimSize(i);
			if (size == 0)
				out << "[]";
			else
				out << "[" << size << "]";
		}
		maxArrayDim = outputDims - 1;
		
		out << "(";
		int num = 0;
		if (maxArrayDim > 0)
		{
			for (int maxDim = sizes->getDimSize(dims - outputDims); maxDim > 0; maxDim--)
			{
				out << QUALIFIER_DOT;
				emitConst(index, constArray, type, maxArrayDim);
			}
		}
		else
		{
			for (int maxDim = sizes->getDimSize(dims - outputDims); maxDim > 0; maxDim--)
			{
				out << QUALIFIER_DOT;
				emitNonArrayConst(index, constArray, type);
			}
		}
		out << ")";
	}
	else
	{
		emitNonArrayConst(index, constArray, type);
	}
}

void EmitterTraverser::emitNonArrayConst(int & index, TConstUnionArray & constArray, TType & type)
{
	if (type.isStruct())
	{
		int num = 0;
		out << type.getTypeName().c_str() << "(";
		auto structMembers = *type.getStruct();
		ITERATE_BEGIN(TVector<TTypeLoc>, structMembers, member)
			out << QUALIFIER_DOT;
			emitConst(index, constArray, *member.type);
		ITERATE_END
		out << ")";
	}
	else
	{
		int size = 1;
		auto isVec = type.isVector() || type.isMatrix();
		if (isVec)
		{
			auto op = intermediate->mapTypeToConstructorOp(type);
			switch (op)
			{
			case EOpConstructTextureSampler:
				out << type.getSampler().getString().c_str();
				break;
			default:
				out << getTypeConstruction(op);
				break;
			}
			out << "(";
			size = type.isVector() ? type.getVectorSize() : (type.getMatrixCols() * type.getMatrixRows());
			bool allSame = true;
			for (int i = 1; i < size; i++)
			{
				if (constArray[index + i] != constArray[index])
				{
					allSame = false;
					break;
				}
			}
			if (allSame)
			{
				index += size - 1;
				size = 1;
			}
		}

		for (int i = 0; i < size; i++) {
			if (i > 0)
				out << ", ";
			switch (constArray[index].getType()) {
			case EbtBool:
				if (constArray[index].getBConst())
					out << "true";
				else
					out << "false";
				break;
			case EbtFloat:
			case EbtDouble:
#ifdef AMD_EXTENSIONS
			case EbtFloat16:
#endif
			{
				const double value = constArray[index].getDConst();
				if (IsInfinity(value)) {
					if (value < 0)
						out << "(-1.0 / 0.0)";
					else
						out << "(1.0 / 0.0)";
				}
				else if (IsNan(value))
					out << "(0.0 / 0.0)";
				else {
					const int maxSize = 128;
					char buf[maxSize];
					dtoa_milo(value, buf);
					out << buf;
				}
			}
			break;
			case EbtInt:
			{
				out << constArray[index].getIConst();
			}
			break;
			case EbtUint:
			{
				out << constArray[index].getUConst();
			}
			break;
			case EbtInt64:
			{
				out << constArray[index].getI64Const();
			}
			break;
			case EbtUint64:
			{
				out << constArray[index].getU64Const();
			}
			break;
#ifdef AMD_EXTENSIONS
			case EbtInt16:
			{
				out << constArray[index].getIConst();
			}
			break;
			case EbtUint16:
			{
				out << constArray[index].getUConst();
			}
			break;
#endif
			default:
				break;
				// TODO: throw
			}
			index++;
		}

		if (isVec)
		{
			out << ")";
		}
	}
}

const char* EmitterTraverser::getType(const TType & type)
{
	/*auto basicType = type.getBasicType();
	switch (basicType)
	{
	case EbtVoid: out << "void"; break;
	case EbtFloat: out << "float"; break;
	case EbtDouble: out << "double"; break;
	case EbtFloat16: out << "float16_t"; break;
	case EbtInt: out << "int"; break;
	case EbtUint: out << "uint"; break;
	case EbtInt64: out << "int64_t"; break;
	case EbtUint64: out << "uint64_t"; break;
	case EbtInt16: out << "int16_t"; break;
	case EbtUint16: out << "uint16_t"; break;
	case EbtBool: out << "bool"; break;
	case EbtAtomicUint: out << "atomic_uint"; break;
	case EbtSampler: out << type.getSampler().getString(); break;
	case EbtStruct: break; // TODO:
	case EbtBlock: break; // TODO:
	case EbtString: break;
	case EbtNumTypes: break;
	}
	return "";*/
	TOperator op = intermediate->mapTypeToConstructorOp(type);
	switch (op)
	{
	case EOpConstructStruct:
		return type.getTypeName().c_str();
	case EOpConstructTextureSampler:
		return type.getSampler().getString().c_str();
	case EOpNull: // block
		if (type.getBasicType() == EbtVoid)
			return "void";
		if (type.getBasicType() == EbtBlock)
			return type.getTypeName().c_str();
		return ""; // TODO: throw
	default:
		return getTypeConstruction(op);
	}
}

const char* EmitterTraverser::getOriginTextureFunction(TOperator & op, TVector<TIntermNode*>& seq)
{
	auto samplerNode = (*seq.front()).getAsSymbolNode();
	if (samplerNode == nullptr)
	{
		return ""; // TODO: throw
	}
	auto sampler = samplerNode->getType().getSampler();
	if (ShaderCapabilityHas(shaderCapabilities, GLSL130))
	{
		switch (op)
		{
		case EOpTexture: return "texture";
		case EOpTextureProj: return "textureProj";
		case EOpTextureLod: return "textureLod";
		case EOpTextureOffset: return "textureOffset";
		case EOpTextureFetch: return "texelFetch";
		case EOpTextureFetchOffset: return "texelFetchOffset";
		case EOpTextureProjOffset: return "textureProjOffset";
		case EOpTextureLodOffset: return "textureLodOffset";
		case EOpTextureProjLod: return "textureProjLod";
		case EOpTextureProjLodOffset: return "textureProjLodOffset";
		case EOpTextureGrad: return "textureGrad";
		case EOpTextureGradOffset: return "textureGradOffset";
		case EOpTextureProjGrad: return "textureProjGrad";
		case EOpTextureProjGradOffset: return "textureProjGradOffset";
		case EOpTextureGather: return "textureGather";
		case EOpTextureGatherOffset: return "textureGatherOffset";
		case EOpTextureGatherOffsets: return "textureGatherOffsets";
		case EOpTextureClamp: return "textureClampARB";
		case EOpTextureOffsetClamp: return "textureOffsetClampARB";
		case EOpTextureGradClamp: return "textureGradClampARB";
		case EOpTextureGradOffsetClamp: return "textureGradOffsetClampARB";
		default:
			return ""; // TODO: throw
		}
	}
	else
	{
		switch (op)
		{
		case EOpTexture:
			switch (sampler.dim)
			{
			case Esd1D:
				if (sampler.isShadow())
					return "shadow1D";
				return "texture1D";
			case Esd2D:
				if (sampler.isShadow())
					return "shadow2D";
				return "texture2D";
			case Esd3D:
				return "texture3D";
			case EsdCube:
				return "textureCube";
			}
			break;
		case EOpTextureProj:
			switch (sampler.dim)
			{
			case Esd1D:
				if (sampler.isShadow())
					return "shadow1DProj";
				return "texture1DProj";
			case Esd2D:
				if (sampler.isShadow())
					return "shadow2DProj";
				return "texture2DProj";
			case Esd3D:
				return "texture3DProj";
			}
			break;
		case EOpTextureLod:
			switch (sampler.dim)
			{
			case Esd1D:
				if (sampler.isShadow())
					return "shadow1DLod";
				return "texture1DLod";
			case Esd2D:
				if (sampler.isShadow())
					return "shadow2DLod";
				return "texture2DLod";
			case Esd3D:
				return "texture3DLod";
			case EsdCube:
				return "textureCubeLod";
			}
			break;
		case EOpTextureProjLod:
			switch (sampler.dim)
			{
			case Esd1D:
				if (sampler.isShadow())
					return "shadow1DProjLod";
				return "texture1DProjLod";
			case Esd2D:
				if (sampler.isShadow())
					return "shadow2DProjLod";
				return "texture2DProjLod";
			case Esd3D:
				return "texture3DProjLod";
			}
			break;
		default:
			return ""; // TODO: throw
		}
	}
	return ""; // TODO: throw
}

const char* EmitterTraverser::getTypeConstruction(TOperator & op)
{
	switch (op)
	{
	case EOpConstructFloat: return "float";
	case EOpConstructDouble: return "double";

	case EOpConstructVec2: return "vec2";
	case EOpConstructVec3: return "vec3";
	case EOpConstructVec4: return "vec4";
	case EOpConstructDVec2: return "dvec2";
	case EOpConstructDVec3: return "dvec3";
	case EOpConstructDVec4: return "dvec4";
	case EOpConstructBool: return "bool";
	case EOpConstructBVec2: return "bvec2";
	case EOpConstructBVec3: return "bvec3";
	case EOpConstructBVec4: return "bvec4";
	case EOpConstructInt: return "int";
	case EOpConstructIVec2: return "ivec2";
	case EOpConstructIVec3: return "ivec3";
	case EOpConstructIVec4: return "ivec4";
	case EOpConstructUint: return "uint";
	case EOpConstructUVec2: return "uvec2";
	case EOpConstructUVec3: return "uvec3";
	case EOpConstructUVec4: return "uvec4";
	case EOpConstructInt64: return "int64_t";
	case EOpConstructI64Vec2: return "i64vec2";
	case EOpConstructI64Vec3: return "i64vec3";
	case EOpConstructI64Vec4: return "i64vec4";
	case EOpConstructUint64: return "uint64_t";
	case EOpConstructU64Vec2: return "u64vec2";
	case EOpConstructU64Vec3: return "u64vec3";
	case EOpConstructU64Vec4: return "u64vec4";
#ifdef AMD_EXTENSIONS
	case EOpConstructInt16: return "int16_t"; //TODO: not sure
	case EOpConstructI16Vec2: return "i16vec2";
	case EOpConstructI16Vec3: return "i16vec3";
	case EOpConstructI16Vec4: return "i16vec4";
	case EOpConstructUint16: return "uint16_t";
	case EOpConstructU16Vec2: return "u16vec2";
	case EOpConstructU16Vec3: return "u16vec3";
	case EOpConstructU16Vec4: return "u16vec4";
#endif
	case EOpConstructMat2x2: return "mat2";
	case EOpConstructMat2x3: return "mat2x3";
	case EOpConstructMat2x4: return "mat2x4";
	case EOpConstructMat3x2: return "mat3x2";
	case EOpConstructMat3x3: return "mat3";
	case EOpConstructMat3x4: return "mat3x4";
	case EOpConstructMat4x2: return "mat4x2";
	case EOpConstructMat4x3: return "mat4x3";
	case EOpConstructMat4x4: return "mat4";
	case EOpConstructDMat2x2: return "dmat2";
	case EOpConstructDMat2x3: return "dmat2x3";
	case EOpConstructDMat2x4: return "dmat2x4";
	case EOpConstructDMat3x2: return "dmat3x2";
	case EOpConstructDMat3x3: return "dmat3";
	case EOpConstructDMat3x4: return "dmat3x4";
	case EOpConstructDMat4x2: return "dmat4x2";
	case EOpConstructDMat4x3: return "dmat4x3";
	case EOpConstructDMat4x4: return "dmat4";
	case EOpConstructIMat2x2: return "imat2";
	case EOpConstructIMat2x3: return "imat2x3";
	case EOpConstructIMat2x4: return "imat2x4";
	case EOpConstructIMat3x2: return "imat3x2";
	case EOpConstructIMat3x3: return "imat3";
	case EOpConstructIMat3x4: return "imat3x4";
	case EOpConstructIMat4x2: return "imat4x2";
	case EOpConstructIMat4x3: return "imat4x3";
	case EOpConstructIMat4x4: return "imat4";
	case EOpConstructUMat2x2: return "umat2";
	case EOpConstructUMat2x3: return "umat2x3";
	case EOpConstructUMat2x4: return "umat2x4";
	case EOpConstructUMat3x2: return "umat3x2";
	case EOpConstructUMat3x3: return "umat3";
	case EOpConstructUMat3x4: return "umat3x4";
	case EOpConstructUMat4x2: return "umat4x2";
	case EOpConstructUMat4x3: return "umat4x3";
	case EOpConstructUMat4x4: return "umat4";
	case EOpConstructBMat2x2: return "bmat2";
	case EOpConstructBMat2x3: return "bmat2x3";
	case EOpConstructBMat2x4: return "bmat2x4";
	case EOpConstructBMat3x2: return "bmat3x2";
	case EOpConstructBMat3x3: return "bmat3";
	case EOpConstructBMat3x4: return "bmat3x4";
	case EOpConstructBMat4x2: return "bmat4x2";
	case EOpConstructBMat4x3: return "bmat4x3";
	case EOpConstructBMat4x4: return "bmat4";
#ifdef AMD_EXTENSIONS
	case EOpConstructFloat16: return "float16_t";
	case EOpConstructF16Vec2: return "f16vec2";
	case EOpConstructF16Vec3: return "f16vec3";
	case EOpConstructF16Vec4: return "f16vec4";
	case EOpConstructF16Mat2x2: return "f16mat2";
	case EOpConstructF16Mat2x3: return "f16mat2x3";
	case EOpConstructF16Mat2x4: return "f16mat2x4";
	case EOpConstructF16Mat3x2: return "f16mat3x2";
	case EOpConstructF16Mat3x3: return "f16mat3";
	case EOpConstructF16Mat3x4: return "f16mat3x4";
	case EOpConstructF16Mat4x2: return "f16mat4x2";
	case EOpConstructF16Mat4x3: return "f16mat4x3";
	case EOpConstructF16Mat4x4: return "f16mat4";
#endif
	default: return ""; // Throw
	}
}

bool EmitterTraverser::visitBinary(TVisit, TIntermBinary * node)
{
	auto left = node->getLeft();
	auto right = node->getRight();
	switch (node->getOp()) {
	case EOpAssign: return emitBinary("=", left, right, false);
	case EOpAddAssign: return EMIT_BINARY("+=");
	case EOpSubAssign: return EMIT_BINARY("-=");
	case EOpMulAssign: return EMIT_BINARY("*=");
	case EOpVectorTimesMatrixAssign: return EMIT_BINARY("*=");
	case EOpVectorTimesScalarAssign: return EMIT_BINARY("*=");
	case EOpMatrixTimesScalarAssign: return EMIT_BINARY("*=");
	case EOpMatrixTimesMatrixAssign: return EMIT_BINARY("*=");
	case EOpDivAssign: return EMIT_BINARY("/=");
	case EOpModAssign: return EMIT_BINARY("%=");
	case EOpAndAssign: return EMIT_BINARY("&=");
	case EOpInclusiveOrAssign: return EMIT_BINARY("|=");
	case EOpExclusiveOrAssign: return EMIT_BINARY("^=");
	case EOpLeftShiftAssign: return EMIT_BINARY("<<=");
	case EOpRightShiftAssign: return EMIT_BINARY(">>=");

	case EOpIndexDirect:
	case EOpIndexIndirect:
		{
			left->traverse(this);
			out << "[";
			right->traverse(this);
			out << "]";
		}
		break;
	case EOpIndexDirectStruct:
		{
			left->traverse(this);
			out << "." << 
				(*node->getLeft()->getType().getStruct())[
					node->getRight()->getAsConstantUnion()->getConstArray()[0].getIConst()
				].type->getFieldName();
		}
		break;
	case EOpVectorSwizzle:
		{
			left->traverse(this);
			out << ".";
			auto seq = right->getAsAggregate()->getSequence();
			ITERATE_BEGIN(TIntermSequence, seq, child)
				auto c = child->getAsConstantUnion();
				int index = c->getConstArray()[0].getIConst();
				char comp = '?';
				switch (index)
				{
				case 0: comp = 'x'; break;
				case 1: comp = 'y'; break;
				case 2: comp = 'z'; break;
				case 3: comp = 'w'; break;
				}
				out << comp;
			ITERATE_END
		}
		break;
	case EOpMatrixSwizzle: break; // TODO: It's a hlsl op.

	case EOpAdd: return EMIT_BINARY("+");
	case EOpSub: return EMIT_BINARY("-");
	case EOpMul: return EMIT_BINARY("*");
	case EOpDiv: return EMIT_BINARY("/");
	case EOpMod: return EMIT_BINARY("%");
	case EOpRightShift: return EMIT_BINARY(">>");
	case EOpLeftShift: return EMIT_BINARY("<<");
	case EOpAnd: return EMIT_BINARY("&");
	case EOpInclusiveOr: return EMIT_BINARY("|");
	case EOpExclusiveOr: return EMIT_BINARY("^");
	case EOpEqual: return EMIT_BINARY("==");
	case EOpNotEqual: return EMIT_BINARY("!=");
	case EOpLessThan: return EMIT_BINARY("<");
	case EOpGreaterThan: return EMIT_BINARY(">");
	case EOpLessThanEqual: return EMIT_BINARY("<=");
	case EOpGreaterThanEqual: return EMIT_BINARY(">=");
	case EOpVectorEqual: return EMIT_BINARY("==");
	case EOpVectorNotEqual: return EMIT_BINARY("!=");

	case EOpVectorTimesScalar: return EMIT_BINARY("*");
	case EOpVectorTimesMatrix: return EMIT_BINARY("*");
	case EOpMatrixTimesVector: return EMIT_BINARY("*");
	case EOpMatrixTimesScalar: return EMIT_BINARY("*");
	case EOpMatrixTimesMatrix: return EMIT_BINARY("*");

	case EOpLogicalOr: return EMIT_BINARY("||");
	case EOpLogicalXor: return EMIT_BINARY("^^");
	case EOpLogicalAnd: return EMIT_BINARY("&&");

	default: break; // TODO: throw
	}
	return false;
}

bool EmitterTraverser::visitUnary(TVisit, TIntermUnary* node)
{
	auto operand = node->getOperand();
	auto op = node->getOp();
	switch (op) {
	case EOpNegative: out << "-"; operand->traverse(this); break;
	case EOpVectorLogicalNot:
	case EOpLogicalNot: out << "!"; operand->traverse(this); break;
	case EOpBitwiseNot: out << "~"; operand->traverse(this); break;

	case EOpPostIncrement: operand->traverse(this);  out << "++"; break;
	case EOpPostDecrement: operand->traverse(this);  out << "--"; break;
	case EOpPreIncrement: out << "++"; operand->traverse(this); break;
	case EOpPreDecrement: out << "--"; operand->traverse(this); break;

	case EOpConvIntToBool:
	case EOpConvUintToBool:
	case EOpConvFloatToBool:
	case EOpConvDoubleToBool:
	case EOpConvInt64ToBool:
	case EOpConvUint64ToBool: return emitUnary("bool", operand);
	case EOpConvIntToFloat:
	case EOpConvUintToFloat:
	case EOpConvDoubleToFloat:
	case EOpConvInt64ToFloat:
	case EOpConvUint64ToFloat:
	case EOpConvBoolToFloat: return emitUnary("float", operand);
	case EOpConvUintToInt:
	case EOpConvFloatToInt:
	case EOpConvDoubleToInt:
	case EOpConvBoolToInt:
	case EOpConvInt64ToInt:
	case EOpConvUint64ToInt: return emitUnary("int", operand);
	case EOpConvIntToUint:
	case EOpConvFloatToUint:
	case EOpConvDoubleToUint:
	case EOpConvBoolToUint:
	case EOpConvInt64ToUint:
	case EOpConvUint64ToUint: return emitUnary("uint", operand);
	case EOpConvIntToDouble:
	case EOpConvUintToDouble:
	case EOpConvFloatToDouble:
	case EOpConvBoolToDouble:
	case EOpConvInt64ToDouble:
	case EOpConvUint64ToDouble: return emitUnary("double", operand);
	case EOpConvBoolToInt64:
	case EOpConvIntToInt64:
	case EOpConvUintToInt64:
	case EOpConvFloatToInt64:
	case EOpConvDoubleToInt64:
	case EOpConvUint64ToInt64: return emitUnary("int64_t", operand);
	case EOpConvBoolToUint64:
	case EOpConvIntToUint64:
	case EOpConvUintToUint64:
	case EOpConvFloatToUint64:
	case EOpConvDoubleToUint64:
	case EOpConvInt64ToUint64: return emitUnary("uint64_t", operand);

	case EOpRadians: return emitUnary("radians", operand);
	case EOpDegrees: return emitUnary("degrees", operand);
	case EOpSin: return emitUnary("sin", operand);
	case EOpCos: return emitUnary("cos", operand);
	case EOpTan: return emitUnary("tan", operand);
	case EOpAsin: return emitUnary("asin", operand);
	case EOpAcos: return emitUnary("acos", operand);
	case EOpAtan: return emitUnary("atan", operand);
	case EOpSinh: return emitUnary("sinh", operand);
	case EOpCosh: return emitUnary("cosh", operand);
	case EOpTanh: return emitUnary("tanh", operand);
	case EOpAsinh: return emitUnary("asinh", operand);
	case EOpAcosh: return emitUnary("acosh", operand);
	case EOpAtanh: return emitUnary("atanh", operand);

	case EOpExp: return emitUnary("exp", operand);
	case EOpLog: return emitUnary("log", operand);
	case EOpExp2: return emitUnary("exp2", operand);
	case EOpLog2: return emitUnary("log2", operand);
	case EOpSqrt: return emitUnary("sqrt", operand);
	case EOpInverseSqrt: return emitUnary("inversesqrt", operand);

	case EOpAbs: return emitUnary("abs", operand);
	case EOpSign: return emitUnary("sign", operand);
	case EOpFloor: return emitUnary("floor", operand);
	case EOpTrunc: return emitUnary("trunc", operand);
	case EOpRound: return emitUnary("round", operand);
	case EOpRoundEven: return emitUnary("roundEven", operand);
	case EOpCeil: return emitUnary("ceil", operand);
	case EOpFract: return emitUnary("fract", operand);

	case EOpIsNan: return emitUnary("isnan", operand);
	case EOpIsInf: return emitUnary("isinf", operand);

	case EOpFloatBitsToInt: return emitUnary("floatBitsToInt", operand);
	case EOpFloatBitsToUint: return emitUnary("floatBitsToUint", operand);
	case EOpIntBitsToFloat: return emitUnary("intBitsToFloat", operand);
	case EOpUintBitsToFloat: return emitUnary("uintBitsToFloat", operand);
	case EOpDoubleBitsToInt64: return emitUnary("doubleBitsToInt64", operand);
	case EOpDoubleBitsToUint64: return emitUnary("doubleBitsToUint64", operand);
	case EOpInt64BitsToDouble: return emitUnary("int64BitsToDouble", operand);
	case EOpUint64BitsToDouble: return emitUnary("uint64BitsToDouble", operand);
#ifdef AMD_EXTENSIONS
	case EOpFloat16BitsToInt16: return emitUnary("float16BitsToInt16", operand);
	case EOpFloat16BitsToUint16: return emitUnary("float16BitsToUint16", operand);
	case EOpInt16BitsToFloat16: return emitUnary("int16BitsToFloat16", operand);
	case EOpUint16BitsToFloat16: return emitUnary("uint16BitsToFloat16", operand);
#endif

	case EOpPackSnorm2x16: return emitUnary("packSnorm2x16", operand);
	case EOpUnpackSnorm2x16: return emitUnary("unpackSnorm2x16", operand);
	case EOpPackUnorm2x16: return emitUnary("packUnorm2x16", operand);
	case EOpUnpackUnorm2x16: return emitUnary("unpackUnorm2x16", operand);
	case EOpPackHalf2x16: return emitUnary("packHalf2x16", operand);
	case EOpUnpackHalf2x16: return emitUnary("unpackHalf2x16", operand);

	case EOpPackSnorm4x8: return emitUnary("packSnorm4x8", operand);
	case EOpUnpackSnorm4x8: return emitUnary("unpackSnorm4x8", operand);
	case EOpPackUnorm4x8: return emitUnary("packUnorm4x8", operand);
	case EOpUnpackUnorm4x8: return emitUnary("unpackUnorm4x8", operand);
	case EOpPackDouble2x32: return emitUnary("packDouble2x32", operand);
	case EOpUnpackDouble2x32: return emitUnary("unpackDouble2x32", operand);

	case EOpPackInt2x32: return emitUnary("packInt2x32", operand);
	case EOpUnpackInt2x32: return emitUnary("unpackInt2x32", operand);
	case EOpPackUint2x32: return emitUnary("packUint2x32", operand);
	case EOpUnpackUint2x32: return emitUnary("unpackUint2x32", operand);

#ifdef AMD_EXTENSIONS
	case EOpPackInt2x16: return emitUnary("packInt2x16", operand);
	case EOpUnpackInt2x16: return emitUnary("unpackInt2x16", operand);
	case EOpPackUint2x16: return emitUnary("packUint2x16", operand);
	case EOpUnpackUint2x16: return emitUnary("unpackUint2x16", operand);

	case EOpPackInt4x16: return emitUnary("packInt4x16", operand);
	case EOpUnpackInt4x16: return emitUnary("unpackInt4x16", operand);
	case EOpPackUint4x16: return emitUnary("packUint4x16", operand);
	case EOpUnpackUint4x16: return emitUnary("unpackUint4x16", operand);

	case EOpPackFloat2x16: return emitUnary("packFloat2x16", operand);
	case EOpUnpackFloat2x16: return emitUnary("unpackFloat2x16", operand);
#endif

	case EOpLength: return emitUnary("length", operand);
	case EOpNormalize: return emitUnary("normalize", operand);
	case EOpDPdx: return emitUnary("dFdx", operand);
	case EOpDPdy: return emitUnary("dFdy", operand);
	case EOpFwidth: return emitUnary("fwidth", operand);
	case EOpDPdxFine: return emitUnary("dFdxFine", operand);
	case EOpDPdyFine: return emitUnary("dFdyFine", operand);
	case EOpFwidthFine: return emitUnary("fwidthFine", operand);
	case EOpDPdxCoarse: return emitUnary("dFdxCoarse", operand);
	case EOpDPdyCoarse: return emitUnary("dFdyCoarse", operand);
	case EOpFwidthCoarse: return emitUnary("fwidthCoarse", operand);

	case EOpInterpolateAtCentroid: return emitUnary("", operand);

	case EOpDeterminant: return emitUnary("determinant", operand);
	case EOpMatrixInverse: return emitUnary("inverse", operand);
	case EOpTranspose: return emitUnary("transpose", operand);

	case EOpAny: return emitUnary("any", operand);
	case EOpAll: return emitUnary("all", operand);

	case EOpArrayLength: // get the size of an implicitly-sized array of a SSBO
		{
			out << "(";
			operand->traverse(this);
			out << ").length()";
		}
		break;

	case EOpEmitStreamVertex: return emitUnary("EmitStreamVertex", operand);
	case EOpEndStreamPrimitive: return emitUnary("EndStreamPrimitive", operand);

	case EOpAtomicCounterIncrement: return emitUnary("atomicCounterIncrement", operand);
	case EOpAtomicCounterDecrement: return emitUnary("atomicCounterDecrement", operand);
	case EOpAtomicCounter: return emitUnary("atomicCounter", operand);

	case EOpTextureQuerySize: return emitUnary("textureSize", operand);
	case EOpTextureQueryLod: return emitUnary("textureQueryLod", operand);
	case EOpTextureQueryLevels: return emitUnary("textureQueryLevels", operand);
	case EOpTextureQuerySamples: return emitUnary("textureSamples", operand);
	case EOpImageQuerySize: return emitUnary("imageSize", operand);
	case EOpImageQuerySamples: return emitUnary("imageSamples", operand);
	case EOpImageLoad: return emitUnary("imageLoad", operand);

	case EOpBitFieldReverse: return emitUnary("bitfieldReverse", operand);
	case EOpBitCount: return emitUnary("bitCount", operand);
	case EOpFindLSB: return emitUnary("findLSB", operand);
	case EOpFindMSB: return emitUnary("findMSB", operand);

	case EOpNoise:
		{
			auto size = node->getType().getVectorSize();
			switch (size)
			{
			case 1: return emitUnary("noise1", operand);
			case 2: return emitUnary("noise2", operand);
			case 3: return emitUnary("noise3", operand);
			case 4: return emitUnary("noise4", operand);
			default: break; // TODO: throw exception
			}
		}
		break;

	case EOpBallot: return emitUnary("ballotARB", operand);
	case EOpReadFirstInvocation: return emitUnary("readFirstInvocationARB", operand);

	case EOpAnyInvocation: return emitUnary("anyInvocationARB", operand); // TODO: non-arb version of 460?
	case EOpAllInvocations: return emitUnary("allInvocationsARB", operand);
	case EOpAllInvocationsEqual: return emitUnary("allInvocationsEqual", operand);

	case EOpClip: out << "if (("; operand->traverse(this); out << " < 0.0) { discard; }" << endl; break;
	case EOpIsFinite: return ""; // TODO: throw
	case EOpLog10: return ""; // TODO: throw;
	case EOpRcp: return ""; // TODO: throw
	case EOpSaturate: out << "clamp("; operand->traverse(this); out << ", 0.0, 1.0)"; break;

	case EOpSparseTexelsResident: return emitUnary("sparseTexelsResident", operand);

#ifdef AMD_EXTENSIONS
	case EOpMinInvocations: return emitUnary("minInvocationsAMD", operand);
	case EOpMaxInvocations: return emitUnary("maxInvocationsAMD", operand);
	case EOpAddInvocations: return emitUnary("addInvocationsAMD", operand);
	case EOpMinInvocationsNonUniform: return emitUnary("minInvocationsNonUniformAMD", operand);
	case EOpMaxInvocationsNonUniform: return emitUnary("maxInvocationsNonUniformAMD", operand);
	case EOpAddInvocationsNonUniform: return emitUnary("addInvocationsNonUniformAMD", operand);

	case EOpMinInvocationsInclusiveScan: return emitUnary("minInvocationsInclusiveScanAMD", operand);
	case EOpMaxInvocationsInclusiveScan: return emitUnary("maxInvocationsInclusiveScanAMD", operand);
	case EOpAddInvocationsInclusiveScan: return emitUnary("addInvocationsInclusiveScanAMD", operand);
	case EOpMinInvocationsInclusiveScanNonUniform: return emitUnary("minInvocationsInclusiveScanNonUniformAMD", operand);
	case EOpMaxInvocationsInclusiveScanNonUniform: return emitUnary("maxInvocationsInclusiveScanNonUniformAMD", operand);
	case EOpAddInvocationsInclusiveScanNonUniform: return emitUnary("addInvocationsInclusiveScanNonUniformAMD", operand);

	case EOpMinInvocationsExclusiveScan: return emitUnary("minInvocationsExclusiveScanAMD", operand);
	case EOpMaxInvocationsExclusiveScan: return emitUnary("maxInvocationsExclusiveScanAMD", operand);
	case EOpAddInvocationsExclusiveScan: return emitUnary("addInvocationsExclusiveScanAMD", operand);
	case EOpMinInvocationsExclusiveScanNonUniform: return emitUnary("minInvocationsExclusiveScanNonUniformAMD", operand);
	case EOpMaxInvocationsExclusiveScanNonUniform: return emitUnary("maxInvocationsExclusiveScanNonUniformAMD", operand);
	case EOpAddInvocationsExclusiveScanNonUniform: return emitUnary("addInvocationsExclusiveScanNonUniformAMD", operand);

	case EOpMbcnt: return emitUnary("mbcntAMD", operand);

	case EOpCubeFaceIndex: return emitUnary("cubeFaceIndexAMD", operand);
	case EOpCubeFaceCoord: return emitUnary("cubeFaceCoordAMD", operand);

	case EOpFragmentMaskFetch: return emitUnary("fragmentMaskFetchAMD", operand);
	case EOpFragmentFetch: return emitUnary("fragmentFetchAMD", operand);

	case EOpConvBoolToFloat16:
	case EOpConvIntToFloat16:
	case EOpConvUintToFloat16:
	case EOpConvFloatToFloat16:
	case EOpConvDoubleToFloat16:
	case EOpConvInt64ToFloat16:
	case EOpConvUint64ToFloat16: return emitUnary("float16_t", operand);
	case EOpConvFloat16ToBool:	 return emitUnary("bool", operand);
	case EOpConvFloat16ToInt:	 return emitUnary("int", operand);
	case EOpConvFloat16ToUint:	 return emitUnary("uint", operand);
	case EOpConvFloat16ToFloat:	 return emitUnary("float", operand);
	case EOpConvFloat16ToDouble: return emitUnary("double", operand);
	case EOpConvFloat16ToInt64:	 return emitUnary("int64_t", operand);
	case EOpConvFloat16ToUint64: return emitUnary("uint64_t", operand);

	case EOpConvBoolToInt16:
	case EOpConvIntToInt16:
	case EOpConvUintToInt16:
	case EOpConvFloatToInt16:
	case EOpConvDoubleToInt16:
	case EOpConvFloat16ToInt16:
	case EOpConvInt64ToInt16:
	case EOpConvUint64ToInt16:
	case EOpConvUint16ToInt16:  return emitUnary("int16_t", operand);
	case EOpConvInt16ToBool:    return emitUnary("bool", operand);
	case EOpConvInt16ToInt:     return emitUnary("int", operand);
	case EOpConvInt16ToUint:    return emitUnary("uint", operand);
	case EOpConvInt16ToFloat:   return emitUnary("float", operand);
	case EOpConvInt16ToDouble:  return emitUnary("double", operand);
	case EOpConvInt16ToFloat16: return emitUnary("float16_t", operand);
	case EOpConvInt16ToInt64:   return emitUnary("int64_t", operand);
	case EOpConvInt16ToUint64:  return emitUnary("uint64_t", operand);

	case EOpConvBoolToUint16:
	case EOpConvIntToUint16:
	case EOpConvUintToUint16:
	case EOpConvFloatToUint16:
	case EOpConvDoubleToUint16:
	case EOpConvFloat16ToUint16:
	case EOpConvInt64ToUint16:
	case EOpConvUint64ToUint16:
	case EOpConvInt16ToUint16:   return emitUnary("uint16_t", operand);
	case EOpConvUint16ToBool:    return emitUnary("bool", operand);
	case EOpConvUint16ToInt:     return emitUnary("int", operand);
	case EOpConvUint16ToUint:    return emitUnary("uint", operand);
	case EOpConvUint16ToFloat:   return emitUnary("float", operand);
	case EOpConvUint16ToDouble:  return emitUnary("double", operand);
	case EOpConvUint16ToFloat16: return emitUnary("float16_t", operand);
	case EOpConvUint16ToInt64:   return emitUnary("int64_t", operand);
	case EOpConvUint16ToUint64:  return emitUnary("uint64_t", operand);
#endif

	case EOpSubpassLoad: return emitUnary("subpassLoad", operand);
	case EOpSubpassLoadMS: return emitUnary("subpassLoadMS", operand);

	default: break; // TODO: throw
	}
	return false;
}

bool EmitterTraverser::visitAggregate(TVisit, TIntermAggregate* node)
{
	auto op = node->getOp();
	auto seq = node->getSequence();
	if (op == EOpNull) { // TODO: throw
		return false;
	}
	switch (op)
	{
	case EOpSequence:
		{
			if (node == initSeq)
				return false;
			//out << ";" << endl; 
			//ITERATE_BEGIN(TIntermSequence, seq, child)
			//	child->traverse(this);
			//	out << ";" << endl;
			//ITERATE_END
			return true;
		}
	case EOpLinkerObjects: return false;
	case EOpComma: return true; // TODO: wtf it is?
	case EOpFunction:
		{
			TString name = node->getName();
			auto originName = name.substr(0, name.find_first_of('('));
			out << getType(node->getType()) << " " << originName << "(";
			node->getSequence().at(0)->traverse(this);
			out << ") {" << endl;
			pushDepth();
			if (initSeq != nullptr && originName == "main")
				emitStatements(initSeq);
			if (node->getSequence().size() > 1)
				emitStatements(node->getSequence().at(1));
			popDepth();
			out << "}" << endl;
		}
		return false;
	case EOpFunctionCall:
		{
			TString name = node->getName();
			auto originName = name.substr(0, name.find_first_of('('));
			emitInvoking(originName.c_str(), node->getSequence());
		}
		return false;
	case EOpParameters:
		{
			int i = 0;
			ITERATE_BEGIN(TIntermSequence, seq, child)
				if (i++ > 0)
					out << ", ";
				emitSymbol(static_cast<TIntermSymbol*>(child), true, false);
			ITERATE_END
		}
		return false;
	case EOpConstructFloat:
	case EOpConstructDouble:
	case EOpConstructVec2:
	case EOpConstructVec3:
	case EOpConstructVec4:
	case EOpConstructDVec2:
	case EOpConstructDVec3:
	case EOpConstructDVec4:
	case EOpConstructBool:
	case EOpConstructBVec2:
	case EOpConstructBVec3:
	case EOpConstructBVec4:
	case EOpConstructInt:
	case EOpConstructIVec2:
	case EOpConstructIVec3:
	case EOpConstructIVec4:
	case EOpConstructUint:
	case EOpConstructUVec2:
	case EOpConstructUVec3:
	case EOpConstructUVec4:
	case EOpConstructInt64:
	case EOpConstructI64Vec2:
	case EOpConstructI64Vec3:
	case EOpConstructI64Vec4:
	case EOpConstructUint64:
	case EOpConstructU64Vec2:
	case EOpConstructU64Vec3:
	case EOpConstructU64Vec4:
#ifdef AMD_EXTENSIONS
	case EOpConstructInt16:
	case EOpConstructI16Vec2:
	case EOpConstructI16Vec3:
	case EOpConstructI16Vec4:
	case EOpConstructUint16:
	case EOpConstructU16Vec2:
	case EOpConstructU16Vec3:
	case EOpConstructU16Vec4:
#endif
	case EOpConstructMat2x2:
	case EOpConstructMat2x3:
	case EOpConstructMat2x4:
	case EOpConstructMat3x2:
	case EOpConstructMat3x3:
	case EOpConstructMat3x4:
	case EOpConstructMat4x2:
	case EOpConstructMat4x3:
	case EOpConstructMat4x4:
	case EOpConstructDMat2x2:
	case EOpConstructDMat2x3:
	case EOpConstructDMat2x4:
	case EOpConstructDMat3x2:
	case EOpConstructDMat3x3:
	case EOpConstructDMat3x4:
	case EOpConstructDMat4x2:
	case EOpConstructDMat4x3:
	case EOpConstructDMat4x4:
	case EOpConstructIMat2x2:
	case EOpConstructIMat2x3:
	case EOpConstructIMat2x4:
	case EOpConstructIMat3x2:
	case EOpConstructIMat3x3:
	case EOpConstructIMat3x4:
	case EOpConstructIMat4x2:
	case EOpConstructIMat4x3:
	case EOpConstructIMat4x4:
	case EOpConstructUMat2x2:
	case EOpConstructUMat2x3:
	case EOpConstructUMat2x4:
	case EOpConstructUMat3x2:
	case EOpConstructUMat3x3:
	case EOpConstructUMat3x4:
	case EOpConstructUMat4x2:
	case EOpConstructUMat4x3:
	case EOpConstructUMat4x4:
	case EOpConstructBMat2x2:
	case EOpConstructBMat2x3:
	case EOpConstructBMat2x4:
	case EOpConstructBMat3x2:
	case EOpConstructBMat3x3:
	case EOpConstructBMat3x4:
	case EOpConstructBMat4x2:
	case EOpConstructBMat4x3:
	case EOpConstructBMat4x4:
#ifdef AMD_EXTENSIONS
	case EOpConstructFloat16:
	case EOpConstructF16Vec2:
	case EOpConstructF16Vec3:
	case EOpConstructF16Vec4:
	case EOpConstructF16Mat2x2:
	case EOpConstructF16Mat2x3:
	case EOpConstructF16Mat2x4:
	case EOpConstructF16Mat3x2:
	case EOpConstructF16Mat3x3:
	case EOpConstructF16Mat3x4:
	case EOpConstructF16Mat4x2:
	case EOpConstructF16Mat4x3:
	case EOpConstructF16Mat4x4:
#endif
	case EOpConstructStruct:
	case EOpConstructTextureSampler:
		return emitConstruction(op, const_cast<TType&>(node->getType()), seq);
	/*case EOpLessThan: return emitEquation("<", seq);
	case EOpGreaterThan: return emitEquation(">", seq);
	case EOpLessThanEqual: return emitEquation("<=", seq);
	case EOpGreaterThanEqual: return emitEquation(">=", seq);
	case EOpVectorEqual: return emitEquation("==", seq);
	case EOpVectorNotEqual: return emitEquation("!=", seq);*/
	case EOpLessThan: return emitInvoking("lessThan", seq);
	case EOpGreaterThan: return emitInvoking("greaterThan", seq);
	case EOpLessThanEqual: return emitInvoking("lessThanEqual", seq);
	case EOpGreaterThanEqual: return emitInvoking("greaterThanEqual", seq);
	case EOpVectorEqual: return emitInvoking("equal", seq);
	case EOpVectorNotEqual: return emitInvoking("notEqual", seq);

	case EOpMod: return emitInvoking("mod", seq);
	case EOpModf: return emitInvoking("modf", seq);
	case EOpPow: return emitInvoking("pow", seq);

	case EOpAtan: return emitInvoking("atan", seq);

	case EOpMin: return emitInvoking("min", seq);
	case EOpMax: return emitInvoking("max", seq);
	case EOpClamp: return emitInvoking("clamp", seq);
	case EOpMix: return emitInvoking("mix", seq);
	case EOpStep: return emitInvoking("step", seq);
	case EOpSmoothStep: return emitInvoking("smoothstep", seq);

	case EOpDistance: return emitInvoking("distance", seq);
	case EOpDot: return emitInvoking("dot", seq);
	case EOpCross: return emitInvoking("cross", seq);
	case EOpFaceForward: return emitInvoking("faceforward", seq);
	case EOpReflect: return emitInvoking("reflect", seq);
	case EOpRefract: return emitInvoking("refract", seq);
	case EOpMul: return emitInvoking("matrixCompMult", seq);
	case EOpOuterProduct: return emitInvoking("outerProduct", seq);

	case EOpFtransform: return emitInvoking("ftransform", seq);

	case EOpEmitVertex: return emitInvoking("EmitVertex", seq);
	case EOpEndPrimitive: return emitInvoking("EndPrimitive", seq);

	case EOpBarrier: return emitInvoking("barrier", seq);
	case EOpMemoryBarrier: return emitInvoking("memoryBarrier", seq);
	case EOpMemoryBarrierAtomicCounter: return emitInvoking("memoryBarrierAtomicCounter", seq);
	case EOpMemoryBarrierBuffer: return emitInvoking("memoryBarrierBuffer", seq);
	case EOpMemoryBarrierImage: return emitInvoking("memoryBarrierImage", seq);
	case EOpMemoryBarrierShared: return emitInvoking("memoryBarrierShared", seq);
	case EOpGroupMemoryBarrier: return emitInvoking("groupMemoryBarrier", seq);

	case EOpReadInvocation: return emitInvoking("readInvocationARB", seq);

#ifdef AMD_EXTENSIONS
	case EOpSwizzleInvocations: return emitInvoking("swizzleInvocationsAMD", seq);
	case EOpSwizzleInvocationsMasked: return emitInvoking("swizzleInvocationsMaskedAMD", seq);
	case EOpWriteInvocation: return emitInvoking("writeInvocationAMD", seq);

	case EOpMin3: return emitInvoking("min3", seq);
	case EOpMax3: return emitInvoking("max3", seq);
	case EOpMid3: return emitInvoking("mid3", seq);

	case EOpTime: return emitInvoking("timeAMD", seq);
#endif

	case EOpAtomicAdd: return emitInvoking("atomicAdd", seq);
	case EOpAtomicMin: return emitInvoking("atomicMin", seq);
	case EOpAtomicMax: return emitInvoking("atomicMax", seq);
	case EOpAtomicAnd: return emitInvoking("atomicAnd", seq);
	case EOpAtomicOr: return emitInvoking("atomicOr", seq);
	case EOpAtomicXor: return emitInvoking("atomicXor", seq);
	case EOpAtomicExchange: return emitInvoking("atomicExchange", seq);
	case EOpAtomicCompSwap: return emitInvoking("atomicCompSwap", seq);

	case EOpAtomicCounterAdd: return emitInvoking("atomicCounterAdd", seq);
	case EOpAtomicCounterSubtract: return emitInvoking("atomicCounterSubtract", seq);
	case EOpAtomicCounterMin: return emitInvoking("atomicCounterMin", seq);
	case EOpAtomicCounterMax: return emitInvoking("atomicCounterMax", seq);
	case EOpAtomicCounterAnd: return emitInvoking("atomicCounterAnd", seq);
	case EOpAtomicCounterOr: return emitInvoking("atomicCounterOr", seq);
	case EOpAtomicCounterXor: return emitInvoking("atomicCounterXor", seq);
	case EOpAtomicCounterExchange: return emitInvoking("atomicCounterExchange", seq);
	case EOpAtomicCounterCompSwap: return emitInvoking("atomicCounterCompSwap", seq);

	case EOpImageQuerySize: return emitInvoking("imageSize", seq); // not sure
	case EOpImageQuerySamples: return emitInvoking("imageSamples", seq); // not sure
	case EOpImageLoad: return emitInvoking("imageLoad", seq);
	case EOpImageStore: return emitInvoking("imageStore", seq);
	case EOpImageAtomicAdd: return emitInvoking("imageAtomicAdd", seq);
	case EOpImageAtomicMin: return emitInvoking("imageAtomicMin", seq);
	case EOpImageAtomicMax: return emitInvoking("imageAtomicMax", seq);
	case EOpImageAtomicAnd: return emitInvoking("imageAtomicAnd", seq);
	case EOpImageAtomicOr: return emitInvoking("imageAtomicOr", seq);
	case EOpImageAtomicXor: return emitInvoking("imageAtomicXor", seq);
	case EOpImageAtomicExchange: return emitInvoking("imageAtomicExchange", seq);
	case EOpImageAtomicCompSwap: return emitInvoking("imageAtomicCompSwap", seq);
#ifdef AMD_EXTENSIONS
	case EOpImageLoadLod: return emitInvoking("imageLoadLodAMD", seq);
	case EOpImageStoreLod: return emitInvoking("imageStoreLodAMD", seq);
#endif

	case EOpTextureQuerySize: return emitInvoking("textureSize", seq); // not sure
	case EOpTextureQueryLod: return emitInvoking("textureQueryLod", seq);
	case EOpTextureQueryLevels: return emitInvoking("textureQueryLevels", seq);
	case EOpTextureQuerySamples: return emitInvoking("textureSamples", seq); // not sure
	case EOpTexture:
	case EOpTextureProj:
	case EOpTextureLod:
	case EOpTextureOffset:
	case EOpTextureFetch:
	case EOpTextureFetchOffset:
	case EOpTextureProjOffset:
	case EOpTextureLodOffset:
	case EOpTextureProjLod:
	case EOpTextureProjLodOffset:
	case EOpTextureGrad:
	case EOpTextureGradOffset:
	case EOpTextureProjGrad:
	case EOpTextureProjGradOffset:
	case EOpTextureGather:
	case EOpTextureGatherOffset:
	case EOpTextureGatherOffsets:
	case EOpTextureClamp:
	case EOpTextureOffsetClamp:
	case EOpTextureGradClamp:
	case EOpTextureGradOffsetClamp: return emitInvoking(getOriginTextureFunction(op, seq), seq);
#ifdef AMD_EXTENSIONS
	case EOpTextureGatherLod: return emitInvoking("textureGatherLod", seq);
	case EOpTextureGatherLodOffset: return emitInvoking("textureGatherLodOffset", seq);
	case EOpTextureGatherLodOffsets: return emitInvoking("textureGatherLodOffsets", seq);
#endif

	case EOpSparseTexture: return emitInvoking("sparseTextureARB", seq);
	case EOpSparseTextureOffset: return emitInvoking("sparseTextureOffsetARB", seq);
	case EOpSparseTextureLod: return emitInvoking("sparseTextureLodARB", seq);
	case EOpSparseTextureLodOffset: return emitInvoking("sparseTextureLodOffsetARB", seq);
	case EOpSparseTextureFetch: return emitInvoking("sparseTexelFetchARB", seq);
	case EOpSparseTextureFetchOffset: return emitInvoking("sparseTexelFetchOffsetARB", seq);
	case EOpSparseTextureGrad: return emitInvoking("sparseTextureGradARB", seq);
	case EOpSparseTextureGradOffset: return emitInvoking("sparseTextureGradOffsetARB", seq);
	case EOpSparseTextureGather: return emitInvoking("sparseTextureGatherARB", seq);
	case EOpSparseTextureGatherOffset: return emitInvoking("sparseTextureGatherOffsetARB", seq);
	case EOpSparseTextureGatherOffsets: return emitInvoking("sparseTextureGatherOffsetsARB", seq);
	case EOpSparseImageLoad: return emitInvoking("sparseImageLoadARB", seq);
	case EOpSparseTextureClamp: return emitInvoking("sparseTextureClampARB", seq);
	case EOpSparseTextureOffsetClamp: return emitInvoking("sparseTextureOffsetClampARB", seq);
	case EOpSparseTextureGradClamp: return emitInvoking("sparseTextureGradClampARB", seq);
	case EOpSparseTextureGradOffsetClamp: return emitInvoking("sparseTextureGradOffsetClampARB", seq);
#ifdef AMD_EXTENSIONS
	case EOpSparseTextureGatherLod: return emitInvoking("sparseTextureGatherLodAMD", seq);
	case EOpSparseTextureGatherLodOffset: return emitInvoking("sparseTextureGatherLodOffsetAMD", seq);
	case EOpSparseTextureGatherLodOffsets: return emitInvoking("sparseTextureGatherLodOffsetsAMD", seq);
	// not sure. can't find out which extension provides SparseImageLoadLod.
	case EOpSparseImageLoadLod: return emitInvoking("sparseImageLoadLodAMD", seq);
#endif

	case EOpAddCarry: return emitInvoking("uaddCarry", seq);
	case EOpSubBorrow: return emitInvoking("usubBorrow", seq);
	case EOpUMulExtended: return emitInvoking("umulExtended", seq);
	case EOpIMulExtended: return emitInvoking("imulExtended", seq);
	case EOpBitfieldExtract: return emitInvoking("bitfieldExtract", seq);
	case EOpBitfieldInsert: return emitInvoking("bitfieldInsert", seq);

	case EOpFma: return emitInvoking("fma", seq);
	case EOpFrexp: return emitInvoking("frexp", seq);
	case EOpLdexp: return emitInvoking("ldexp", seq);

	case EOpInterpolateAtSample: return emitInvoking("interpolateAtSample", seq);
	case EOpInterpolateAtOffset: return emitInvoking("interpolateAtOffset", seq);
#ifdef AMD_EXTENSIONS
	case EOpInterpolateAtVertex: return emitInvoking("interpolateAtVertexAMD", seq);
#endif

	case EOpSinCos: return emitInvoking("sinCos", seq); // not sure
	case EOpGenMul: return emitInvoking("mul", seq); // hlsl op

	case EOpAllMemoryBarrierWithGroupSync: return emitInvoking("AllMemoryBarrierWithGroupSync", seq); // hlsl op
	case EOpDeviceMemoryBarrier: return emitInvoking("DeviceMemoryBarrier", seq); // hlsl op
	case EOpDeviceMemoryBarrierWithGroupSync: return emitInvoking("DeviceMemoryBarrierWithGroupSync", seq); // hlsl op
	case EOpWorkgroupMemoryBarrier: return emitInvoking("WorkgroupMemoryBarrier", seq); // hlsl op
	case EOpWorkgroupMemoryBarrierWithGroupSync: return emitInvoking("WorkgroupMemoryBarrierWithGroupSync", seq); // hlsl op

	case EOpSubpassLoad: return emitInvoking("subpassLoad", seq);
	case EOpSubpassLoadMS: return emitInvoking("subpassLoadMS", seq); // not sure
	}
	return true;
}

bool EmitterTraverser::visitSelection(TVisit, TIntermSelection* node)
{
	out << "if (";
	node->getCondition()->traverse(this);
	out << ") {" << endl;
	if (node->getTrueBlock())
	{
		pushDepth();
		emitStatements(node->getTrueBlock());
		popDepth();
	}
	printTabs(); 
	out << "}";
	if (node->getFalseBlock())
	{
		out << " else ";
		if (node->getFalseBlock()->getAsSelectionNode())
		{
			//pushDepth();
			emitStatements(node->getFalseBlock());
			//popDepth();
		}
		else
		{
			out << "{" << endl;
			pushDepth();
			emitStatements(node->getFalseBlock());
			popDepth();
			printTabs();
			out << "}" << endl;
		}
	}
	else
	{
		out << endl;
	}
	return false;
}

void EmitterTraverser::visitConstantUnion(TIntermConstantUnion* node)
{
	int index = 0;
	emitConst(index, const_cast<TConstUnionArray&>(node->getConstArray()), const_cast<TType&>(node->getType()));
}

void EmitterTraverser::visitSymbol(TIntermSymbol* node)
{
	bool declare = node->getQualifier().storage == EvqTemporary && !isVarDeclared(const_cast<TString&>(node->getName()));
	emitSymbol(node, declare, false);
	if (declare)
		declareVar(const_cast<TString&>(node->getName()));
}

bool EmitterTraverser::visitLoop(TVisit, TIntermLoop* node)
{
	// No for-loop on here.
	if (node->testFirst()) // while {}
	{
		out << "while (";
		if (node->getTest())
			node->getTest()->traverse(this);
		else
			out << "true";
		out << ") {" << endl;
		pushDepth();
		if (node->getBody())
			emitStatements(node->getBody());
		popDepth();
		printTabs();
		out << "}";
	}
	else // do {} while
	{
		out << "do {" << endl;
		pushDepth();
		if (node->getBody())
			emitStatements(node->getBody());
		popDepth();
		printTabs();
		out << "} while (";
		if (node->getTest())
			node->getTest()->traverse(this);
		else
			out << "true";
		out << ");" << endl;
	}
	return false;
}

bool EmitterTraverser::visitBranch(TVisit, TIntermBranch* node)
{
	switch (node->getFlowOp()) {
	case EOpKill: out << "discard;" << endl; break;
	case EOpBreak: out << "break;" << endl; break;
	case EOpContinue: out << "continue;" << endl; break;
	case EOpReturn: out << "return"; break;
	case EOpCase: printTabs(); out << "case"; break;
	case EOpDefault: printTabs(); out << "default:" << endl; break;
	default: break; // TODO: throw
	}
	if (node->getExpression()) {
		out << " ";
		node->getExpression()->traverse(this);
	}
	switch (node->getFlowOp()) {
	case EOpReturn: out << ";" << endl; break;
	case EOpCase: out << ":" << endl; break;
	}
	return false;
}

bool EmitterTraverser::visitSwitch(TVisit, TIntermSwitch* node)
{
	out << "switch(";
	node->getCondition()->traverse(this);
	out << ") {" << endl;
	pushDepth();
	if (node->getBody())
	{
		auto seq = node->getBody()->getSequence();
		ITERATE_BEGIN(TIntermSequence, seq, child)
			if (child->getAsAggregate())
				emitStatements(child->getAsAggregate());
			else
				child->traverse(this);
		ITERATE_END
	}
	popDepth();
	printTabs();
	out << "}";
	return false;
}
