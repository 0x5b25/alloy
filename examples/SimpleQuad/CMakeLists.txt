
alloy_add_example_executable(SimpleQuad
    SimpleQuad.cpp
    )

compile_shader(SimpleQuad_shaders SOURCES QuadShader.hlsl TYPES ps_6_0 vs_6_0 WITH_DBG_INFO)
add_shader_object_depends(SimpleQuad_shaders
    FILES SimpleQuad.cpp)
