
cmake_minimum_required(VERSION 3.13)

project(vld_third_party LANGUAGES C CXX)

# glm


#option(GLM_TEST_ENABLE OFF FORCE)
#add_subdirectory(glm)

# GLFW
#option(GLFW_VULKAN_STATIC ON)
#option(GLFW_BUILD_DOCS OFF)
#option(GLFW_BUILD_TESTS OFF)
#option(GLFW_BUILD_EXAMPLES OFF)
#option(GLFW_BUILD_TESTS OFF)
#option(GLFW_INSTALL OFF)
#if (VKB_WSI_SELECTION STREQUAL WAYLAND)
#    set(GLFW_USE_WAYLAND ON)
#endif()

#add_subdirectory(glfw)

#set_property(TARGET glfw PROPERTY FOLDER "ThirdParty")


# glslang
option(ENABLE_SPVREMAPPER OFF)
option(ENABLE_GLSLANG_BINARIES OFF)
set(ENABLE_HLSL OFF)
option(ENABLE_OPT OFF)
option(BUILD_TESTING OFF)
option(BUILD_EXTERNAL OFF)

add_subdirectory(glslang)

if (NOT TARGET glslang-default-resource-limits)
    add_library(glslang-default-resource-limits
                glslang/StandAlone/ResourceLimits.cpp)

    set_property(TARGET glslang-default-resource-limits PROPERTY FOLDER "3rd-party/glslang")
    
    target_include_directories(glslang-default-resource-limits
                                PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/glslang/StandAlone)
endif()

if(NOT MSVC)
    target_compile_options(glslang PRIVATE
        "-Wno-logical-op-parentheses"
        "-Wno-unused-parameter")

    target_compile_options(SPIRV PRIVATE
        "-Wno-logical-op-parentheses"
        "-Wno-unused-parameter")
endif()

set_property(TARGET glslang PROPERTY FOLDER "3rd-party/glslang")
set_property(TARGET OGLCompiler PROPERTY FOLDER "3rd-party/glslang")
set_property(TARGET OSDependent PROPERTY FOLDER "3rd-party/glslang")
set_property(TARGET SPIRV PROPERTY FOLDER "3rd-party/glslang")
set_property(TARGET MachineIndependent PROPERTY FOLDER "3rd-party/glslang")
set_property(TARGET GenericCodeGen PROPERTY FOLDER "3rd-party/glslang")


# spirv-cross
#add_subdirectory(spirv-cross)

#set_property(TARGET spirv-cross PROPERTY FOLDER "3rd-party/spirv-cross")
#set_property(TARGET spirv-cross-core PROPERTY FOLDER "3rd-party/spirv-cross")
#set_property(TARGET spirv-cross-glsl PROPERTY FOLDER "3rd-party/spirv-cross")
#set_property(TARGET spirv-cross-cpp PROPERTY FOLDER "3rd-party/spirv-cross")
#set_property(TARGET spirv-cross-hlsl PROPERTY FOLDER "3rd-party/spirv-cross")
#set_property(TARGET spirv-cross-msl PROPERTY FOLDER "3rd-party/spirv-cross")
#set_property(TARGET spirv-cross-reflect PROPERTY FOLDER "3rd-party/spirv-cross")
#set_property(TARGET spirv-cross-util PROPERTY FOLDER "3rd-party/spirv-cross")
#set_property(TARGET spirv-cross-c PROPERTY FOLDER "3rd-party/spirv-cross")