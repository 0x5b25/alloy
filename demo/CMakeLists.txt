
cmake_minimum_required(VERSION 3.13)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory("glfw")
add_subdirectory("glm")

set_target_properties(glfw glm PROPERTIES FOLDER "demo/3rd-party")

set(APP_SRC_SET
    "app/App.cpp"
    "app/App.hpp"
)

function(CreateDemoApp DemoName)    
    add_executable(${DemoName}
        ${APP_SRC_SET}
        "${DemoName}.cpp"
    )

    target_link_libraries(${DemoName} 
        PUBLIC 
            Veldrid
            glm::glm
            glfw
    )

    set_target_properties(${DemoName} PROPERTIES FOLDER "demo")

endfunction(CreateDemoApp)

CreateDemoApp(demo)
CreateDemoApp(uniformBufferTest)

