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

set(VLD_BACKEND_VK_SRCS
    "${CMAKE_CURRENT_LIST_DIR}/VulkanDevice.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanDevice.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanResourceFactory.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanResourceFactory.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanTexture.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanTexture.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanFramebuffer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanFramebuffer.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanSwapChain.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanSwapChain.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanShader.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanShader.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanCommandList.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanCommandList.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanBindableResource.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanBindableResource.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanPipeline.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/VulkanPipeline.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/VkCommon.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/VkCommon.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/VkTypeCvt.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/VkTypeCvt.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/VkDescriptorPoolMgr.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/VkDescriptorPoolMgr.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/VkSurfaceUtil.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/VkSurfaceUtil.hpp"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${VLD_BACKEND_VK_SRCS})

target_sources(Veldrid
    PRIVATE
        ${VLD_BACKEND_VK_SRCS})

target_link_libraries(Veldrid 
    PRIVATE
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
    )
