
add_executable(SimpleQuad 
    SimpleQuad.cpp
    )

compile_shader(SimpleQuad_shaders SOURCES QuadShader.hlsl TYPES ps_6_0 vs_6_0 WITH_DBG_INFO)
target_link_libraries(SimpleQuad PRIVATE SimpleQuad_shaders AppBase)
target_compile_features(SimpleQuad PRIVATE cxx_std_20)


if(WIN32)
    alloy_copy_dxc_runtime_libs(SimpleQuad)
    pix_runtime_copy_binaries(SimpleQuad)
endif()