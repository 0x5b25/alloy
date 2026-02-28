

#Metal shader converter


#message("******** Building for ${CMAKE_SYSTEM_NAME} ********")
#
#if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
#    set(MTL_SH_CVT_FRAMEWORK "MetalShaderConverter.xcframework")
#elseif (${CMAKE_SYSTEM_NAME} MATCHES "iOS")
#    set(MTL_SH_CVT_FRAMEWORK "MetalShaderConverter-ios.framework")
#endif()


set(MTL_SH_CVT_FRAMEWORK_NAME "MetalShaderConverter.xcframework")
if(ALLOY_PLATFORM STREQUAL "iOSSim")
    set(MTL_SH_CVT_SUBDIR ios-arm64_x86_64-simulator)
elseif(ALLOY_PLATFORM STREQUAL "iOS")
    set(MTL_SH_CVT_SUBDIR ios-arm64_x86_64)
elseif(ALLOY_PLATFORM STREQUAL "MacOS")
    set(MTL_SH_CVT_SUBDIR macos-arm64_x86_64)
else()
	message(FATAL_ERROR "-- alloy: metal shader converter: unknown platform")
endif()

set(MTL_SH_CVT_FRAMEWORK_PATH ${MTL_SH_CVT_FRAMEWORK_NAME}/${MTL_SH_CVT_SUBDIR}/MetalShaderConverter.framework/MetalShaderConverter)

add_library(MetalShaderConverter SHARED IMPORTED GLOBAL)
set_target_properties(MetalShaderConverter PROPERTIES
    #IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/MetalShaderConverter/lib/libmetalirconverter.dylib"
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/MetalShaderConverter/bin/${MTL_SH_CVT_FRAMEWORK_PATH}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/MetalShaderConverter/bin/include"
)
set_property(TARGET MetalShaderConverter PROPERTY FOLDER "3rd-party/mtl")
set_property(TARGET MetalShaderConverter PROPERTY FRAMEWORK TRUE)
set_property(TARGET MetalShaderConverter PROPERTY MSC_FRAMEWORK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/MetalShaderConverter/bin/${MTL_SH_CVT_FRAMEWORK}")
set_property(TARGET MetalShaderConverter PROPERTY MSC_FRAMEWORK_NAME "${MTL_SH_CVT_FRAMEWORK}")
if(IOS)
	if(${CMAKE_OSX_SYSROOT} MATCHES "iphonesimulator")
        set(FRAMEWORK_SUBDIR ios-arm64_x86_64-simulator)
	elseif(${CMAKE_OSX_SYSROOT} MATCHES "iphoneos")
        set(FRAMEWORK_SUBDIR ios-arm64_x86_64)
	else()
		message(FATAL_ERROR"-- iOS undefined build")
	endif()
elseif(${CMAKE_OSX_SYSROOT} MATCHES "\/MacOSX.platform\/Developer\/SDKs")
    set(FRAMEWORK_SUBDIR macos-arm64_x86_64)
else()
	message(FATAL_ERROR "-- Undefined build")
endif()
#set_property(TARGET Veldrid APPEND PROPERTY BUILD_RPATH "${MTL_SH_CVT_FRAMEWORK}/${FRAMEWORK_SUBDIR}")



#target_include_directories(MetalShaderConverter INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/MetalShaderConverter/include")

message("******** Using ${CMAKE_OSX_SYSROOT} for build ********")

function(alloy_install_metal_shader_converter PREFIX_PATH COMP_NAME)

    #install(IMPORTED_RUNTIME_ARTIFACTS MetalShaderConverter
    #        RUNTIME
    #            DESTINATION ${PREFIX_PATH}
    #           COMPONENT ${COMP_NAME}
    #       LIBRARY
    #           DESTINATION ${PREFIX_PATH}
    #           COMPONENT ${COMP_NAME}
    #   )
    if(COMP_NAME)
        install(DIRECTORY
            "src/backend/mtl/3rd-party/MetalShaderConverter/bin/MetalShaderConverter.xcframework/"
            DESTINATION "${PREFIX_PATH}/MetalShaderConverter.xcframework/"
            COMPONENT ${COMP_NAME})
    else()
        install(DIRECTORY
            "src/backend/mtl/3rd-party/MetalShaderConverter/bin/MetalShaderConverter.xcframework/"
            DESTINATION "${PREFIX_PATH}/MetalShaderConverter.xcframework/")
    endif()

endfunction()

# Function to copy DLLs to target directory
function(metal_shader_converter_copy_binaries TARGET)

    get_target_property(MSC_FRAMEWORK MetalShaderConverter MSC_FRAMEWORK_PATH)
    get_target_property(MSC_NAME MetalShaderConverter MSC_FRAMEWORK_NAME)

    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND echo "Copying metal shader converter to $<TARGET_FILE_DIR:${TARGET}> ..."
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${MSC_FRAMEWORK}"
            "$<TARGET_FILE_DIR:${TARGET}>/${MSC_NAME}"
    )
endfunction()
