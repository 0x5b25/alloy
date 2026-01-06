
include(FetchContent)


#agility SDK fetch and setup
set(PIX_RUNTIME_VER 1.0.240308001)
FetchContent_Declare(
  WinPixEventRuntime
  URL      https://www.nuget.org/api/v2/package/WinPixEventRuntime/${PIX_RUNTIME_VER}
  #URL_HASH MD5=5588a7b18261c20068beabfb4f530b87
  URL_HASH SHA256=726ACC93D6968E2146261A1E415521747D50AD69894C2B42B5D0D4C29FD66EC4
  DOWNLOAD_NAME pix_${PIX_RUNTIME_VER}.zip
)


message( "Fetching WinPixEventRuntime ${PIX_RUNTIME_VER}..." )

FetchContent_MakeAvailable(WinPixEventRuntime)

FetchContent_GetProperties(
    WinPixEventRuntime
    SOURCE_DIR PIX_RUNTIME_SRC
    BINARY_DIR PIX_RUNTIME_BIN
    POPULATED PIX_RUNTIME_FETCHED
)

if(${PIX_RUNTIME_FETCHED})
    message( "WinPixEventRuntime SRC      : ${PIX_RUNTIME_SRC}" )
    message( "WinPixEventRuntime Binaries : ${PIX_RUNTIME_BIN}")
else()
    message( FATAL_ERROR "WinPixEventRuntime fetch failed" )
endif()



# Determine architecture
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(PIX_RUNTIME_ARCH "arm64")
    else()
        set(PIX_RUNTIME_ARCH "x64")
    endif()
else()
    message(FATAL_ERROR "Unsupported architecture")
endif()


set(PIX_RUNTIME_ROOT_DIR "${PIX_RUNTIME_SRC}")
set(PIX_RUNTIME_BIN_DIR "${PIX_RUNTIME_ROOT_DIR}/bin/${PIX_RUNTIME_ARCH}")

add_library(WinPixEventRuntime SHARED IMPORTED GLOBAL)
set_target_properties(WinPixEventRuntime 
    PROPERTIES 
        IMPORTED_LOCATION ${PIX_RUNTIME_BIN_DIR}
        IMPORTED_IMPLIB ${PIX_RUNTIME_BIN_DIR}/WinPixEventRuntime.lib)

# Add include directories
set(PIX_RUNTIME_INCLUDE_DIR "${PIX_RUNTIME_ROOT_DIR}/Include")
target_include_directories(WinPixEventRuntime INTERFACE "${PIX_RUNTIME_INCLUDE_DIR}")

# Set paths to the DLLs
#set(AGILITY_Core_DLL "${PIX_RUNTIME_ROOT_DIR}/bin/${PIX_RUNTIME_ARCH}/D3D12Core.dll" PARENT_SCOPE)
#set(AGILITY_DX12SDKLayers_DLL "${PIX_RUNTIME_ROOT_DIR}/bin/${PIX_RUNTIME_ARCH}/d3d12SDKLayers.dll" PARENT_SCOPE)

set_target_properties(WinPixEventRuntime
    PROPERTIES
        PIX_RUNTIME_DLL          "${PIX_RUNTIME_BIN_DIR}/WinPixEventRuntime.dll"
)

# Function to copy DLLs to target directory
function(pix_runtime_copy_binaries TARGET)

    get_target_property(PIX_RUNTIME_DLL_PATH WinPixEventRuntime PIX_RUNTIME_DLL)

    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND echo "Copying DX12 WinPixEventRuntime dlls to $<TARGET_FILE_DIR:${TARGET}> ..."
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PIX_RUNTIME_DLL_PATH}"
            "$<TARGET_FILE_DIR:${TARGET}>/"
    )
endfunction()


function(alloy_install_pix_runtime_binaries PREFIX_PATH COMP_NAME)

    get_target_property(PIX_RUNTIME_DLL_PATH WinPixEventRuntime PIX_RUNTIME_DLL)

    if(COMP_NAME)
        message("WinPixEventRuntime install using component ${COMP_NAME} into ${PREFIX_PATH}")
        install(
            PROGRAMS
                ${PIX_RUNTIME_DLL_PATH}
            DESTINATION
                ${PREFIX_PATH}
            COMPONENT
                ${COMP_NAME}
        )
    else()

        install(
            PROGRAMS
                ${PIX_RUNTIME_DLL_PATH}
            DESTINATION
                ${PREFIX_PATH}
            )
    endif()

endfunction()

# Export DXC variables for use in parent projects
set(PIX_RUNTIME_FOUND TRUE)
set(PIX_RUNTIME_DIR ${PIX_RUNTIME_SRC})
