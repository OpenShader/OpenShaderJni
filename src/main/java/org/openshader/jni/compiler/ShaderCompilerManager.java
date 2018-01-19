package org.openshader.jni.compiler;

public class ShaderCompilerManager {

	public ShaderCompiler createShaderCompiler(int defaultVersion) {
		return new ShaderCompiler(createShaderCompiler_do(defaultVersion));
	}
	
	protected static native long createShaderCompiler_do(int defaultVersion);
}
