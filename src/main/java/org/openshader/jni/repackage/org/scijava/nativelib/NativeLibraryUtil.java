/*
 * #%L
 * Native library loader for extracting and loading native libraries from Java.
 * %%
 * Copyright (C) 2010 - 2015 Board of Regents of the University of
 * Wisconsin-Madison and Glencoe Software, Inc.
 * %%
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * #L%
 */

package org.openshader.jni.repackage.org.scijava.nativelib;

import java.io.File;
import java.io.IOException;

/**
 * This class is a utility for loading native libraries.
 * <p>
 * Native libraries should be packaged into a single jar file, with the
 * following directory and file structure:
 *
 * <pre>
 * META-INF
 *   lib
 *     linux_32
 *       libxxx[-vvv].so
 *     linux_64
 *       libxxx[-vvv].so
 *     linux_arm
 *       libxxx[-vvv].so
 *     linux_arm64
 *       libxxx[-vvv].so
 *     osx_32
 *       libxxx[-vvv].dylib
 *     osx_64
 *       libxxx[-vvv].dylib
 *     windows_32
 *       xxx[-vvv].dll
 *     windows_64
 *       xxx[-vvv].dll
 * </pre>
 * <p>
 * Here "xxx" is the name of the native library and "-vvv" is an optional
 * version number.
 * <p>
 * Current approach is to unpack the native library into a temporary file and
 * load from there.
 *
 * @author Aivar Grislis
 */
public class NativeLibraryUtil {

	public static enum Architecture {
		UNKNOWN, LINUX_32, LINUX_64, LINUX_ARM, LINUX_ARM64, WINDOWS_32, WINDOWS_64, OSX_32,
			OSX_64, OSX_PPC
	}

	private static enum Processor {
		UNKNOWN, INTEL_32, INTEL_64, PPC, ARM, AARCH_64
	}

	private static Architecture architecture = Architecture.UNKNOWN;
	private static final String DELIM = "/";
	private static final String JAVA_TMPDIR = "java.io.tmpdir";
	//private static final Logger LOGGER = LoggerFactory.getLogger("org.scijava.nativelib.NativeLibraryUtil");

	/**
	 * Determines the underlying hardware platform and architecture.
	 *
	 * @return enumerated architecture value
	 */
	public static Architecture getArchitecture() {
		if (Architecture.UNKNOWN == architecture) {
			final Processor processor = getProcessor();
			if (Processor.UNKNOWN != processor) {
				final String name = System.getProperty("os.name").toLowerCase();
				if (name.contains("nix") || name.contains("nux")) {
					if (Processor.INTEL_32 == processor) {
						architecture = Architecture.LINUX_32;
					}
					else if (Processor.INTEL_64 == processor) {
						architecture = Architecture.LINUX_64;
					}
					else if (Processor.ARM == processor) {
						architecture = Architecture.LINUX_ARM;
					}
					else if (Processor.AARCH_64 == processor) {
						architecture = Architecture.LINUX_ARM64;
					}
				}
				else if (name.contains("win")) {
					if (Processor.INTEL_32 == processor) {
						architecture = Architecture.WINDOWS_32;
					}
					else if (Processor.INTEL_64 == processor) {
						architecture = Architecture.WINDOWS_64;
					}
				}
				else if (name.contains("mac")) {
					if (Processor.INTEL_32 == processor) {
						architecture = Architecture.OSX_32;
					}
					else if (Processor.INTEL_64 == processor) {
						architecture = Architecture.OSX_64;
					}
					else if (Processor.PPC == processor) {
						architecture = Architecture.OSX_PPC;
					}
				}
			}
		}
		//LOGGER.debug("architecture is " + architecture + " os.name is " + System.getProperty("os.name").toLowerCase());
		return architecture;
	}

	/**
	 * Determines what processor is in use.
	 *
	 * @return The processor in use.
	 */
	private static Processor getProcessor() {
		Processor processor = Processor.UNKNOWN;
		int bits;

		// Note that this is actually the architecture of the installed JVM.
		final String arch = System.getProperty("os.arch").toLowerCase();

		if (arch.contains("arm")) {
			processor = Processor.ARM;
		}
		else if (arch.contains("aarch64")) {
			processor = Processor.AARCH_64;
		}
		else if (arch.contains("ppc")) {
			processor = Processor.PPC;
		}
		else if (arch.contains("86") || arch.contains("amd")) {
			bits = 32;
			if (arch.contains("64")) {
				bits = 64;
			}
			processor = (32 == bits) ? Processor.INTEL_32 : Processor.INTEL_64;
		}
		//LOGGER.debug("processor is " + processor + " os.arch is " + System.getProperty("os.arch").toLowerCase());
		return processor;
	}

	/**
	 * Returns the path to the native library.
	 *
	 * @return path
	 */
	public static String getPlatformLibraryPath() {
		String path = "META-INF" + DELIM + "lib" + DELIM;
		path += getArchitecture().name().toLowerCase() + DELIM;
		//LOGGER.debug("platform specific path is " + path);
		return path;
	}

	/**
	 * Returns the full file name (without path) of the native library.
	 *
	 * @param libName name of library
	 * @return file name
	 */
	public static String getPlatformLibraryName(final String libName) {
		String name = null;
		switch (getArchitecture()) {
			case LINUX_32:
			case LINUX_64:
			case LINUX_ARM:
			case LINUX_ARM64:
				name = libName + ".so";
				break;
			case WINDOWS_32:
			case WINDOWS_64:
				name = libName + ".dll";
				break;
			case OSX_32:
			case OSX_64:
				name = "lib" + libName + ".dylib";
				break;
		}
		//LOGGER.debug("native library name " + name);
		return name;
	}

	/**
	 * Returns the Maven-versioned file name of the native library. In order for
	 * this to work Maven needs to save its version number in the jar manifest.
	 * The version of the library-containing jar and the version encoded in the
	 * native library names should agree.
	 *
	 * <pre>
	 * {@code
	 * <build>
	 *   <plugins>
	 *     <plugin>
	 *       <artifactId>maven-jar-plugin</artifactId>
	 *         <inherited>true</inherited> *
	 *         <configuration>
	 *            <archive>
	 *              <manifest>
	 *                <packageName>com.example.package</packageName>
	 *                <addDefaultImplementationEntries>true</addDefaultImplementationEntries> *
	 *              </manifest>
	 *           </archive>
	 *         </configuration>
	 *     </plugin>
	 *   </plugins>
	 * </build>
	 *
	 * * = necessary to save version information in manifest
	 * }
	 * </pre>
	 *
	 * @param libraryJarClass any class within the library-containing jar
	 * @param libName name of library
	 * @return The Maven-versioned file name of the native library.
	 */
	public static String getVersionedLibraryName(final Class<?> libraryJarClass,
		String libName)
	{
		final String version =
			libraryJarClass.getPackage().getImplementationVersion();
		if (null != version && version.length() > 0) {
			libName += "-" + version;
		}
		return libName;
	}

	/**
	 * Loads the native library. Picks up the version number to specify from the
	 * library-containing jar.
	 *
	 * @param libraryJarClass any class within the library-containing jar
	 * @param libName name of library
	 * @return whether or not successful
	 */
	public static boolean loadVersionedNativeLibrary(
		final Class<?> libraryJarClass, String libName)
	{
		// append version information to native library name
		libName = getVersionedLibraryName(libraryJarClass, libName);

		return loadNativeLibrary(libraryJarClass, libName);
	}

	/**
	 * Loads the native library.
	 *
	 * @param libraryJarClass any class within the library-containing jar
	 * @param libName name of library
	 * @return whether or not successful
	 */
	public static boolean loadNativeLibrary(final Class<?> libraryJarClass,
		final String libName)
	{
		boolean success = false;

		if (Architecture.UNKNOWN == getArchitecture()) {
			//LOGGER.warn("No native library available for this platform.");
		}
		else {
			try {
				// will extract library to temporary directory
				final String tmpDirectory = System.getProperty(JAVA_TMPDIR);
				final JniExtractor jniExtractor =
					new DefaultJniExtractor(libraryJarClass, tmpDirectory);

				// do extraction
				final File extractedFile =
					jniExtractor.extractJni(getPlatformLibraryPath(), libName);

				// load extracted library from temporary directory
				System.load(extractedFile.getPath());

				success = true;
			}
			catch (final IOException e) {
				//LOGGER.debug("IOException creating DefaultJniExtractor", e);
			}
			catch (final SecurityException e) {
				//LOGGER.debug("Can't load dynamic library", e);
			}
			catch (final UnsatisfiedLinkError e) {
				//LOGGER.debug("Problem with library", e);
			}
		}
		return success;
	}
}
