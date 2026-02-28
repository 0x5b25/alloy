cmake_minimum_required(VERSION 3.28)

project(veldridcpp
    DESCRIPTION "A C++ port of the legendary veldrid library"
    LANGUAGES C CXX)

#Determine platform
if(WIN32)
    message("-- alloy: Windows build")
    set(ALLOY_PLATFORM "Win32")
elseif(ANDROID)
    message("-- alloy: Android build")
    set(ALLOY_PLATFORM "Android")
elseif(APPLE)
    if(IOS)
    	if(${CMAKE_OSX_SYSROOT} MATCHES "iphonesimulator")
            message("-- alloy: iOS simulator build")
            set(ALLOY_PLATFORM "iOSSim")
    	elseif(${CMAKE_OSX_SYSROOT} MATCHES "iphoneos")
            message("-- alloy: iOS build")
            set(ALLOY_PLATFORM "iOS")
    	else()
    		message(FATAL_ERROR"-- alloy: unknown iOS build")
    	endif()
    elseif(${CMAKE_OSX_SYSROOT} MATCHES "\/MacOSX.platform\/Developer\/SDKs")
        message("-- alloy: MacOS build")
        set(ALLOY_PLATFORM "MacOS")
    else()
    	message(FATAL_ERROR "-- alloy: unknown apple platform")
    endif()
elseif(UNIX)
    message("-- alloy: Linux build")
    set(ALLOY_PLATFORM "Linux")
else()
    message(FATAL_ERROR "-- alloy: unknown platform")
endif()

#determine ISA
if(IOS)
    if (${CMAKE_OSX_ARCHITECTURES} MATCHES "arm64")
        message("-- alloy: CPU type: arm64")
        set(ALLOY_ABI "arm64")
    elseif(${CMAKE_OSX_ARCHITECTURES} MATCHES "x86_64")
        message("-- alloy: CPU type: x86_64")
        set(ALLOY_ABI "x86_64")
    else()
        message(FATAL_ERROR "-- alloy: unsupported CPU: ${CMAKE_OSX_ARCHITECTURES}")
    endif()
elseif(ANDROID)
    if(${ANDROID_ABI} STREQUAL "x86_64")
        message("-- alloy: CPU type: x86_64")
        set(ALLOY_ABI "x86_64")
    elseif(${ANDROID_ABI} STREQUAL "armeabi-v7a")
        message("-- alloy: CPU type: armeabi-v7a")
        set(ALLOY_ABI "armv7a")
    elseif(${ANDROID_ABI} STREQUAL "arm64-v8a")
        message("-- alloy: CPU type: arm64")
        set(ALLOY_ABI "arm64")
    else()
        message(FATAL_ERROR "-- alloy: unsupported CPU: ${ANDROID_ABI}")
    endif()
else()
    if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
        message("-- alloy: CPU type: x86_64")
        set(ALLOY_ABI "x86_64")
    elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm64")
        message("-- alloy: CPU type: arm64")
        set(ALLOY_ABI "arm64")
    else()
        message(FATAL_ERROR "-- alloy: unsupported CPU: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

endif()


set(VLD_MISC_HEADERS
    "include/alloy/backend/Backends.hpp"
    "include/alloy/common/Common.hpp"
    "include/alloy/common/Macros.h"
    "include/alloy/common/RefCnt.hpp"
    "include/alloy/common/Waitable.hpp"
    "include/alloy/common/BitFlags.hpp"
)

set(VLD_MISC_SRCS

    "src/common/Waitable.cpp"
    "src/utils/Allocators.cpp"
    "src/utils/Allocators.hpp"
    #"src/utils/ResourceStateTracker.cpp"
    #"src/utils/ResourceStateTracker.hpp"
)


set(VLD_IFACE_HEADERS
    "include/alloy/Context.hpp"
    "include/alloy/RenderPass.hpp"
    "include/alloy/CommandQueue.hpp"
    "include/alloy/Buffer.hpp"
    "include/alloy/CommandList.hpp"
    "include/alloy/ResourceBarrier.hpp"
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
    "src/Context.cpp"
)

source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${VLD_MISC_HEADERS} ${VLD_MISC_SRCS} ${VLD_IFACE_HEADERS} ${VLD_IFACE_IMP_SRCS})

add_subdirectory("3rd-party")

#platform detection
if(WIN32)
    set(VLD_BACKEND_DXC true)
    set(VLD_BACKEND_VK  true)
    set(VLD_BACKEND_MTL false)
elseif(APPLE)
    set(VLD_BACKEND_DXC false)
    set(VLD_BACKEND_VK  false)
    set(VLD_BACKEND_MTL true)
elseif(UNIX)
    set(VLD_BACKEND_DXC false)
    set(VLD_BACKEND_VK  true)
    set(VLD_BACKEND_MTL false)
endif()

if(VLD_BACKEND_DXC)
    set(VLD_BACKEND_DXC true PARENT_SCOPE)
endif()


if(VLD_BACKEND_VK)
    set(VLD_BACKEND_VK true PARENT_SCOPE)
endif()

if(VLD_BACKEND_MTL)
    set(VLD_BACKEND_MTL true PARENT_SCOPE)
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

if(WIN32)
    #target_compile_definitions(Veldrid PUBLIC NOMINMAX)
    #add_definitions(-DNOMINMAX)
    message("Disable defines of min and max in windows headers")
    add_compile_definitions(NOMINMAX)
endif()

add_library(Veldrid STATIC
    ${VLD_MISC_HEADERS}
    ${VLD_MISC_SRCS}
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
    enable_language(OBJC)
    enable_language(OBJCXX)
    #target_compile_options(Veldrid PRIVATE -fobjc-arc)
    #target_compile_features(Veldrid PUBLIC objcxx_std_23)
    include("src/backend/mtl/CMakeLists.txt")
endif()

target_sources(Veldrid
    PUBLIC
    FILE_SET  vld_headers
    TYPE      HEADERS
    BASE_DIRS include
    FILES     ${VLD_MISC_HEADERS} ${VLD_IFACE_HEADERS} )


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


target_include_directories(Veldrid 
    PUBLIC 
        "include"
        "${CMAKE_CURRENT_BINARY_DIR}"
    PRIVATE
        "src")
target_compile_features(Veldrid PUBLIC cxx_std_20)


function(link_with_veldrid TARGET)
    target_link_libraries(${TARGET} PRIVATE Veldrid)
    
    if(${VLD_BACKEND_DXC})
        alloy_copy_dxc_runtime_libs(${TARGET})
    #else()
    endif()
endfunction()

function(alloy_install_runtime_libs PREFIX_PATH COMP_NAME)

    install(CODE "MESSAGE(\"alloy: installing runtime libraries for each backend to ${PREFIX_PATH}...\")"  COMPONENT ${COMP_NAME})
    if(${VLD_BACKEND_DXC})
        alloy_install_dxc_runtime_libs(${PREFIX_PATH} ${COMP_NAME})
    endif()

    if(${VLD_BACKEND_VK})
        alloy_install_vulkan_runtime_libs(${PREFIX_PATH} ${COMP_NAME})
    endif()

    if(${VLD_BACKEND_MTL})
        alloy_install_metal_runtime_libs(${PREFIX_PATH} ${COMP_NAME})
    endif()
endfunction()

if(${ALLOY_BUILD_TESTS})
    add_subdirectory("demo")
endif()

if(ALLOY_INSTALL_LIBS)
    message("Will install libraries to ${ALLOY_INSTALL_LIBS}")
    install(TARGETS Veldrid LIBRARY DESTINATION "${ALLOY_INSTALL_LIBS}")
endif()

if(ALLOY_INSTALL_HEADERS)
    message("Will install headers to ${ALLOY_INSTALL_HEADERS}")
    install(TARGETS Veldrid FILE_SET vld_headers DESTINATION "${ALLOY_INSTALL_HEADERS}")
endif()


if(ALLOY_INSTALL_RUNTIME_DEPS)
    message("Will install runtime dependencies to ${ALLOY_INSTALL_RUNTIME_DEPS}")
    alloy_install_runtime_libs("${ALLOY_INSTALL_RUNTIME_DEPS}" NO)
endif()


#install(TARGETS Veldrid 
#    COMPONENT alloy
#    FILE_SET vld_headers DESTINATION "${CMAKE_INSTALL_PREFIX}/include"
#    RUNTIME              DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
#    LIBRARY              DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")

