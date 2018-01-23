package org.openshader.jni.compiler;

import org.openshader.jni.OpenShaderJni;

public final class ShaderCompilerManager {
	
	private final static ThreadLocal<Boolean> registered = new ThreadLocal() {
		@Override
		protected Boolean initialValue() {
			return false;
		}
	};
	
	private final static ThreadLocal<Integer> stackDepth = new ThreadLocal<Integer>();
	
	private ShaderCompilerManager() {}

	public static ShaderCompiler createShaderCompiler(int defaultVersion) {
		return new ShaderCompiler(createShaderCompiler_do(defaultVersion));
	}
	
	public static void registerWorkThread() {
		if (OpenShaderJni.getMainThread() == Thread.currentThread())
			throw new RuntimeException("Can't register main thread as work thread!");
		if (!registered.get()) {
			registerWorkThread_do();
			registered.set(true);
			stackDepth.set(0);
		}
	}
	
	public static void unregisterWorkThread() {
		if (OpenShaderJni.getMainThread() == Thread.currentThread())
			throw new RuntimeException("Can't unregister main thread!");
		if (registered.get()) {
			unregisterWorkThread_do();
			registered.set(false);
		}
		unregisterWorkThread_do();
	}
	
	public static void pushMemoryPool() {
		if (!registered.get())
			throw new RuntimeException("Thread hasn't been registered as worker yet!");
		stackDepth.set(stackDepth.get() + 1);
		pushMemoryPool_do();
	}
	
	public static void popMemoryPool() {
		if (!registered.get())
			throw new RuntimeException("Thread hasn't been registered as worker yet!");
		int newDepth = stackDepth.get() - 1;
		if (newDepth < 0)
			throw new RuntimeException("Glslang memory pool stack error!");
		stackDepth.set(newDepth);
		popMemoryPool_do();
	}
	
	protected static native long createShaderCompiler_do(int defaultVersion);
	
	protected static native void registerWorkThread_do();
	
	protected static native void unregisterWorkThread_do();
	
	protected static native void pushMemoryPool_do();
	
	protected static native void popMemoryPool_do();
}
