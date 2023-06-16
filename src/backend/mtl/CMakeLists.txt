cmake_minimum_required(VERSION 3.13)
#project(vk_backend VERSION 0.1.0)



#add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/3rd-party")


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

set(VLD_BACKEND_MTL_SRCS
    "${CMAKE_CURRENT_LIST_DIR}/MetalDevice.mm"
    "${CMAKE_CURRENT_LIST_DIR}/MetalDevice.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/MetalSwapChain.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/MetalSwapChain.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/MetalTexture.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/MetalTexture.hpp"

    "${CMAKE_CURRENT_LIST_DIR}/MtlSurfaceUtil.mm"
    "${CMAKE_CURRENT_LIST_DIR}/MtlSurfaceUtil.hpp"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${VLD_BACKEND_MTL_SRCS})

target_sources(Veldrid
    PRIVATE
        ${VLD_BACKEND_MTL_SRCS})

target_link_libraries(Veldrid 
    PRIVATE
        "-framework Foundation"
        "-framework Metal"
        "-framework QuartzCore"
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
