cmake_minimum_required(VERSION 3.28)

project(veldridcpp
    DESCRIPTION "A C++ port of the legendary veldrid library"
    LANGUAGES CXX)

set(VLD_MISC_HEADERS
    "include/alloy/backend/Backends.hpp"
    "include/alloy/common/Common.hpp"
    "include/alloy/common/Macros.h"
    "include/alloy/common/RefCnt.hpp"
)

set(VLD_IFACE_HEADERS
    "include/alloy/RenderPass.hpp"
    "include/alloy/CommandQueue.hpp"
    "include/alloy/Buffer.hpp"
    "include/alloy/CommandList.hpp"
    "include/alloy/BindableResource.hpp"
    "include/alloy/SyncObjects.hpp"
    "include/alloy/FixedFunctions.hpp"
    "include/alloy/FrameBuffer.hpp"
    "include/alloy/GraphicsDevice.hpp"
    "include/alloy/Helpers.hpp"
    "include/alloy/Pipeline.hpp"
    "include/alloy/ResourceFactory.hpp"
    "include/alloy/Sampler.hpp"
    "include/alloy/Shader.hpp"
    "include/alloy/SwapChain.hpp"
    "include/alloy/SwapChainSources.hpp"
    "include/alloy/Texture.hpp"
    "include/alloy/Types.hpp"
)


set(VLD_IFACE_IMP_SRCS
    "src/GraphicsDevice.cpp"
    "src/BindableResource.cpp"
    "src/Shader.cpp"
    "src/Helpers.cpp"
    #"src/DeviceResource.cpp"
    "src/Backends.cpp"
)

source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${VLD_MISC_HEADERS} ${VLD_IFACE_HEADERS} ${VLD_IFACE_IMP_SRCS})

add_subdirectory("3rd-party")

#platform detection
if(WIN32)
    set(VLD_BACKEND_DXC true)
    set(VLD_BACKEND_VK  true)
    set(VLD_BACKEND_MTL false)
elseif(APPLE)
    enable_language(OBJCXX)
    set(VLD_BACKEND_DXC false)
    set(VLD_BACKEND_VK  false)
    set(VLD_BACKEND_MTL true)
elseif(UNIX)
    set(VLD_BACKEND_DXC false)
    set(VLD_BACKEND_VK  true)
    set(VLD_BACKEND_MTL false)
endif()

function(PrintBackendInfo)
    if(WIN32)
        message("Platform       : Windows")
    elseif(APPLE)
        message("Platform       : Apple")
    elseif(UNIX)
        message("Platform       : *nix")
    endif()

    if(${VLD_BACKEND_DXC})
        message("Backend DX12   : Enabled")
    else()
        message("Backend DX12   : Disabled")
    endif()

    if(${VLD_BACKEND_VK})
        message("Backend Vulkan : Enabled")
    else()
        message("Backend Vulkan : Disabled")
    endif()

    if(${VLD_BACKEND_MTL})
        message("Backend Metal  : Enabled")
    else()
        message("Backend Metal  : Disabled")
    endif()
endfunction(PrintBackendInfo)

PrintBackendInfo()



add_library(Veldrid STATIC
    ${VLD_MISC_HEADERS}
    ${VLD_IFACE_HEADERS}
    ${VLD_IFACE_IMP_SRCS}
    )

if(${VLD_BACKEND_DXC})
    target_compile_definitions(Veldrid PRIVATE VLD_BACKEND_DXC=1)
    include("src/backend/dx12/CMakeLists.txt")
endif()

if(${VLD_BACKEND_VK})
    target_compile_definitions(Veldrid PRIVATE VLD_BACKEND_VK=1)
    include("src/backend/vk/CMakeLists.txt")
endif()

if(${VLD_BACKEND_MTL})
    target_compile_definitions(Veldrid PRIVATE VLD_BACKEND_MTL=1)
    include("src/backend/mtl/CMakeLists.txt")
endif()


target_link_libraries(Veldrid 
    PUBLIC
    #    common
    #    glm
    #    glslang
    #    OGLCompiler
    #    SPIRV
    #    spirv-cross-glsl
    #    spdlog
    )

if(WIN32)
    target_compile_definitions(Veldrid PUBLIC NOMINMAX)
    #add_definitions(-DNOMINMAX)
endif()

target_include_directories(Veldrid PUBLIC "include" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(Veldrid PUBLIC cxx_std_20)

function(link_with_veldrid TARGET)

    target_link_libraries(${TARGET} PRIVATE Veldrid)

    if(${VLD_BACKEND_DXC})
        agility_sdk_copy_binaries(${TARGET})
    endif()

endfunction()

add_subdirectory("demo")

#add_executable("APITest" "demo.cpp")
#target_link_libraries(APITest PRIVATE Veldrid)
