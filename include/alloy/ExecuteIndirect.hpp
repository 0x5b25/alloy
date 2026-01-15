#pragma once

#include "common/RefCnt.hpp"

#include <vector>

namespace alloy
{
    class IResourceLayout;

    //#TODO: Use DX12 implementations for now, may use a custom
    //  compute shader to convert alloy types to backend API specific
    //  types 

    //DX12 reference : https://learn.microsoft.com/en-us/windows/win32/direct3d12/indirect-drawing

    //Structs used when writing indirect command buffers

    //D3D12_DRAW_ARGUMENTS
    struct IndirectCommandArgDraw {
        uint32_t vertexCountPerInstance;
        uint32_t instanceCount;
        uint32_t startVertexLocation;
        uint32_t startInstanceLocation;
    };

    //D3D12_DRAW_INDEXED_ARGUMENTS
    struct IndirectCommandArgDrawIndexed {
        uint32_t indexCountPerInstance;
        uint32_t instanceCount;
        uint32_t startIndexLocation;
        int32_t baseVertexLocation;
        uint32_t startInstanceLocation;
    };

    //D3D12_DISPATCH_ARGUMENTS
    struct IndirectCommandArgDispatch {
        uint32_t threadGroupCountX;
        uint32_t threadGroupCountY;
        uint32_t threadGroupCountZ;
    };

    //D3D12_DISPATCH_MESH_ARGUMENTS
    struct IndirectCommandArgDispatchMesh {
        uint32_t threadGroupCountX;
        uint32_t threadGroupCountY;
        uint32_t threadGroupCountZ;
    };

    //D3D12_DISPATCH_RAYS_DESC
    struct IndirectCommandArgDispatchRays {
        uint64_t RayGenerationShaderRecord;

        //D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE
        struct GPUAddressRangeAndStride {
            uint64_t startAddress;
            uint64_t sizeInBytes;
            uint64_t strideInBytes;
        };

        GPUAddressRangeAndStride missShaderTable;
        GPUAddressRangeAndStride hitGroupTable;
        GPUAddressRangeAndStride callableShaderTable;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
    };

    //D3D12_INDEX_BUFFER_VIEW
    struct IndirectCommandArgBindIndexBuffer {
        uint64_t bufferLocation;
        uint32_t sizeInBytes;
        //Using values from DXGI_FORMAT
        uint32_t format;
    };

    //D3D12_VERTEX_BUFFER_VIEW
    struct IndirectCommandArgBindVertexBuffer {
        uint64_t bufferLocation;
        uint32_t sizeInBytes;
        uint32_t strideInBytes;
    };

    class IIndirectCommandLayout : public common::RefCntBase {

    public:

        enum class OpType {
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

        struct Operation {
            OpType type;

            union {
                struct {
                    std::uint32_t slot;
                } bindVertexBuffer;

                struct {
                    std::uint32_t pushConstantIndex;
                    std::uint32_t destOffsetIn32BitValues;
                    std::uint32_t num32BitValuesToSet;
                } setPushConstant;

                struct {
                    std::uint32_t layoutIndex;
                } bindCBV, BindSRV, BindUAV;
            };
        };

        struct Description {

            uint32_t argumentStrideInBytes; // Stride in bytes to fetch arguments for next command 
            std::vector<Operation> operations;

            common::sp<IResourceLayout> layout; //optional if no operation will alter resource sets
        };


    public:
        virtual ~IIndirectCommandLayout() {}

    };


} // namespace alloy

