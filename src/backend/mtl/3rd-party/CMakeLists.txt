

#Metal shader converter


#message("******** Building for ${CMAKE_SYSTEM_NAME} ********")
#
#if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
#    set(MTL_SH_CVT_FRAMEWORK "MetalShaderConverter.xcframework")
#elseif (${CMAKE_SYSTEM_NAME} MATCHES "iOS")
#    set(MTL_SH_CVT_FRAMEWORK "MetalShaderConverter-ios.framework")
#endif()

set(MTL_SH_CVT_FRAMEWORK "MetalShaderConverter.xcframework")


add_library(MetalShaderConverter SHARED IMPORTED GLOBAL)
set_target_properties(MetalShaderConverter PROPERTIES
    #IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/MetalShaderConverter/lib/libmetalirconverter.dylib"
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/MetalShaderConverter/bin/${MTL_SH_CVT_FRAMEWORK}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/bin/include"
)
set_property(TARGET MetalShaderConverter PROPERTY FOLDER "3rd-party/mtl")
#set_property(TARGET MetalShaderConverter PROPERTY FRAMEWORK TRUE)

#target_include_directories(MetalShaderConverter INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/MetalShaderConverter/include")

message("******** Using ${CMAKE_OSX_SYSROOT} for build ********")

function(alloy_install_metal_shader_converter COMP_NAME PREFIX_PATH)

    #install(IMPORTED_RUNTIME_ARTIFACTS MetalShaderConverter
    #        RUNTIME
    #            DESTINATION ${PREFIX_PATH}
    #           COMPONENT ${COMP_NAME}
    #       LIBRARY
    #           DESTINATION ${PREFIX_PATH}
    #           COMPONENT ${COMP_NAME}
    #   )
    install(DIRECTORY
        "src/backend/mtl/3rd-party/MetalShaderConverter/bin/MetalShaderConverter.xcframework/"
        DESTINATION "${PREFIX_PATH}/MetalShaderConverter.xcframework/"
        COMPONENT ${COMP_NAME})

endfunction()
