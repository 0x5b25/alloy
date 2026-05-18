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

set(VLD_BACKEND_DXC_SRCS
    "${CMAKE_CURRENT_LIST_DIR}/DXCContext.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/DXCContext.hpp"
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
    "${CMAKE_CURRENT_LIST_DIR}/D3DPixEventEncoder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/D3DPixEventEncoder.hpp"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${VLD_BACKEND_DXC_SRCS})

target_sources(Veldrid
    PRIVATE
        ${VLD_BACKEND_DXC_SRCS})

target_link_libraries(Veldrid 
    PRIVATE
        D3D12MemoryAllocator
        AgilitySDK
    #    DirectX-Headers
    #    d3d12.lib
    #    dxgi.lib
    #    WinPixEventRuntime
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


function(alloy_install_dxc_runtime_deps)
    set(oneValueArgs COMPONENT RUNTIME_DESTINATION DX12_DESTINATION)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "alloy_install_dxc_runtime_deps: unexpected arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT ARG_RUNTIME_DESTINATION)
        message(FATAL_ERROR
            "alloy_install_dxc_runtime_deps: RUNTIME_DESTINATION is required")
    endif()

    if(NOT ARG_DX12_DESTINATION)
        set(ARG_DX12_DESTINATION "${ARG_RUNTIME_DESTINATION}/D3D12")
    endif()

    set(_component_args)
    if(ARG_COMPONENT)
        list(APPEND _component_args COMPONENT "${ARG_COMPONENT}")
    endif()

    install(CODE "message(\"alloy: installing DirectX 12 runtime libraries...\")"
        ${_component_args})

    alloy_install_agility_sdk_runtime_deps(
        ${_component_args}
        DX12_DESTINATION "${ARG_DX12_DESTINATION}")

    if(COMMAND alloy_install_pix_runtime_deps)
        alloy_install_pix_runtime_deps(
            ${_component_args}
            RUNTIME_DESTINATION "${ARG_RUNTIME_DESTINATION}")
    endif()
endfunction()

function(alloy_copy_dxc_runtime_libs TARGET)
        
    message("alloy: will copy directx 12 runtime libraries to ${TARGET} binary path...")

    agility_sdk_copy_binaries(${TARGET})

endfunction()
