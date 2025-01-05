

#cmake_minimum_required(VERSION 3.13)

#project(vk_third_party LANGUAGES C CXX)

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
#option(ENABLE_SPVREMAPPER OFF)
#option(ENABLE_GLSLANG_BINARIES OFF)
#set(ENABLE_HLSL OFF)
#option(ENABLE_OPT OFF)
#option(BUILD_TESTING OFF)
#option(BUILD_EXTERNAL OFF)
#
#add_subdirectory(glslang)
#
#if (NOT TARGET glslang-default-resource-limits)
#    add_library(glslang-default-resource-limits
#                glslang/StandAlone/ResourceLimits.cpp)
#
#    set_property(TARGET glslang-default-resource-limits PROPERTY FOLDER "ThirdParty")
#    
#    target_include_directories(glslang-default-resource-limits
#                                PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/glslang/StandAlone)
#endif()
#
#if(NOT MSVC)
#    target_compile_options(glslang PRIVATE
#        "-Wno-logical-op-parentheses"
#        "-Wno-unused-parameter")
#
#    target_compile_options(SPIRV PRIVATE
#        "-Wno-logical-op-parentheses"
#        "-Wno-unused-parameter")
#endif()
#
#set_property(TARGET glslang PROPERTY FOLDER "ThirdParty")
#set_property(TARGET OGLCompiler PROPERTY FOLDER "ThirdParty")
#set_property(TARGET OSDependent PROPERTY FOLDER "ThirdParty")
#set_property(TARGET SPIRV PROPERTY FOLDER "ThirdParty")


# spirv-cross
#add_subdirectory(spirv-cross)
#
#set_property(TARGET spirv-cross PROPERTY FOLDER "ThirdParty")
#set_property(TARGET spirv-cross-core PROPERTY FOLDER "ThirdParty")
#set_property(TARGET spirv-cross-glsl PROPERTY FOLDER "ThirdParty")
#set_property(TARGET spirv-cross-cpp PROPERTY FOLDER "ThirdParty")
#set_property(TARGET spirv-cross-hlsl PROPERTY FOLDER "ThirdParty")
#set_property(TARGET spirv-cross-msl PROPERTY FOLDER "ThirdParty")
#set_property(TARGET spirv-cross-reflect PROPERTY FOLDER "ThirdParty")
#set_property(TARGET spirv-cross-util PROPERTY FOLDER "ThirdParty")

# vulkan
add_library(vulkan INTERFACE)
set(VULKAN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan-Headers/include)
target_sources(vulkan INTERFACE ${VULKAN_INCLUDE_DIR}/vulkan/vulkan.h)
target_include_directories(vulkan INTERFACE ${VULKAN_INCLUDE_DIR})

target_compile_definitions(vulkan INTERFACE VK_NO_PROTOTYPES)


if(ANDROID)
    set(VULKAN_WSI_DEF VK_USE_PLATFORM_ANDROID_KHR)
elseif(WIN32)
    set(VULKAN_WSI_DEF VK_USE_PLATFORM_WIN32_KHR)
elseif(APPLE)
	set(VULKAN_WSI_DEF VK_USE_PLATFORM_METAL_EXT)
elseif(UNIX)
    # Choose WSI based on VKB_WSI_SELECTION
    if (VKB_WSI_SELECTION STREQUAL XCB OR VKB_WSI_SELECTION STREQUAL XLIB OR VKB_WSI_SELECTION STREQUAL WAYLAND)
        find_package(PkgConfig REQUIRED)
    endif()
    if (VKB_WSI_SELECTION STREQUAL XCB)
        pkg_check_modules(XCB xcb REQUIRED)
        if (XCB_FOUND)
            set(VULKAN_WSI_DEF VK_USE_PLATFORM_XCB_KHR)
        endif()
    elseif (VKB_WSI_SELECTION STREQUAL XLIB)
        pkg_check_modules(X11 x11 REQUIRED)
        if (X11_FOUND)
            set(VULKAN_WSI_DEF VK_USE_PLATFORM_XLIB_KHR)
        endif()
    elseif (VKB_WSI_SELECTION STREQUAL WAYLAND)
        pkg_check_modules(WAYLAND wayland-client REQUIRED)
        if (WAYLAND_FOUND)
            set(VULKAN_WSI_DEF VK_USE_PLATFORM_WAYLAND_KHR)
        endif()
    elseif (VKB_WSI_SELECTION STREQUAL D2D)
        set(DIRECT_TO_DISPLAY TRUE)
        set(DIRECT_TO_DISPLAY TRUE PARENT_SCOPE)
        set(VULKAN_WSI_DEF VK_USE_PLATFORM_DISPLAY_KHR)
    else()
        message(FATAL_ERROR "Unknown WSI")
    endif()
endif()


target_compile_definitions(vulkan INTERFACE ${VULKAN_WSI_DEF})

set_property(TARGET vulkan PROPERTY FOLDER "3rd-party/vk")

# volk
set(VOLK_STATIC_DEFINES ${VULKAN_WSI_DEF})
add_subdirectory(volk)
target_link_libraries(volk PUBLIC vulkan)

#target_include_directories(volk PUBLIC ${VOLK_DIR})
if (VKB_WSI_SELECTION STREQUAL XCB)
    target_include_directories(volk PUBLIC ${XCB_INCLUDE_DIRS})
elseif (VKB_WSI_SELECTION STREQUAL XLIB)
    target_include_directories(volk PUBLIC ${X11_INCLUDE_DIRS})
elseif (VKB_WSI_SELECTION STREQUAL WAYLAND)
    target_include_directories(volk PUBLIC ${WAYLAND_INCLUDE_DIRS})
endif()

set_property(TARGET volk PROPERTY FOLDER "3rd-party/vk")


# spdlog
#add_subdirectory(spdlog)
##set_property(TARGET spdlog_headers_for_ide PROPERTY FOLDER "ThirdParty")
#set_property(TARGET spdlog_header_only PROPERTY FOLDER "ThirdParty")


# vma
add_library(vma INTERFACE)
set(VMA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vma/include")
target_sources(vma INTERFACE ${VMA_DIR}/vk_mem_alloc.h)
target_include_directories(vma INTERFACE ${VMA_DIR})
#target_link_libraries(vma PUBLIC vulkan)

set_property(TARGET vma PROPERTY FOLDER "3rd-party/vk")
# dxil-spirv

set(DXIL_SPIRV_CLI OFF CACHE BOOL "" FORCE)
add_subdirectory(dxil-spirv)
set_property(TARGET dxil-spirv-c-static PROPERTY FOLDER "3rd-party/vk/dxil-spv")
set_property(TARGET dxil-spirv-c-shared PROPERTY FOLDER "3rd-party/vk/dxil-spv")
set_property(TARGET dxil-converter PROPERTY FOLDER "3rd-party/vk/dxil-spv")
set_property(TARGET dxil-debug PROPERTY FOLDER "3rd-party/vk/dxil-spv")
set_property(TARGET dxil-utils PROPERTY FOLDER "3rd-party/vk/dxil-spv")