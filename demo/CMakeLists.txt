
#cmake_minimum_required(VERSION 3.13)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory("glfw")
add_subdirectory("glm")
include("FetchDXC.cmake")

set_target_properties(glfw glm PROPERTIES FOLDER "demo/3rd-party")

set(APP_SRC_SET
    "app/App.cpp"
    "app/App.hpp"
    "app/HLSLCompiler.cpp"
    "app/HLSLCompiler.hpp"
)

function(CreateDemoApp DemoName)    
    add_executable(${DemoName}
        ${APP_SRC_SET}
        "${DemoName}.cpp"
    )

    target_link_libraries(${DemoName} 
        PUBLIC
            glm::glm
            glfw
    )
    link_with_veldrid(${DemoName})
    target_link_libraries(${DemoName} PRIVATE dxc)
    dxc_copy_binaries(${DemoName})  # If you need the DLLs in your output directory

    set_target_properties(${DemoName} PROPERTIES FOLDER "demo")

endfunction(CreateDemoApp)

#CreateDemoApp(demo)
CreateDemoApp(uniformBufferTest)
CreateDemoApp(textureTest)

