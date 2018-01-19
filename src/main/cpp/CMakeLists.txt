find_package(JNI)

file(GLOB SOURCES "./*.cpp" "./*.h")  

add_library(jni SHARED ${SOURCES})

target_include_directories(jni
        PUBLIC ${PROJECT_SOURCE_DIR}/glslang/glslang/Include
        PUBLIC ${PROJECT_SOURCE_DIR}/glslang/glslang/Public
		PUBLIC ${PROJECT_SOURCE_DIR}/glslang/glslang/MachineIndependent
		PUBLIC ${PROJECT_SOURCE_DIR}/glslang/OGLCompilersDLL
		PUBLIC ${JAVA_INCLUDE_PATH}
		PUBLIC ${JAVA_INCLUDE_PATH2}
    )

set_property(TARGET jni PROPERTY FOLDER product)
glslang_set_link_args(jni)

set(LIBRARIES
    glslang
	)

if(WIN32)
    set(LIBRARIES ${LIBRARIES} psapi)
	set_target_properties(jni PROPERTIES OUTPUT_NAME "OpenShaderNative")
	set_target_properties(jni PROPERTIES LINK_LIBRARIES "%(AdditionalDependencies)psapi") # dirty hack...
elseif(UNIX)
    if(NOT ANDROID)
        set(LIBRARIES ${LIBRARIES} pthread)
    endif()
	set_target_properties(jni PROPERTIES OUTPUT_NAME "libOpenShaderNative")
endif(WIN32)

target_link_libraries(jni ${LIBRARIES})

if(WIN32)
    source_group("Source" FILES ${SOURCES})
endif(WIN32)
