cmake_minimum_required(VERSION 3.13)

project(veldridcpp
    DESCRIPTION "A C++ port of the legendary veldrid library"
    LANGUAGES CXX)

set(VLD_MISC_HEADERS
    "include/veldrid/backend/Backends.hpp"
    "include/veldrid/common/Common.hpp"
    "include/veldrid/common/Macros.h"
    "include/veldrid/common/RefCnt.hpp"
)

set(VLD_IFACE_HEADERS
    "include/veldrid/BindableResource.hpp"
    "include/veldrid/Buffer.hpp"
    "include/veldrid/CommandList.hpp"
    "include/veldrid/DeviceResource.hpp"
    "include/veldrid/Fence.hpp"
    "include/veldrid/FixedFunctions.hpp"
    "include/veldrid/Framebuffer.hpp"
    "include/veldrid/GraphicsDevice.hpp"
    "include/veldrid/Helpers.hpp"
    "include/veldrid/Pipeline.hpp"
    "include/veldrid/ResourceFactory.hpp"
    "include/veldrid/Sampler.hpp"
    "include/veldrid/Shader.hpp"
    "include/veldrid/SwapChain.hpp"
    "include/veldrid/SwapChainSources.hpp"
    "include/veldrid/Texture.hpp"
    "include/veldrid/Types.hpp"
)

add_subdirectory("3rd-party")

add_library(Veldrid STATIC
    ${VLD_MISC_HEADERS}
    ${VLD_IFACE_HEADERS}
    "src/GraphicsDevice.cpp"
    "src/BindableResource.cpp"
    "src/Shader.cpp"
    "src/Helpers.cpp"
    "src/DeviceResource.cpp")

if (WIN32)
    add_definitions(-DNOMINMAX)
endif()

target_link_libraries(Veldrid 
    PUBLIC
    #    common
    #    glm
        glslang
        OGLCompiler
        SPIRV
        spirv-cross-glsl
    #    spdlog
    )

include("src/backend/vk/CMakeLists.txt")

target_include_directories(Veldrid PUBLIC "include")
target_compile_features(Veldrid PUBLIC cxx_std_17)

add_subdirectory("demo")
