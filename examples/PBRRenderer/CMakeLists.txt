
add_executable(PBRRenderer 
    main.cpp
    PBRRendererApp.cpp
    PBRRendererApp.hpp
    MeshRenderer.cpp
    MeshRenderer.hpp
    MeshBuilder.cpp
    MeshBuilder.hpp
    Scene.cpp
    Scene.hpp
    )

compile_shader(PBRRenderer_shaders SOURCES Shaders/PBRObjectShader.hlsl TYPES ps_6_0 vs_6_0 WITH_DBG_INFO)
target_link_libraries(PBRRenderer PRIVATE PBRRenderer_shaders AppBase HLSLCompiler)
target_compile_features(PBRRenderer PRIVATE cxx_std_20)


if(WIN32)
    alloy_copy_dxc_runtime_libs(PBRRenderer)
    pix_runtime_copy_binaries(PBRRenderer)
endif()