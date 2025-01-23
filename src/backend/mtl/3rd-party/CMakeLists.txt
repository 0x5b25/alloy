

#Metal shader converter
add_library(MetalShaderConverter SHARED IMPORTED GLOBAL)
set_target_properties(MetalShaderConverter PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/MetalShaderConverter/lib/libmetalirconverter.dylib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/MetalShaderConverter/include"
)
set_property(TARGET MetalShaderConverter PROPERTY FOLDER "3rd-party/mtl")

add_library(demo_dep INTERFACE)
target_include_directories(demo_dep INTERFACE "${CMAKE_CURRENT_LIST_DIR}/demo_dep/inc")
