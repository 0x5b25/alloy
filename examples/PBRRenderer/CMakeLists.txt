
alloy_add_example_executable(PBRRenderer
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
# Force consumers to recompile in the same incremental build pass when
# generated shader headers change (works around Ninja's static planning
# of depfile-based implicit deps).
add_shader_object_depends(PBRRenderer_shaders
    FILES MeshRenderer.cpp)

target_link_libraries(PBRRenderer PRIVATE HLSLCompiler)
