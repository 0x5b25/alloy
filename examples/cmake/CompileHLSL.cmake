

macro(compile_shader TARGET_NAME)

    #dxc -E PSMain -Fh ImGuiAlloyBackend_ps.h -T ps_6_0 -Zi -Qembed_debug ImGuiAlloyBackend.hlsl

    set(SHADER_SOURCE_FILES ${ARGN}) # the rest of arguments to this function will be assigned as shader source files
  
    # Validate that source files have been passed
    list(LENGTH SHADER_SOURCE_FILES FILE_COUNT)
    if(FILE_COUNT EQUAL 0)
        message(FATAL_ERROR "Cannot create a shaders target without any source files")
    endif()

    set(HLSL_COMPILER ${DXC_EXECUTABLE_DIR}/Debug/dxc)
    set(HLSL_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

    #make the generated output folder
    file(MAKE_DIRECTORY ${HLSL_OUTPUT_DIR})

    #set(SHADER_COMMANDS)
    set(SHADER_PRODUCTS)

    macro(add_dxc_compile_command SHADER_SRC SHADER_NAME ENTRY_NAME TYPE)
        set(SHADER_RESULT "${HLSL_OUTPUT_DIR}/${SHADER_NAME}_${TYPE}.h")

        add_custom_command(
            OUTPUT ${SHADER_RESULT}
            COMMAND ${HLSL_COMPILER} -E ${ENTRY_NAME} -T ${TYPE}_6_0 -Fh ${SHADER_RESULT} -Zi -Qembed_debug ${SHADER_SRC}
            DEPENDS ${SHADER_SRC}
            COMMENT "Compiling ${TYPE} shader from ${SHADER_SRC}"
            VERBATIM
        )

        ## Build command
        #list(APPEND SHADER_COMMANDS COMMAND)
        #list(APPEND SHADER_COMMANDS ${HLSL_COMPILER})
        ##set entry 
        #list(APPEND SHADER_COMMANDS "-E ${ENTRY_NAME}")
        ##Shader type
        #list(APPEND SHADER_COMMANDS "-T ${TYPE}_6_0")
        ##Embed debug info
        #list(APPEND SHADER_COMMANDS "-Zi -Qembed_debug")
        ##output header name
        #list(APPEND SHADER_COMMANDS "-Fh ${SHADER_RESULT}")
        ##input
        #list(APPEND SHADER_COMMANDS "${SHADER_SRC}")

        # Add product
        list(APPEND SHADER_PRODUCTS ${SHADER_RESULT})
    endmacro()

    foreach(SHADER_SOURCE IN LISTS SHADER_SOURCE_FILES)
        #expand the shader file path
        cmake_path(ABSOLUTE_PATH SHADER_SOURCE NORMALIZE)

        #get shader file name
        cmake_path(GET SHADER_SOURCE STEM SHADER_NAME)
        # Build command
        # Generate pixel shader
        add_dxc_compile_command(${SHADER_SOURCE} ${SHADER_NAME} PSMain ps)
        # Generate vertex shader
        add_dxc_compile_command(${SHADER_SOURCE} ${SHADER_NAME} VSMain vs)

    endforeach()

    #add_custom_target(${TARGET_NAME}
    #    ${SHADER_COMMANDS}
    #    COMMENT "Compiling Shaders [${TARGET_NAME}]"
    #    SOURCES ${SHADER_SOURCE_FILES}
    #    BYPRODUCTS ${SHADER_PRODUCTS}
    #)


    add_library(${TARGET_NAME} INTERFACE)
    target_sources(${TARGET_NAME} PUBLIC 
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR}
        FILES ${SHADER_PRODUCTS})


    #target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

endmacro()
