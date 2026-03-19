
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

compile_shader(PBRRenderer_shaders Shaders/PBRObjectShader.hlsl)
target_link_libraries(PBRRenderer PRIVATE PBRRenderer_shaders AppBase HLSLCompiler)
target_compile_features(PBRRenderer PRIVATE cxx_std_20)


if(WIN32)
    alloy_copy_dxc_runtime_libs(PBRRenderer)
endif()