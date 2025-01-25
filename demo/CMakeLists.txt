
#cmake_minimum_required(VERSION 3.13)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory("glfw")
add_subdirectory("glm")


if(APPLE)
    set(DXC_BIN_DIR "${CMAKE_CURRENT_LIST_DIR}/dxc")
    set(DXC_COMPILER_DLL "${DXC_BIN_DIR}/lib/libdxcompiler.dylib")
    set(DXC_DXIL_DLL "${DXC_BIN_DIR}/lib/libdxil.dylib")

    add_library(dxc::compiler SHARED IMPORTED)
    set_target_properties(dxc::compiler PROPERTIES
        IMPORTED_LOCATION ${DXC_COMPILER_DLL}
    )

    add_library(dxc INTERFACE)
    target_link_libraries(dxc INTERFACE dxc::compiler)
    target_include_directories(dxc INTERFACE "${DXC_BIN_DIR}/include")
    target_compile_options(dxc INTERFACE -fms-extensions)
    
    
    function(dxc_copy_binaries TARGET)
    
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND echo "Copying DirectX shader compiler dlls to $<TARGET_FILE_DIR:${TARGET}> ..."
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DXC_COMPILER_DLL}"
                "${DXC_DXIL_DLL}"
                $<TARGET_FILE_DIR:${TARGET}>
        )
    endfunction()

else()
    include("FetchDXC.cmake")
endif()


add_library(imgui STATIC)
target_sources(imgui
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_demo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp"
)
target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
target_link_libraries(imgui PRIVATE glfw)
set_target_properties(imgui PROPERTIES FOLDER "demo/3rd-party")


set_target_properties(glfw glm PROPERTIES FOLDER "demo/3rd-party")

add_library(demoAppBase STATIC
        "app/App.cpp"
        "app/App.hpp"
        "app/HLSLCompiler.cpp"
        "app/HLSLCompiler.hpp"
        "app/ImGuiBackend.cpp"
)

target_link_libraries(demoAppBase
    PUBLIC
        glm::glm
        glfw
        imgui
        dxc
        Veldrid
)
target_compile_features(demoAppBase PUBLIC cxx_std_20)

set_target_properties(demoAppBase PROPERTIES FOLDER "demo")

function(CreateDemoApp DemoName)    
    add_executable(${DemoName} "${DemoName}.cpp"
    )

    target_link_libraries(${DemoName} 
        PUBLIC demoAppBase
    )
    link_with_veldrid(${DemoName})
    target_link_libraries(${DemoName} PRIVATE dxc)
    dxc_copy_binaries(${DemoName})  # If you need the DLLs in your output directory

    #target_compile_options(${DemoName} PUBLIC -123456)
    set_target_properties(${DemoName} PROPERTIES FOLDER "demo")

endfunction(CreateDemoApp)

#CreateDemoApp(demo)
CreateDemoApp(uniformBufferTest)
CreateDemoApp(textureTest)
CreateDemoApp(imguiTest)
