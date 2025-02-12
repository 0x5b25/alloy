
include(FetchContent)


#agility SDK fetch and setup
set(AGILITY_SDK_VER 1.614.1)
FetchContent_Declare(
  DX12AgilitySDK
  URL      https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/${AGILITY_SDK_VER}
  #URL_HASH MD5=5588a7b18261c20068beabfb4f530b87
  URL_HASH SHA256=9880AA91602DD51DD6CF7911A2BCA7A2323513B15338573CDE014B3356EEAFF2
  DOWNLOAD_NAME agility_${AGILITY_SDK_VER}.zip
)


message( "Fetching DX12 Agility SDK ${AGILITY_SDK_VER}..." )

FetchContent_MakeAvailable(DX12AgilitySDK)

FetchContent_GetProperties(
    DX12AgilitySDK
    SOURCE_DIR AGILITY_SDK_SRC
    BINARY_DIR AGILITY_SDK_BIN
    POPULATED AGILITY_SDK_FETCHED
)

if(${AGILITY_SDK_FETCHED})
    message( "Agility SDK SRC      : ${AGILITY_SDK_SRC}" )
    message( "Agility SDK Binaries : ${AGILITY_SDK_BIN}")
else()
    message( FATAL_ERROR "Agility SDK fetch failed" )
endif()

set(AGILITY_ROOT_DIR "${AGILITY_SDK_SRC}/build/native")

# Determine architecture
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(AGILITY_ARCH "arm64")
    else()
        set(AGILITY_ARCH "x64")
    endif()
else()
    set(AGILITY_ARCH "win32")
endif()

add_library(AgilitySDK INTERFACE)

# Add include directories
set(AGILITY_SDK_INCLUDE_DIR "${AGILITY_ROOT_DIR}/include")
target_include_directories(AgilitySDK INTERFACE "${AGILITY_SDK_INCLUDE_DIR}")
target_compile_definitions(AgilitySDK INTERFACE AGILITY_SDK_VERSION_EXPORT=614)

# Set paths to the DLLs
set(AGILITY_Core_DLL "${AGILITY_ROOT_DIR}/bin/${AGILITY_ARCH}/D3D12Core.dll" PARENT_SCOPE)
set(AGILITY_DX12SDKLayers_DLL "${AGILITY_ROOT_DIR}/bin/${AGILITY_ARCH}/d3d12SDKLayers.dll" PARENT_SCOPE)

# Function to copy DLLs to target directory
function(agility_sdk_copy_binaries TARGET)

    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND echo "Copying DX12 agility SDK dlls to $<TARGET_FILE_DIR:${TARGET}>/D3D12 ..."
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET}>/D3D12"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${AGILITY_Core_DLL}"
            "${AGILITY_DX12SDKLayers_DLL}"
            "$<TARGET_FILE_DIR:${TARGET}>/D3D12/"
    )
endfunction()


function(alloy_install_agility_sdk_binaries COMP_NAME PREFIX_PATH)

    install(
        PROGRAMS
            ${AGILITY_Core_DLL}
            ${AGILITY_DX12SDKLayers_DLL}
        DESTINATION
            ${PREFIX_PATH}/DX12
        COMPONENT
            ${COMP_NAME}
    )

endfunction()

# Export DXC variables for use in parent projects
set(AGILITY_SDK_FOUND TRUE)
set(AGILITY_SDK_DIR ${AGILITY_SDK_SRC})
