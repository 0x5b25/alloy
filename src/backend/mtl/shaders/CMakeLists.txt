
message("******** Building for ${CMAKE_SYSTEM_NAME} ********")

if(ALLOY_PLATFORM STREQUAL "iOSSim")
    set(XCRUN_SDK iphonesimulator)
elseif(ALLOY_PLATFORM STREQUAL "iOS")
    set(XCRUN_SDK iphoneos)
elseif(ALLOY_PLATFORM STREQUAL "MacOS")
    set(XCRUN_SDK macosx)
else()
	message(FATAL_ERROR "-- alloy: metal shader: unknown platform")
endif()

function(compile_metal_shader SHADER_FILE)
    #Get shader name
    cmake_path(GET SHADER_FILE STEM SHADER_NAME)
    #result file name
    set(SHADER_IR_NAME ${SHADER_NAME}.ir )
    set(SHADER_LIB_NAME ${SHADER_NAME}.metallib )
    set(SHADER_HEADER_NAME ${SHADER_NAME}.h )
    #set(SHADER_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_LIB_NAME})

    add_custom_command(OUTPUT ${SHADER_HEADER_NAME}
        COMMAND xcrun -sdk ${XCRUN_SDK} metal -o ${SHADER_IR_NAME} -c ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILE}
        COMMAND xcrun -sdk ${XCRUN_SDK} metal -o ${SHADER_LIB_NAME} ${SHADER_IR_NAME}
        COMMAND "${CMAKE_COMMAND}"
            -DFILE_IN=${SHADER_LIB_NAME}
            -DFILE_OUT=${SHADER_HEADER_NAME}
            -DVARIABLE_NAME=${SHADER_NAME}_contents
            -P "${CMAKE_CURRENT_SOURCE_DIR}/Bin2Hex.cmake"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        DEPENDS ${SHADER_FILE}
        COMMENT "Compile ${SHADER_FILE}"
        VERBATIM)

endfunction(compile_metal_shader)

set(METAL_SHADER_FILES
    "MSAAResolve.metal"
    "ExecuteIndirectArgGen.metal"
    )

foreach(SHADER_FILE ${METAL_SHADER_FILES})
    #Add custom commands for every shader
    compile_metal_shader(${SHADER_FILE})
    #Append dependency list
    cmake_path(GET SHADER_FILE STEM SHADER_NAME)
    set(SHADER_HEADER_NAME ${SHADER_NAME}.h )
    set(ALL_SHADER_LIBS ${ALL_SHADER_LIBS} ${SHADER_HEADER_NAME})
endforeach()

add_custom_target(metal_shaders DEPENDS ${ALL_SHADER_LIBS})

add_dependencies(Veldrid metal_shaders)

cmake_path(GET CMAKE_CURRENT_BINARY_DIR PARENT_PATH metal_shaders_include_dir)
target_include_directories(Veldrid PRIVATE ${metal_shaders_include_dir})
