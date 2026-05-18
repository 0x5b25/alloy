alloy_add_example_executable(BindlessT1
    BindlessT1.cpp
)

compile_shader(BindlessT1_shaders
    SOURCES BindlessT1.hlsl
    TYPES
        ps_6_0
        vs_6_0
    WITH_DBG_INFO)

target_link_libraries(BindlessT1 PRIVATE BindlessT1_shaders)

add_shader_object_depends(BindlessT1_shaders
    FILES BindlessT1.cpp)
