#include "DXCResourceBarrier.hpp"

#include "DXCDevice.hpp"
#include "DXCTexture.hpp"

namespace alloy::dxc {

    
    D3D12_RESOURCE_STATES AlToD3DResourceState (ResourceAccess access) {

        if(access == ResourceAccess::None) {
            return D3D12_RESOURCE_STATE_COMMON;
        }

        if(access == ResourceAccess::CopySource)
            return D3D12_RESOURCE_STATE_COPY_SOURCE;
        if(access == ResourceAccess::CopyDest)
            return D3D12_RESOURCE_STATE_COPY_DEST;

        if(access == ResourceAccess::AccelerationStructureRead ||
           access == ResourceAccess::AccelerationStructureWrite)
            return D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

        if(access == ResourceAccess::IndirectArgumentRead)
            return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

        if(access == ResourceAccess::RenderTarget)
            return D3D12_RESOURCE_STATE_RENDER_TARGET;

        if(access == ResourceAccess::DepthStencilWrite)
            return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        if(access == ResourceAccess::DepthStencilRead)
            return D3D12_RESOURCE_STATE_DEPTH_READ;

        if(access == ResourceAccess::UnorderedAccess) {
            return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }

        if(access == ResourceAccess::ShaderResourceRead) {
            return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
                | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }

        if(access == ResourceAccess::IndexBufferRead) {
            return D3D12_RESOURCE_STATE_INDEX_BUFFER;
        }

        if(access == ResourceAccess::VertexBufferRead ||
           access == ResourceAccess::ConstantBufferRead) {
            return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        }

        if(access == ResourceAccess::Present) {
            return D3D12_RESOURCE_STATE_COMMON;
        }

        return {};
    }

    D3D12_BARRIER_LAYOUT AlToD3DTexLayoutEnhanced (const alloy::TextureLayout& layout) {
        switch(layout) {
            case alloy::TextureLayout::Undefined: return D3D12_BARRIER_LAYOUT_UNDEFINED;
            case alloy::TextureLayout::General: return D3D12_BARRIER_LAYOUT_COMMON;
            case alloy::TextureLayout::ShaderReadOnly: return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
            case alloy::TextureLayout::Storage: return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
            case alloy::TextureLayout::ColorAttachment: return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
            case alloy::TextureLayout::DepthStencilReadOnly: return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
            case alloy::TextureLayout::DepthStencilWrite: return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
            case alloy::TextureLayout::CopySource: return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
            case alloy::TextureLayout::CopyDest: return D3D12_BARRIER_LAYOUT_COPY_DEST;
            case alloy::TextureLayout::ResolveSource: return D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;
            case alloy::TextureLayout::ResolveDest: return D3D12_BARRIER_LAYOUT_RESOLVE_DEST;
            case alloy::TextureLayout::Present: return D3D12_BARRIER_LAYOUT_PRESENT;

            default: return D3D12_BARRIER_LAYOUT_COMMON;
        }
    }

    D3D12_BARRIER_SYNC AlToD3DPipelineStageEnhanced(PipelineStage stage) {

        if(stage == alloy::PipelineStage::AllCommands)
            return D3D12_BARRIER_SYNC_ALL;

        if(stage == alloy::PipelineStage::AllGraphics)
            return D3D12_BARRIER_SYNC_DRAW;
        if(stage == alloy::PipelineStage::AllShaders)
            return D3D12_BARRIER_SYNC_ALL_SHADING;

        if(stage == alloy::PipelineStage::DrawIndirect)
            return D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
        if(stage == alloy::PipelineStage::VertexInput)
            return D3D12_BARRIER_SYNC_INDEX_INPUT;
        if(stage == alloy::PipelineStage::VertexShader)
            return D3D12_BARRIER_SYNC_VERTEX_SHADING;
        if(stage == alloy::PipelineStage::MeshShader)
            return D3D12_BARRIER_SYNC_NON_PIXEL_SHADING;
        if(stage == alloy::PipelineStage::FragmentShader)
            return D3D12_BARRIER_SYNC_PIXEL_SHADING;
        if(stage == alloy::PipelineStage::DepthStencil)
            return D3D12_BARRIER_SYNC_DEPTH_STENCIL;
        if(stage == alloy::PipelineStage::ColorOutput)
            return D3D12_BARRIER_SYNC_RENDER_TARGET;
        if(stage == alloy::PipelineStage::ComputeShader)
            return D3D12_BARRIER_SYNC_COMPUTE_SHADING;
        if(stage == alloy::PipelineStage::RayTracing)
            return D3D12_BARRIER_SYNC_RAYTRACING;
        if(stage == alloy::PipelineStage::Copy)
            return D3D12_BARRIER_SYNC_COPY;
        if(stage == alloy::PipelineStage::BuildAS)
            return D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
        return D3D12_BARRIER_SYNC_NONE;
    }

    D3D12_BARRIER_ACCESS AlToD3DResourceAccessEnhanced(ResourceAccess access) {
        if(access == ResourceAccess::None)
            return D3D12_BARRIER_ACCESS_NO_ACCESS;
        
        if(access == alloy::ResourceAccess::IndirectArgumentRead)
           return D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
        if(access == alloy::ResourceAccess::VertexBufferRead)
           return D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
        if(access == alloy::ResourceAccess::IndexBufferRead)
           return D3D12_BARRIER_ACCESS_INDEX_BUFFER;
        if(access == alloy::ResourceAccess::ConstantBufferRead)
           return D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
        if(access == alloy::ResourceAccess::ShaderResourceRead)
           return D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
        if(access == alloy::ResourceAccess::UnorderedAccess)
           return D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
        if(access == alloy::ResourceAccess::RenderTarget)
           return D3D12_BARRIER_ACCESS_RENDER_TARGET;
        if(access == alloy::ResourceAccess::DepthStencilRead)
           return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
        if(access == alloy::ResourceAccess::DepthStencilWrite)
           return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
        if(access == alloy::ResourceAccess::CopySource)
           return D3D12_BARRIER_ACCESS_COPY_SOURCE;
        if(access == alloy::ResourceAccess::CopyDest)
           return D3D12_BARRIER_ACCESS_COPY_DEST;
        if(access == alloy::ResourceAccess::AccelerationStructureRead)
           return D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
        if(access == alloy::ResourceAccess::AccelerationStructureWrite)
           return D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;
        if(access == alloy::ResourceAccess::Present)
           return D3D12_BARRIER_ACCESS_COMMON;
        return {};
    }

    void InsertBarrier(
        DXCDevice* dev,
        ID3D12GraphicsCommandList* cmdBuf,
        const alloy::utils::BarrierActions& actions
    ) {

        std::vector<D3D12_RESOURCE_BARRIER> barriers{};

        
        for(auto& desc : actions.bufferBarrierActions) {

            auto dxcBuffer = PtrCast<DXCBuffer>(desc.buffer);
            
            auto& barrier = barriers.emplace_back();
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.UAV.pResource = dxcBuffer->GetHandle();
        }

        for(auto& desc : actions.textureBarrierActions) {
        
            auto dxcTex = PtrCast<DXCTexture>(desc.texture);

            
            auto& barrier = barriers.emplace_back();
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = AlToD3DResourceState(desc.before.access);
            barrier.Transition.StateAfter = AlToD3DResourceState(desc.after.access);
            barrier.Transition.pResource = dxcTex->GetHandle();
        }

        if(!barriers.empty())
            cmdBuf->ResourceBarrier(barriers.size(), barriers.data());

    }

        
    void InsertBarrierEnhanced(
        DXCDevice* dev,
        ID3D12GraphicsCommandList7* cmdBuf,
        const alloy::utils::BarrierActions& actions
    ) {
        std::vector<D3D12_BUFFER_BARRIER> bufBarriers;
        std::vector<D3D12_TEXTURE_BARRIER> texBarrier;

        for(auto& desc : actions.bufferBarrierActions) {
            auto& barrier = bufBarriers.emplace_back();
            barrier.SyncAfter = AlToD3DPipelineStageEnhanced(desc.after.stage);
            barrier.SyncBefore = AlToD3DPipelineStageEnhanced(desc.before.stage);
            
            barrier.AccessAfter = AlToD3DResourceAccessEnhanced(desc.after.access);
            barrier.AccessBefore = AlToD3DResourceAccessEnhanced(desc.before.access);
            auto dxcBuffer = PtrCast<DXCBuffer>(desc.buffer);
            barrier.pResource = dxcBuffer->GetHandle();
        }

        
        for(auto& desc : actions.textureBarrierActions) {
            auto& barrier = texBarrier.emplace_back();
            barrier.SyncAfter = AlToD3DPipelineStageEnhanced(desc.after.stage);
            barrier.SyncBefore = AlToD3DPipelineStageEnhanced(desc.before.stage);
            barrier.AccessAfter = AlToD3DResourceAccessEnhanced(desc.after.access);
            barrier.AccessBefore = AlToD3DResourceAccessEnhanced(desc.before.access);
            barrier.LayoutAfter = AlToD3DTexLayoutEnhanced(desc.after.layout);
            barrier.LayoutBefore = AlToD3DTexLayoutEnhanced(desc.before.layout);
            
            auto dxcTex = common::PtrCast<DXCTexture>(desc.texture);
            barrier.pResource = dxcTex->GetHandle();
        }

        std::vector<D3D12_BARRIER_GROUP> barrierGrps;

        if(!bufBarriers.empty()) {
            barrierGrps.emplace_back();
            auto& grp = barrierGrps.back();
            grp.Type = D3D12_BARRIER_TYPE_BUFFER;
            grp.NumBarriers = bufBarriers.size();
            grp.pBufferBarriers = bufBarriers.data();
        }

        if(!texBarrier.empty()) {
            barrierGrps.emplace_back();
            auto& grp = barrierGrps.back();
            grp.Type = D3D12_BARRIER_TYPE_TEXTURE;
            grp.NumBarriers = texBarrier.size();
            grp.pTextureBarriers = texBarrier.data();
        }
        
        cmdBuf->Barrier(barrierGrps.size(), barrierGrps.data());
    }


}
