#pragma once

#include "alloy/ResourceBarrier.hpp"

#include <dxgi1_4.h> //Guaranteed by DX12
#include <d3d12.h>
#include <span>

namespace alloy::dxc
{
    class DXCCommandList;
    class DXCCommandList7;
    class DXCDevice;


    D3D12_RESOURCE_STATES AlToD3DResourceState (ResourceAccess access);

    D3D12_BARRIER_LAYOUT AlToD3DTexLayoutEnhanced (const alloy::TextureLayout& layout);
    D3D12_BARRIER_SYNC AlToD3DPipelineStageEnhanced(PipelineStage stage);
    D3D12_BARRIER_ACCESS AlToD3DResourceAccessEnhanced(ResourceAccess access);

    void InsertBarrier(
        DXCDevice* dev,
        ID3D12GraphicsCommandList* cmdBuf,
        const alloy::utils::BarrierActions& barriers);
    
    void InsertBarrierEnhanced(
        DXCDevice* dev,
        ID3D12GraphicsCommandList7* cmdBuf,
        const alloy::utils::BarrierActions& barriers);

    void BindBarrier(
        DXCCommandList* cmdBuf,
        std::span<const alloy::BarrierOp> barriers);

        
    void BindBarrierEnhanced(
        DXCCommandList7* cmdBuf,
        std::span<const alloy::BarrierOp> barriers);

} // namespace alloy

