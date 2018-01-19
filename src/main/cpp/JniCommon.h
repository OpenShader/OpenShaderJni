#ifndef _Included_JniCommon
#define _Included_JniCommon

enum ShaderCapability
{
	GLSL130,
	GLSL150,
	GLSL400,
	GEOMETRY_SHADER,
	TESSELLATION_SHADER,
	COMPUTE_SHADER,
	UNIFORM_BLOCK,
	TEXTURE_ARRAY,
	SUBROUTINE,
};

bool ShaderCapabilityHas(int capabilities, ShaderCapability capability);
void ShaderCapabilitySet(int &capabilities, ShaderCapability capability, bool value = true);

#ifdef IMPLEMENT_JNICOMMON

bool ShaderCapabilityHas(int capabilities, ShaderCapability capability)
{
	return ((1 << static_cast<int>(capability)) & capabilities) > 0;
}

void ShaderCapabilitySet(int &capabilities, ShaderCapability capability, bool value)
{
	if (value)
		capabilities |= 1 << static_cast<int>(capability);
	else
		capabilities &= ~(1 << static_cast<int>(capability));
}

#endif

#endif