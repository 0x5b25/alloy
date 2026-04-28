cmake_minimum_required(VERSION 3.28)

project(alloy_example
    DESCRIPTION "Example app for alloy basic functionalities"
    LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

include(cmake/FetchDXC.cmake)
include(cmake/CompileHLSL.cmake)

function(alloy_add_example_executable TARGET_NAME)

    add_executable(${TARGET_NAME} ${ARGN})

    target_link_libraries(${TARGET_NAME} PRIVATE AppBase)

    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

    if(WIN32)
        alloy_copy_dxc_runtime_libs(${TARGET_NAME})
        pix_runtime_copy_binaries(${TARGET_NAME})
    endif()

    if(APPLE)
        message(DEBUG "Setting rpath for ${TARGET_NAME}")
        # Apply rpaths as target properties so we don't depend on the
        # CMAKE_* variables happening to be in scope at add_executable
        # time. BUILD_RPATH covers running from the build tree, while
        # INSTALL_RPATH covers the installed/packaged layout where the
        # dylibs sit next to the executable.
        set_target_properties(${TARGET_NAME} PROPERTIES
            INSTALL_RPATH "@executable_path"
            BUILD_RPATH "@executable_path;@loader_path"
        )
    endif()

endfunction()

add_subdirectory(AppBase)
add_subdirectory(HLSLCompiler)
add_subdirectory(SimpleQuad)
add_subdirectory(PBRRenderer)
add_subdirectory(SimpleMeshShader)
