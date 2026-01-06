

#cmake_minimum_required(VERSION 3.28)

project(dxc_third_party LANGUAGES C CXX)

include(FetchAgilitySDK.cmake)
include(FetchWinPix.cmake)

# dx headers
#add_subdirectory(DirectX-Headers)

#set_property(TARGET DirectX-Headers PROPERTY FOLDER "3rd-party/dx12")

# vma
set(D3D12MA_BUILD_SAMPLE OFF)
set(D3D12MA_OPTIONS16_SUPPORTED ON CACHE BOOL "enable ReBAR functionality" FORCE)
set(D3D12MA_AGILITY_SDK_DIRECTORY ${AGILITY_SDK_DIR} CACHE STRING "enable agility SDK" FORCE)

add_subdirectory(D3D12MemoryAllocator EXCLUDE_FROM_ALL )
#target_include_directories(D3D12MemoryAllocator PUBLIC "D3D12MemoryAllocator/include")

set_property(TARGET D3D12MemoryAllocator PROPERTY FOLDER "3rd-party/dx12")
# dozen from Mesa project
#add_subdirectory(Mesa)
