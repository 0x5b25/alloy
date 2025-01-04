

cmake_minimum_required(VERSION 3.13)

project(dxc_third_party LANGUAGES C CXX)

# dx headers
add_subdirectory(DirectX-Headers)

set_property(TARGET DirectX-Headers PROPERTY FOLDER "3rd-party/dx12")

# vma
option(D3D12MA_BUILD_SAMPLE OFF)
add_subdirectory(D3D12MemoryAllocator)
target_include_directories(D3D12MemoryAllocator PUBLIC "D3D12MemoryAllocator/include")

set_property(TARGET D3D12MemoryAllocator PROPERTY FOLDER "3rd-party/dx12")
# dozen from Mesa project
#add_subdirectory(Mesa)
