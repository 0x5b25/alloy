
add_executable(PBRRenderer 
    main.cpp
    PBRRendererApp.cpp
    PBRRendererApp.hpp
    MeshRenderer.cpp
    MeshRenderer.hpp
    MeshBuilder.cpp
    MeshBuilder.hpp
    )

target_link_libraries(PBRRenderer PRIVATE AppBase)
target_compile_features(PBRRenderer PRIVATE cxx_std_20)

if(WIN32)
    alloy_copy_dxc_runtime_libs(PBRRenderer)
endif()