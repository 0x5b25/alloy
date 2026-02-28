//Convert DX12 style indirect arguments to metal style
#include <metal_stdlib>
using namespace metal;

struct ICBContainer
{
    command_buffer commandBuffer;
};

enum IndirectArgumentType {
    Draw,
    DrawIndexed,
    Dispatch,
    BindVertexBuffer,
    BindIndexBuffer,
    SetPushConstant,
    BindCBV,
    BindSRV,
    BindUAV,
    DispatchRays, //Not implemented
    DispatchMesh, //Not implemented
};

union IndirectArgumentData {
    //D3D12_DRAW_ARGUMENTS
    struct IndirectCommandArgDraw {
        uint vertexCountPerInstance;
        uint instanceCount;
        uint startVertexLocation;
        uint startInstanceLocation;
    } draw;

    //D3D12_DRAW_INDEXED_ARGUMENTS
    struct IndirectCommandArgDrawIndexed {
        uint indexCountPerInstance;
        uint instanceCount;
        uint startIndexLocation;
        int baseVertexLocation;
        uint startInstanceLocation;
    } drawIndexed;

    //D3D12_DISPATCH_ARGUMENTS
    struct IndirectCommandArgDispatch {
        uint threadGroupCountX;
        uint threadGroupCountY;
        uint threadGroupCountZ;
    } dispatch;

    //D3D12_DISPATCH_MESH_ARGUMENTS
    struct IndirectCommandArgDispatchMesh {
        uint threadGroupCountX;
        uint threadGroupCountY;
        uint threadGroupCountZ;
    } dispatchMesh;

    //D3D12_DISPATCH_RAYS_DESC
    struct IndirectCommandArgDispatchRays {
        ulong RayGenerationShaderRecord;

        //D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE
        struct GPUAddressRangeAndStride {
            ulong startAddress;
            ulong sizeInBytes;
            ulong strideInBytes;
        };

        GPUAddressRangeAndStride missShaderTable;
        GPUAddressRangeAndStride hitGroupTable;
        GPUAddressRangeAndStride callableShaderTable;
        uint width;
        uint height;
        uint depth;
    } dispatchRays;

    //D3D12_INDEX_BUFFER_VIEW
    struct IndirectCommandArgBindIndexBuffer {
        ulong bufferLocation;
        uint sizeInBytes;
        //Using values from DXGI_FORMAT
        uint format;
    } bindIndexBuffer;

    //D3D12_VERTEX_BUFFER_VIEW
    struct IndirectCommandArgBindVertexBuffer {
        ulong bufferLocation;
        uint sizeInBytes;
        uint strideInBytes;
    } bindVertexBuffer;
};

struct IndirectArgumentIn {
    IndirectArgumentType type;
    IndirectArgumentData data;
};

kernel void
ExecuteIndirectArgGen(uint                         threadIndex   [[ thread_position_in_grid ]],
                      device uint*                 maxArgCount   [[ buffer(0) ]],
                      device IndirectArgumentIn*   argIn         [[ buffer(1) ]],
                      device ICBContainer         *icb_container [[ buffer(2) ]])
{
    if(threadIndex >= *maxArgCount) return;
}