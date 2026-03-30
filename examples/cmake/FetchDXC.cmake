include(FetchContent)

set(DXC_STATIC_VERSION "v0.1")
set(DXC_STATIC_BASE_URL "https://github.com/0x5b25/dxc_static/releases/download/${DXC_STATIC_VERSION}")

if(WIN32)
    set(HOST_OS windows)
    set(HOST_COMPILER "msvc")
    set(TARGET_OS ${HOST_OS})
    set(TARGET_COMPILER ${HOST_COMPILER})
elseif(APPLE)
    set(HOST_OS macos)
    set(HOST_COMPILER "appleclang")
    set(TARGET_COMPILER ${HOST_COMPILER})

    if(IOS)
        set(TARGET_OS ios)
    elseif(${CMAKE_OSX_SYSROOT} MATCHES "\/MacOSX.platform\/Developer\/SDKs")
        set(TARGET_OS macos)
    else()
    	message(FATAL_ERROR "unknown apple target platform")
    endif()

elseif(UNIX)
    set(HOST_OS linux)
    set(HOST_COMPILER "gcc")
    set(TARGET_OS ${HOST_OS})
    set(TARGET_COMPILER ${HOST_COMPILER})
else()
    message(FATAL_ERROR "-unknown platform")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
    set(HOST_CPU_ARCH "arm64")
else()
    set(HOST_CPU_ARCH "x86_64")
endif()

if(IOS)
    if (${CMAKE_OSX_ARCHITECTURES} MATCHES "arm64")
        set(TARGET_CPU_ARCH "arm64")
    elseif(${CMAKE_OSX_ARCHITECTURES} MATCHES "x86_64")
        set(TARGET_CPU_ARCH "x86_64")
    else()
        message(FATAL_ERROR "unsupported CPU: ${CMAKE_OSX_ARCHITECTURES}")
    endif()
else()
    set(TARGET_CPU_ARCH ${HOST_CPU_ARCH})
endif()

set(DXC_STATC_TARGET_TRIPLE ${TARGET_CPU_ARCH}-${TARGET_OS}-${TARGET_COMPILER})
set(DXC_STATC_HOST_TRIPLE ${HOST_CPU_ARCH}-${HOST_OS}-${HOST_COMPILER})

set(DXC_STATIC_LIB_NAME "libdxcompiler-${DXC_STATC_TARGET_TRIPLE}.7z")
set(DXC_EXECUTABLE_NAME "dxc-${DXC_STATC_TARGET_TRIPLE}.7z")

set(DXC_STATIC_LIB_URL "${DXC_STATIC_BASE_URL}/${DXC_STATIC_LIB_NAME}")
set(DXC_EXECUTABLE_URL "${DXC_STATIC_BASE_URL}/${DXC_EXECUTABLE_NAME}")


FetchContent_Declare(
    DXC_STATIC_LIB
    URL ${DXC_STATIC_LIB_URL})

FetchContent_Declare(
    DXC_EXECUTABLE
    URL ${DXC_EXECUTABLE_URL})

    
message(STATUS "Fetching libdxcompiler ${DXC_STATIC_VERSION} from ${DXC_STATIC_LIB_URL}...")
FetchContent_MakeAvailable(DXC_STATIC_LIB)

message(STATUS "Fetching dxc ${DXC_STATIC_VERSION} from ${DXC_EXECUTABLE_URL}...")
FetchContent_MakeAvailable(DXC_EXECUTABLE)

FetchContent_GetProperties(DXC_STATIC_LIB SOURCE_DIR DXC_STATIC_LIB_DIR)
FetchContent_GetProperties(DXC_EXECUTABLE SOURCE_DIR DXC_EXECUTABLE_DIR)

if(WIN32)
    set(DXC_STATIC_LIB_BINARY_NAME dxcompiler.lib)
else()
    set(DXC_STATIC_LIB_BINARY_NAME libdxcompiler.a)
endif()


add_library(dxcompiler STATIC IMPORTED GLOBAL)
set_target_properties(dxcompiler 
    PROPERTIES
        IMPORTED_LOCATION_DEBUG ${DXC_STATIC_LIB_DIR}/lib/Debug/${DXC_STATIC_LIB_BINARY_NAME}
        IMPORTED_LOCATION ${DXC_STATIC_LIB_DIR}/lib/Release/${DXC_STATIC_LIB_BINARY_NAME}
)
target_include_directories(dxcompiler INTERFACE ${DXC_STATIC_LIB_DIR}/include)
