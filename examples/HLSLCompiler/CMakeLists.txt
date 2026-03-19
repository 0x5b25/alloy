add_library(HLSLCompiler
    HLSLCompiler.cpp
    HLSLCompiler.hpp
)

target_compile_features(HLSLCompiler PUBLIC cxx_std_20)
target_include_directories(HLSLCompiler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(HLSLCompiler PRIVATE dxcompiler)