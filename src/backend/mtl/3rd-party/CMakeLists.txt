

#Metal shader converter


#message("******** Building for ${CMAKE_SYSTEM_NAME} ********")
#
#if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
#    set(MTL_SH_CVT_FRAMEWORK "MetalShaderConverter.xcframework")
#elseif (${CMAKE_SYSTEM_NAME} MATCHES "iOS")
#    set(MTL_SH_CVT_FRAMEWORK "MetalShaderConverter-ios.framework")
#endif()

set(MTL_SH_CVT_FRAMEWORK "MetalShaderConverter.xcframework")
set(MTL_SH_CVT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/MetalShaderConverter/bin")
set(MTL_SH_CVT_XCFRAMEWORK "${MTL_SH_CVT_ROOT}/${MTL_SH_CVT_FRAMEWORK}")


add_library(MetalShaderConverter SHARED IMPORTED GLOBAL)
set_target_properties(MetalShaderConverter PROPERTIES
    #IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/MetalShaderConverter/lib/libmetalirconverter.dylib"
    IMPORTED_LOCATION "${MTL_SH_CVT_XCFRAMEWORK}"
    INTERFACE_INCLUDE_DIRECTORIES "${MTL_SH_CVT_ROOT}/include"
)
set_property(TARGET MetalShaderConverter PROPERTY FOLDER "3rd-party/mtl")
#set_property(TARGET MetalShaderConverter PROPERTY FRAMEWORK TRUE)

#target_include_directories(MetalShaderConverter INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/MetalShaderConverter/include")

message("******** Using ${CMAKE_OSX_SYSROOT} for build ********")

function(alloy_install_metal_shader_converter_xcframework)
    set(oneValueArgs COMPONENT DESTINATION)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "alloy_install_metal_shader_converter_xcframework: unexpected arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT ARG_DESTINATION)
        message(FATAL_ERROR
            "alloy_install_metal_shader_converter_xcframework: DESTINATION is required")
    endif()

    get_target_property(_mtl_sh_cvt_xcframework MetalShaderConverter IMPORTED_LOCATION)

    if(NOT _mtl_sh_cvt_xcframework)
        message(FATAL_ERROR
            "MetalShaderConverter imported target does not have IMPORTED_LOCATION set")
    endif()

    if(NOT EXISTS "${_mtl_sh_cvt_xcframework}")
        message(FATAL_ERROR
            "MetalShaderConverter.xcframework was not found at '${_mtl_sh_cvt_xcframework}'. Run src/backend/mtl/3rd-party/MetalShaderConverter/package.sh first.")
    endif()

    set(_component_args)
    if(ARG_COMPONENT)
        list(APPEND _component_args COMPONENT "${ARG_COMPONENT}")
    endif()

    install(DIRECTORY
        "${_mtl_sh_cvt_xcframework}/"
        DESTINATION "${ARG_DESTINATION}/MetalShaderConverter.xcframework"
        ${_component_args})
endfunction()
