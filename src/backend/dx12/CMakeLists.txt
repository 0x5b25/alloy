cmake_minimum_required(VERSION 3.13)
#project(vk_backend VERSION 0.1.0)



add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/3rd-party")


#add_executable(VulkanTest 
#    main.cpp
#    Device.cpp
#    Shader.cpp
#     "Renderer.hpp" "Renderer.cpp" "Texture.hpp" "Texture.cpp" "DescriptorPool.hpp" "DescriptorPool.cpp")
#
#target_compile_features(VulkanTest PRIVATE cxx_std_17)
#
#target_link_libraries(VulkanTest PUBLIC
#    common
#    glm
#    vulkan
#    glslang
#    OGLCompiler
#    SPIRV
#    spirv-cross-glsl
#    glfw
#    volk
#    vma
#    spdlog)

set(VLD_BACKEND_DXC_SRCS
    "${CMAKE_CURRENT_LIST_DIR}/DXCDevice.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCDevice.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCResourceFactory.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCResourceFactory.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCTexture.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCTexture.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCFrameBuffer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCFrameBuffer.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCSwapChain.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCSwapChain.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCShader.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCShader.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCCommandList.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCCommandList.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCBindableResource.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCBindableResource.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCPipeline.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCPipeline.cpp"
    #"${CMAKE_CURRENT_LIST_DIR}/VkCommon.cpp"
    #"${CMAKE_CURRENT_LIST_DIR}/VkCommon.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/D3DTypeCvt.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/D3DTypeCvt.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/D3DDescriptorHeapMgr.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/D3DDescriptorHeapMgr.hpp"
    #"${CMAKE_CURRENT_LIST_DIR}/VkSurfaceUtil.cpp"
    #"${CMAKE_CURRENT_LIST_DIR}/VkSurfaceUtil.hpp"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${VLD_BACKEND_DXC_SRCS})

target_sources(Veldrid
    PRIVATE
        ${VLD_BACKEND_DXC_SRCS})

target_link_libraries(Veldrid 
    PRIVATE
        D3D12MemoryAllocator
        DirectX-Headers
        d3d12.lib
        dxgi.lib
    #    dxguid.lib
    #    common
    #    glm
    #    vulkan
    #    glslang
    #    OGLCompiler
    #    SPIRV
    #    spirv-cross-glsl
    #    glfw
    #    volk
    #    vma
    #    spdlog
    #    spirv_to_dxil
    #    vtn
    #    dxil_compiler
    )
