#cmake_minimum_required(VERSION 3.13)
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

set(VLD_BACKEND_MTL_SRCS
    "${CMAKE_CURRENT_LIST_DIR}/MetalDevice.mm"
    "${CMAKE_CURRENT_LIST_DIR}/MetalDevice.h"
    "${CMAKE_CURRENT_LIST_DIR}/MetalFrameBuffer.mm"
    "${CMAKE_CURRENT_LIST_DIR}/MetalFrameBuffer.h"
    "${CMAKE_CURRENT_LIST_DIR}/MetalSwapChain.mm"
    "${CMAKE_CURRENT_LIST_DIR}/MetalSwapChain.h"
    "${CMAKE_CURRENT_LIST_DIR}/MetalTexture.mm"
    "${CMAKE_CURRENT_LIST_DIR}/MetalTexture.h"
    "${CMAKE_CURRENT_LIST_DIR}/MetalPipeline.mm"
    "${CMAKE_CURRENT_LIST_DIR}/MetalPipeline.h"
    "${CMAKE_CURRENT_LIST_DIR}/MetalCommandList.mm"
    "${CMAKE_CURRENT_LIST_DIR}/MetalCommandList.h"
    "${CMAKE_CURRENT_LIST_DIR}/MetalShader.mm"
    "${CMAKE_CURRENT_LIST_DIR}/MetalShader.h"
    "${CMAKE_CURRENT_LIST_DIR}/MetalBindableResource.mm"
    "${CMAKE_CURRENT_LIST_DIR}/MetalBindableResource.h"
    "${CMAKE_CURRENT_LIST_DIR}/MetalResourceFactory.mm"
    "${CMAKE_CURRENT_LIST_DIR}/MetalResourceFactory.h"

    "${CMAKE_CURRENT_LIST_DIR}/MtlSurfaceUtil.mm"
    "${CMAKE_CURRENT_LIST_DIR}/MtlSurfaceUtil.h"


    "${CMAKE_CURRENT_LIST_DIR}/MtlTypeCvt.mm"
    "${CMAKE_CURRENT_LIST_DIR}/MtlTypeCvt.h"

    "${CMAKE_CURRENT_LIST_DIR}/MtlShaderCvtImpl.m"
)

#enable objc ARC for metal shader converter
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/MtlShaderCvtImpl.m" PROPERTIES COMPILE_FLAGS -fobjc-arc)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${VLD_BACKEND_MTL_SRCS})

#add_library(metal_shader_converter_impl STATIC "${CMAKE_CURRENT_LIST_DIR}/MtlShaderCvtImpl.m")
#target_compile_options(metal_shader_converter_impl PRIVATE -fobjc-arc)
#target_link_libraries(metal_shader_converter_impl 
#    PRIVATE
#        "-framework Metal"
#        MetalShaderConverter)

#target_compile_options(metal_shader_converter_impl PRIVATE "-I${CMAKE_CURRENT_SOURCE_DIR}/3rd-party/MetalShaderConverter/include")

target_sources(Veldrid
    PRIVATE
        ${VLD_BACKEND_MTL_SRCS})

target_link_libraries(Veldrid 
    PRIVATE
        "-framework Foundation"
        "-framework Metal"
        "-framework QuartzCore"
        "-framework IOKit"
        MetalShaderConverter
    #    metal_shader_converter_impl
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


function(alloy_install_metal_runtime_libs PREFIX_PATH COMP_NAME)
    install(CODE "MESSAGE(\"alloy: installing metal runtime libraries...\")" COMPONENT ${COMP_NAME})
    #message("alloy: installing metal runtime libraries to ${PREFIX_PATH}...")
    alloy_install_metal_shader_converter(${PREFIX_PATH} ${COMP_NAME})
endfunction()
