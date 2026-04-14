
add_executable(SimpleMeshShader 
SimpleMeshShader.cpp
    )

compile_shader(SimpleMeshShader_shaders 
    SOURCES QuadMeshShader.hlsl 
    TYPES 
        as_6_7
        ms_6_7
        ps_6_7
    WITH_DBG_INFO)

target_link_libraries(SimpleMeshShader PRIVATE SimpleMeshShader_shaders AppBase)
target_compile_features(SimpleMeshShader PRIVATE cxx_std_20)


if(WIN32)
    alloy_copy_dxc_runtime_libs(SimpleMeshShader)
    pix_runtime_copy_binaries(SimpleMeshShader)
endif()