package org.openshader.jni.compiler;

public interface IHeaderIncluder {

	public abstract String includeLocal(String headerName, String includerName);
	
	public abstract String includeSystem(String headerName, String includerName);
}
