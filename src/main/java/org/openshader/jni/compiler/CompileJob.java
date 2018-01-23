package org.openshader.jni.compiler;

import java.util.Collection;

import org.openshader.jni.compiler.ShaderCompiler.UniformBlock;

public class CompileJob {

	private final long p_address;
	private boolean deleted = false;
	
	protected CompileJob(long address) {
		this.p_address = address;
	}

	public void delete() {
		if (!deleted) {
			deleted = true;
			delete_do(p_address);
		}
	}
	
	public void setSource(String source) {
		setSource(source, null);
	}
	
	public void setSource(String source, String filename) {
		setSource_do(p_address, source, filename);
	}
	
	public void addPreprocess(Collection<String> preprocess) {
		addPreprocess_do(p_address, preprocess.toArray(new String[preprocess.size()]));
	}
	
	public void addPreprocess(String ... preprocess) {
		addPreprocess_do(p_address, preprocess);
	}
	
	public void addUniformBlock(String name, UniformBlock ub) {
		addUniformBlock_do(this.p_address, ub.getAddress(), name);
	}
	
	public String compile() {
		return compile_do(p_address);
	}
	
	protected native static void setSource_do(long address, String source, String filename);
	
	protected native static void addUniformBlock_do(long jobAddress, long ubAddress, String ubName);
	
	protected native static void addPreprocess_do(long address, String[] preprocess);
	
	protected native static void delete_do(long address);
	
	protected native static String compile_do(long address);
	
	protected void assertNotDeleted() {
		if (deleted)
			throw new RuntimeException("The compile job had already been deleted!");
	}
	
	@Override
	protected void finalize() throws Throwable {
		delete();
		super.finalize();
	}
}
