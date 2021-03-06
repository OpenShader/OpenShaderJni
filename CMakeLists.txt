cmake_minimum_required(VERSION 2.8)
if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif()
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(GNUInstallDirs)

set (SKIP_GLSLANG_INSTALL ON)
set (ENABLE_GLSLANG_INSTALL OFF)
option(ENABLE_AMD_EXTENSIONS "Enables support of AMD-specific extensions" ON)
option(ENABLE_NV_EXTENSIONS "Enables support of Nvidia-specific extensions" ON)
option(ENABLE_HLSL "Enables HLSL input support" OFF)
set (ENABLE_OPT ON)

project(OpenShaderJni)

if(WIN32)
	include("VC-LTL/VC-LTL helper for cmake.cmake")
    #set_target_properties(glslang OGLCompiler OSDependent jni PROPERTIES VS_USER_PROPS "${PROJECT_SOURCE_DIR}/src/main/cpp/MSVC.props")
endif(WIN32)

if(WIN32)
    #set(CMAKE_DEBUG_POSTFIX "d")
    if(MSVC)
        include(glslang/ChooseMSVCCRT.cmake)
		add_definitions(-D_DISABLE_DEPRECATE_LTL_MESSAGE)
    endif(MSVC)
    add_definitions(-DGLSLANG_OSINCLUDE_WIN32)
elseif(UNIX)
    add_definitions(-DGLSLANG_OSINCLUDE_UNIX)
else(WIN32)
    message("unknown platform")
endif(WIN32)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-Wall -Wmaybe-uninitialized -Wuninitialized -Wunused -Wunused-local-typedefs
                        -Wunused-parameter -Wunused-value  -Wunused-variable -Wunused-but-set-parameter -Wunused-but-set-variable)
    add_compile_options(-Wno-reorder)  # disable this from -Wall, since it happens all over.
	add_compile_options(-fno-rtti)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wuninitialized -Wunused -Wunused-local-typedefs
                        -Wunused-parameter -Wunused-value  -Wunused-variable)
    add_compile_options(-Wno-reorder)  # disable this from -Wall, since it happens all over.
	add_compile_options(-fno-rtti)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	if(CMAKE_CXX_FLAGS MATCHES "/GR ") 
		string(REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}") 
	endif()
	if(CMAKE_CXX_FLAGS MATCHES "/EHsc") 
		string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}") 
	endif()
endif()

if(ENABLE_AMD_EXTENSIONS)
    add_definitions(-DAMD_EXTENSIONS)
endif(ENABLE_AMD_EXTENSIONS)

if(ENABLE_NV_EXTENSIONS)
    add_definitions(-DNV_EXTENSIONS)
endif(ENABLE_NV_EXTENSIONS)

if(ENABLE_HLSL)
    add_definitions(-DENABLE_HLSL)
endif(ENABLE_HLSL)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(glslang_set_link_args TARGET)
    # For MinGW compiles, statically link against the GCC and C++ runtimes.
    # This avoids the need to ship those runtimes as DLLs.
    if(WIN32 AND ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
        set_target_properties(${TARGET} PROPERTIES
                              LINK_FLAGS "-static -static-libgcc -static-libstdc++")
    endif()
endfunction(glslang_set_link_args)

if(NOT TARGET SPIRV-Tools-opt)
    set(ENABLE_OPT OFF)
endif()

if(ENABLE_OPT)
    add_definitions(-DENABLE_OPT)
endif()

add_subdirectory(glslang/glslang)
add_subdirectory(glslang/OGLCompilersDLL)
if(ENABLE_HLSL)
    add_subdirectory(glslang/hlsl)
endif(ENABLE_HLSL)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	string(REPLACE "-fno-exceptions" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}") 
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	string(REPLACE "-fno-exceptions" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}") 
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	add_compile_options(/EHsc)
endif()

add_subdirectory(src/main/cpp)
