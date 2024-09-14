

cmake_minimum_required(VERSION 3.13)

project(dxc_third_party LANGUAGES C CXX)

# dx headers
add_subdirectory(DirectX-Headers)

# vma
option(D3D12MA_BUILD_SAMPLE OFF)
add_subdirectory(D3D12MemoryAllocator)
target_include_directories(D3D12MemoryAllocator PUBLIC "D3D12MemoryAllocator/include")

# dozen from Mesa project
#add_subdirectory(Mesa)
