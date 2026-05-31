alloy_add_example_executable(BindlessT2
    BindlessT2.cpp
)

compile_shader(BindlessT2_shaders
    SOURCES BindlessT2.hlsl
    TYPES
        ps_6_6
        vs_6_6
    WITH_DBG_INFO)

target_link_libraries(BindlessT2 PRIVATE BindlessT2_shaders)

add_shader_object_depends(BindlessT2_shaders
    FILES BindlessT2.cpp)
