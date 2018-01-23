package org.openshader.jni;

import java.io.IOException;

import org.openshader.jni.compiler.CompileJob;
import org.openshader.jni.compiler.IHeaderIncluder;
import org.openshader.jni.compiler.ShaderCompiler;
import org.openshader.jni.compiler.ShaderCompilerManager;
import org.openshader.jni.compiler.ShaderCompiler.ShaderCapability;
import org.openshader.jni.compiler.ShaderCompiler.ShaderType;
import org.openshader.jni.compiler.ShaderCompiler.UniformBlock;
import org.openshader.jni.repackage.org.scijava.nativelib.NativeLibraryUtil;
import org.openshader.jni.repackage.org.scijava.nativelib.NativeLoader;


public final class OpenShaderJni {

	private static boolean loaded = false;
	private static boolean available = false;
	private static Thread mainThread = null;
	
	private OpenShaderJni() {}
	
	public static void init() throws IOException, SecurityException, UnsatisfiedLinkError, NativeLibraryException {
		if (!loaded) {
			loaded = true;
			NativeLibraryUtil.loadNativeLibrary(OpenShaderJni.class, "OpenShaderNative");
			initJni();
			mainThread = Thread.currentThread();
			available = true;
		}
	}
	
	public static boolean isAvailable() {
		return available;
	}
	
	public static Thread getMainThread() {
		return mainThread;
	}
	
	private static native void initJni();
}
