

add_subdirectory("3rd-party")

add_library(AppBase
    AppRunner.cpp    
    AppRunner.hpp    
)

target_compile_features(AppBase PUBLIC cxx_std_20)
target_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(AppBase PUBLIC glfw glm imgui Veldrid)
