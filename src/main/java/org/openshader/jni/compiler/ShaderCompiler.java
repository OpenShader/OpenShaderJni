package org.openshader.jni.compiler;

import java.util.ArrayList;
import java.util.List;

public class ShaderCompiler {

	private final long p_address;
	private boolean deleted = false;
	
	private final List<CompileJob> compileJobs = new ArrayList<>();
	private final List<UniformBlock> uniformBlocks = new ArrayList<>();
	
	protected ShaderCompiler(long pointerAddress) {
		this.p_address = pointerAddress;
	}
	
	public CompileJob createCompileJob(ShaderType type) {
		long job_address = createCompileJob_do(p_address, type.ordinal());
		CompileJob job = new CompileJob(job_address);
		compileJobs.add(job);
		return job;
	}
	
	public void delete() {
		if (!deleted) {
			deleted = true;
			for (CompileJob job : compileJobs) {
				job.delete();
			}
			compileJobs.clear();
			delete_do(p_address);
		}
	}
	
	public void setShaderCapability(ShaderCapability ... capabilities) {
		for (ShaderCapability capability : capabilities) {
			setShaderCapability_do(p_address, capability.ordinal());
		}
	}
	
	public void setIncluder(IHeaderIncluder includer) {
		setIncluder_do(p_address, includer);
	}
	
	public UniformBlock createUniformBlock() {
		UniformBlock block = new UniformBlock(createUniformBlock_do(p_address));
		uniformBlocks.add(block);
		return block;
	}
	
	protected native static void delete_do(long address);
	
	protected native static long createCompileJob_do(long address, int shaderType);
	
	protected native static long setShaderCapability_do(long address, int capability);
	
	protected native static long setIncluder_do(long address, IHeaderIncluder includer);
	
	protected native static long createUniformBlock_do(long address);
	
	protected native static void addMember_do(long clAddress, long ubAddress, String member);
	
	protected void assertNotDeleted() {
		if (deleted)
			throw new RuntimeException("The shader compiler had already been deleted!");
	}
	
	/**
	 * Should be same to EShLanguage.
	 */
	public static enum ShaderType {
		VERTEX,
		TESSCCONTROL,
		TESSEVALUATION,
		GEOMETRY,
		FRAGMENT,
		COMPUTE;
	}
	
	/**
	 * Should be same to the one in native code.
	 */
	public static enum ShaderCapability {
		GLSL130, // EXT_gpu_shader4 provides unsigned integer, bitwise operators and some new functions.
		GLSL150, // OpenGL3.2 provides in and out in shader.
		GLSL400, // ARB_gpu_shader5 provides double and some new functions.
		GEOMETRY_SHADER, // ARB_geometry_shader4
		TESSELLATION_SHADER, // ARB_tessellation_shader
		COMPUTE_SHADER, // ARB_compute_shader
		UNIFORM_BLOCK, // ARB_uniform_buffer_object
		TEXTURE_ARRAY, // EXT_texture_array 
		SUBROUTINE; // ARB_shader_subroutine
	}
	
	public class UniformBlock {
		private final long p_address;
		
		protected UniformBlock(long address) {
			this.p_address = address;
		}
		
		public void addMember(String type, String name) {
			addMember_do(ShaderCompiler.this.p_address, this.p_address, type + " " + name);
		}
		
		long getAddress() {
			return this.p_address;
		}
	}
}
