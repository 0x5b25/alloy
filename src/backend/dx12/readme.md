
# Pipeline stages:

## Graphics Pipelines:
  1. TOP_OF_PIPE
  1. CONDITIONAL_RENDERING(EXT)
  1. DRAW_INDIRECT
  1. VERTEX_INPUT
  1. VERTEX_SHADER
  1. TESSELLATION_CONTROL_SHADER
  1. TESSELLATION_EVALUATION_SHADER
  1. GEOMETRY_SHADER
  1. TRANSFORM_FEEDBACK(EXT)
  1. FRAGMENT_SHADING_RATE(KHR)
  1. EARLY_FRAGMENT_TESTS
  1. FRAGMENT_SHADER
  1. LATE_FRAGMENT_TESTS
  1. COLOR_ATTACHMENT_OUTPUT
  1. BOTTOM_OF_PIPE

## Compute Pipelines:
  1. TOP_OF_PIPE
  1. CONDITIONAL_RENDERING(EXT)
  1. DRAW_INDIRECT
  1. COMPUTE_SHADER
  1. BOTTOM_OF_PIPE

## Ray Tracing Pipelines:
  1. TOP_OF_PIPE
  1. DRAW_INDIRECT
  1. RAY_TRACING_SHADER(KHR)
  1. BOTTOM_OF_PIPE

## Transfer Commands:
  1. TOP_OF_PIPE
  1. TRANSFER
  1. BOTTOM_OF_PIPE

## Aggregate Stages:
  - ALL_GRAPHICS
  - ALL_COMMANDS

## Ray-Tracing Acceleration Structure Build Commands:
  1. TOP_OF_PIPE
  1. ACCELERATION_STRUCTURE_BUILD(KHR)
  1. BOTTOM_OF_PIPE


Graphics Pipeline stage Concepts:
| alloy  | Vulkan |  DX12  |  Metal |
|--------|--------|--------|--------|
|        | TOP_OF_PIPE                    |                              |
|        | CONDITIONAL_RENDERING(EXT)     | PREDICATION                  |
|        | DRAW_INDIRECT                  |                              |
|        | VERTEX_INPUT                   | INPUT_ASSEMBLER              | vertex
|        | VERTEX_SHADER                  | VERTEX_SHADING               |
|        | TESSELLATION_CONTROL_SHADER    |                              |
|        | TESSELLATION_EVALUATION_SHADER |                              |
|        | GEOMETRY_SHADER                |                              |
|        | TRANSFORM_FEEDBACK(EXT)        |                              |
|        | FRAGMENT_SHADING_RATE(KHR)     |                              |
|        | EARLY_FRAGMENT_TESTS           |                              |
|        | FRAGMENT_SHADER                | PIXEL_SHADING                | fragment
|        | LATE_FRAGMENT_TESTS            |                              |
|        | COLOR_ATTACHMENT_OUTPUT        | DEPTH_STENCIL, RENDER_TARGET |
|        | BOTTOM_OF_PIPE                 |



                                          
# Legacy DX12 resource states to vulkan states mapping

| VkAccessFlagBits              |                       D3D12_RESOURCE_STATES |
|------------------------------ | ------------------------------------------- |
| INDIRECT_COMMAND_READ_BIT                     | INDIRECT_ARGUMENT
| INDEX_READ_BIT                                | INDEX_BUFFER
| VERTEX_ATTRIBUTE_READ_BIT                     | VERTEX_AND_CONSTANT_BUFFER
| UNIFORM_READ_BIT                              |  - VERTEX_AND_CONSTANT_BUFFER
| INPUT_ATTACHMENT_READ_BIT                     |
| SHADER_READ_BIT                               |  - VERTEX_AND_CONSTANT_BUFFER
| SHADER_WRITE_BIT                              | UNORDERED_ACCESS
| COLOR_ATTACHMENT_READ_BIT                     | RENDER_TARGET
| COLOR_ATTACHMENT_WRITE_BIT                    |  - RENDER_TARGET
| DEPTH_STENCIL_ATTACHMENT_READ_BIT             | DEPTH_READ
| DEPTH_STENCIL_ATTACHMENT_WRITE_BIT            | DEPTH_WRITE
| TRANSFER_READ_BIT                             | COPY_SOURCE
| TRANSFER_WRITE_BIT                            | COPY_DEST
| HOST_READ_BIT                                 | COMMON
| HOST_WRITE_BIT                                |  - COMMON
| MEMORY_READ_BIT                               |  - COMMON
| MEMORY_WRITE_BIT                              |  - COMMON
| NONE                                          |
| TRANSFORM_FEEDBACK_WRITE_BIT_EXT              | STREAM_OUT
| TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT       |
| TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT      |
| CONDITIONAL_RENDERING_READ_BIT_EXT            |
| COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT     |
| ACCELERATION_STRUCTURE_READ_BIT_KHR           | RAYTRACING_ACCELERATION_STRUCTURE
| ACCELERATION_STRUCTURE_WRITE_BIT_KHR          |
| FRAGMENT_DENSITY_MAP_READ_BIT_EXT             |
| FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR | SHADING_RATE_SOURCE
|                                               | PRESENT
|                                               | RESOLVE_DEST
|                                               | RESOLVE_SOURCE

# DX12 enhanced barriers

enum D3D12_BARRIER_SYNC {
    D3D12_BARRIER_SYNC_NONE,
    D3D12_BARRIER_SYNC_ALL,
    D3D12_BARRIER_SYNC_DRAW,
    D3D12_BARRIER_SYNC_INPUT_ASSEMBLER,
    D3D12_BARRIER_SYNC_VERTEX_SHADING,
    D3D12_BARRIER_SYNC_PIXEL_SHADING,
    D3D12_BARRIER_SYNC_DEPTH_STENCIL,
    D3D12_BARRIER_SYNC_RENDER_TARGET,
    D3D12_BARRIER_SYNC_COMPUTE_SHADING,
    D3D12_BARRIER_SYNC_RAYTRACING,
    D3D12_BARRIER_SYNC_COPY,
    D3D12_BARRIER_SYNC_RESOLVE,
    D3D12_BARRIER_SYNC_EXECUTE_INDIRECT,
    D3D12_BARRIER_SYNC_PREDICATION,
    D3D12_BARRIER_SYNC_ALL_SHADING,
    D3D12_BARRIER_SYNC_NON_PIXEL_SHADING,
    D3D12_BARRIER_SYNC_EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO,
    D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
    D3D12_BARRIER_SYNC_COPY_RAYTRACING_ACCELERATION_STRUCTURE,
    D3D12_BARRIER_SYNC_SPLIT,
};

enum D3D12_BARRIER_ACCESS {
    D3D12_BARRIER_ACCESS_COMMON,
    D3D12_BARRIER_ACCESS_VERTEX_BUFFER,
    D3D12_BARRIER_ACCESS_CONSTANT_BUFFER,
    D3D12_BARRIER_ACCESS_INDEX_BUFFER,
    D3D12_BARRIER_ACCESS_RENDER_TARGET,
    D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
    D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
    D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ,
    D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
    D3D12_BARRIER_ACCESS_STREAM_OUTPUT,
    D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT,
    D3D12_BARRIER_ACCESS_PREDICATION,
    D3D12_BARRIER_ACCESS_COPY_DEST,
    D3D12_BARRIER_ACCESS_COPY_SOURCE,
    D3D12_BARRIER_ACCESS_RESOLVE_DEST,
    D3D12_BARRIER_ACCESS_RESOLVE_SOURCE,
    D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
    D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE,
    D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE,
    D3D12_BARRIER_ACCESS_NO_ACCESS
};

Metal pipeline stages
|        |        |        | object |
|        |        |        | mesh   |
|        |        |        | vertex |
|        |        |        | fragment |
|        |        |        | tile    |
