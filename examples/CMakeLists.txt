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

add_subdirectory(AppBase)
add_subdirectory(HLSLCompiler)
add_subdirectory(SimpleQuad)
add_subdirectory(PBRRenderer)
add_subdirectory(SimpleMeshShader)
