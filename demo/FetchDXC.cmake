include(FetchContent)

FetchContent_Declare(
  DXC
  URL      https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2407/dxc_2024_07_31.zip
  URL_HASH SHA256=E2627F004F0F9424D8C71EA1314D04F38C5A5096884AE9217F1F18BD320267B5
)

message( "Fetching DirectX Shader Compiler 1.8.2407..." )

FetchContent_MakeAvailable(DXC)

FetchContent_GetProperties(
    DXC
    SOURCE_DIR DXC_SRC
    BINARY_DIR DXC_BIN
    POPULATED DXC_FETCHED
)

if(${DXC_FETCHED})
    message( "DXC SRC      : ${DXC_SRC}" )
    message( "DXC Binaries : ${DXC_BIN}")
else()
    message( FATAL_ERROR "DirectX Shader Compiler fetch failed" )
endif()

# Define the path to the DXC binaries relative to this CMakeLists.txt
set(DXC_BIN_DIR "${DXC_SRC}/bin")
set(DXC_BIN_DIR "${DXC_BIN_DIR}" PARENT_SCOPE)

# Determine architecture
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(DXC_ARCH "arm64")
    else()
        set(DXC_ARCH "x64")
    endif()
else()
    set(DXC_ARCH "x86")
endif()

# Set paths to the DLLs
set(DXC_COMPILER_DLL "${DXC_BIN_DIR}/${DXC_ARCH}/dxcompiler.dll")
set(DXC_DXIL_DLL "${DXC_BIN_DIR}/${DXC_ARCH}/dxil.dll")

# Create imported targets for the DLLs
#add_library(DXC::compiler SHARED IMPORTED GLOBAL)
#set_target_properties(DXC::compiler PROPERTIES
#    IMPORTED_IMPLIB "${DXC_COMPILER_DLL}"
#    IMPORTED_LOCATION "${DXC_COMPILER_DLL}"
#)

add_library(dxc INTERFACE)

#add_library(DXC::dxil SHARED IMPORTED GLOBAL)
#set_target_properties(DXC::dxil PROPERTIES
#    IMPORTED_IMPLIB "${DXC_DXIL_DLL}"
#    IMPORTED_LOCATION "${DXC_DXIL_DLL}"
#)

# Add include directories
set(DXC_INCLUDE_DIR "${DXC_SRC}/inc")
target_include_directories(dxc INTERFACE "${DXC_INCLUDE_DIR}")

# Function to copy DLLs to target directory
function(dxc_copy_binaries TARGET)
    
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND echo "Copying DirectX shader compiler dlls to $<TARGET_FILE_DIR:${TARGET}>/D3D12 ..."
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DXC_COMPILER_DLL}"
            "${DXC_DXIL_DLL}"
            $<TARGET_FILE_DIR:${TARGET}>
    )
endfunction()


function(alloy_install_dxc_binaries COMP_NAME PREFIX_PATH)

    install(
        PROGRAMS
            "${DXC_COMPILER_DLL}"
            "${DXC_DXIL_DLL}"
        DESTINATION
            ${PREFIX_PATH}
        COMPONENT
            ${COMP_NAME}
    )

endfunction()

# Export DXC variables for use in parent projects
set(DXC_FOUND TRUE PARENT_SCOPE)
set(DXC_INCLUDE_DIR ${DXC_INCLUDE_DIR} PARENT_SCOPE)
