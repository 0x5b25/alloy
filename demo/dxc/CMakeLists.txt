cmake_minimum_required(VERSION 3.12)
project(DXC VERSION 1.0)

# Define the path to the DXC binaries relative to this CMakeLists.txt
set(DXC_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin")

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
set(DXC_COMPILER_DLL "${DXC_BIN_DIR}/${DXC_ARCH}/dxcompiler.dll" PARENT_SCOPE)
set(DXC_DXIL_DLL "${DXC_BIN_DIR}/${DXC_ARCH}/dxil.dll" PARENT_SCOPE)

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
set(DXC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/inc")
target_include_directories(dxc INTERFACE "${DXC_INCLUDE_DIR}")


set(testA "abcd")

# Function to copy DLLs to target directory
function(dxc_copy_binaries TARGET)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DXC_COMPILER_DLL}"
            "${DXC_DXIL_DLL}"
            $<TARGET_FILE_DIR:${TARGET}>
    )
endfunction()

# Export DXC variables for use in parent projects
set(DXC_FOUND TRUE PARENT_SCOPE)
set(DXC_INCLUDE_DIR ${DXC_INCLUDE_DIR} PARENT_SCOPE)
