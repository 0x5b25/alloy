#include "DXCResourceBarrier.hpp"

#include "DXCDevice.hpp"
#include "DXCTexture.hpp"

namespace alloy::dxc {

    
    D3D12_RESOURCE_STATES AlToD3DResourceState (ResourceAccess access) {

        if(access == ResourceAccess::None) {
            return D3D12_RESOURCE_STATE_COMMON;
        }

        //if(sync & D3D12_BARRIER_SYNC_RESOLVE) { 
            if(access == ResourceAccess::RESOLVE_SOURCE)
                return D3D12_RESOURCE_STATE_RESOLVE_SOURCE; 
            if(access == ResourceAccess::RESOLVE_DEST)
                return D3D12_RESOURCE_STATE_RESOLVE_DEST; 
        //}

        //if(sync & D3D12_BARRIER_SYNC_COPY) { 
            if(access == ResourceAccess::COPY_SOURCE)
                return D3D12_RESOURCE_STATE_COPY_SOURCE; 
            if(access == ResourceAccess::COPY_DEST)
                return D3D12_RESOURCE_STATE_COPY_DEST; 
        //}

        //if(sync & D3D12_BARRIER_SYNC_RAYTRACING) {
            if(access == ResourceAccess::RAYTRACING_ACCELERATION_STRUCTURE_READ ||
               access == ResourceAccess::RAYTRACING_ACCELERATION_STRUCTURE_WRITE)
                return D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        //}

        //if(sync & D3D12_BARRIER_SYNC_EXECUTE_INDIRECT) {
        if(access == ResourceAccess::INDIRECT_ARGUMENT)
            return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        //}

        //if(sync & D3D12_BARRIER_SYNC_RENDER_TARGET) {
        if(access == ResourceAccess::RENDER_TARGET)
            return D3D12_RESOURCE_STATE_RENDER_TARGET;
        //}

        //if(sync & D3D12_BARRIER_SYNC_DEPTH_STENCIL) {
            if(access == ResourceAccess::DepthStencilWritable)
                return D3D12_RESOURCE_STATE_DEPTH_WRITE;   
            if(access == ResourceAccess::DepthStencilReadOnly)
                return D3D12_RESOURCE_STATE_DEPTH_READ;
        //}

        //Infer from access flags
        if(access == ResourceAccess::UNORDERED_ACCESS) {
            return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }

        if(access == ResourceAccess::SHADER_RESOURCE) {
            return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
            //if(sync & D3D12_BARRIER_SYNC_PIXEL_SHADING) {
                | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            //}
        }

        if(access == ResourceAccess::INDEX_BUFFER) {
            return D3D12_RESOURCE_STATE_INDEX_BUFFER;
        }

        if(access == ResourceAccess::VERTEX_BUFFER ||
           access == ResourceAccess::CONSTANT_BUFFER) {
            return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        }

        if(access == ResourceAccess::STREAM_OUTPUT) {
            return D3D12_RESOURCE_STATE_STREAM_OUT;
        }

        return {};
    }

    D3D12_BARRIER_LAYOUT AlToD3DTexLayoutEnhanced (const alloy::TextureLayout& layout) {
        switch(layout) {
            case alloy::TextureLayout::UNDEFINED: return D3D12_BARRIER_LAYOUT_UNDEFINED;
            case alloy::TextureLayout::COMMON_READ: return D3D12_BARRIER_LAYOUT_GENERIC_READ;
            case alloy::TextureLayout::RENDER_TARGET: return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
            case alloy::TextureLayout::UNORDERED_ACCESS: return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
            case alloy::TextureLayout::DEPTH_STENCIL_WRITE: return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
            case alloy::TextureLayout::DEPTH_STENCIL_READ: return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
            case alloy::TextureLayout::SHADER_RESOURCE: return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
            case alloy::TextureLayout::COPY_SOURCE: return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
            case alloy::TextureLayout::COPY_DEST: return D3D12_BARRIER_LAYOUT_COPY_DEST;
            case alloy::TextureLayout::RESOLVE_SOURCE: return D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;
            case alloy::TextureLayout::RESOLVE_DEST: return D3D12_BARRIER_LAYOUT_RESOLVE_DEST;
            case alloy::TextureLayout::SHADING_RATE_SOURCE: return D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE;

            default: return D3D12_BARRIER_LAYOUT_COMMON;
        }
    }

    D3D12_BARRIER_SYNC AlToD3DPipelineStageEnhanced(PipelineStage stage) {

        if(stage == alloy::PipelineStage::All)
            return D3D12_BARRIER_SYNC_ALL;

        //if(stage == alloy::PipelineStage::TopOfPipe){
        //    if(isStagesBefore)
        //        return D3D12_BARRIER_SYNC_NONE;
        //    else
        //        return D3D12_BARRIER_SYNC_ALL;
        //}
        //if(stage == alloy::PipelineStage::BottomOfPipe) {
        //    if(isStagesBefore)
        //        return D3D12_BARRIER_SYNC_ALL;
        //    else
        //        return D3D12_BARRIER_SYNC_NONE;
        //}

        if(stage == alloy::PipelineStage::Draw)
            return D3D12_BARRIER_SYNC_DRAW;
        if(stage == alloy::PipelineStage::NonPixelShading)
            return D3D12_BARRIER_SYNC_NON_PIXEL_SHADING;
        if(stage == alloy::PipelineStage::AllShading)
            return D3D12_BARRIER_SYNC_ALL_SHADING;


        if(stage == alloy::PipelineStage::INPUT_ASSEMBLER)
            return D3D12_BARRIER_SYNC_INDEX_INPUT;
        if(stage == alloy::PipelineStage::VERTEX_SHADING)
            return D3D12_BARRIER_SYNC_VERTEX_SHADING;
        if(stage == alloy::PipelineStage::PIXEL_SHADING)
            return D3D12_BARRIER_SYNC_PIXEL_SHADING;
        if(stage == alloy::PipelineStage::DEPTH_STENCIL)
            return D3D12_BARRIER_SYNC_DEPTH_STENCIL;
        if(stage == alloy::PipelineStage::RENDER_TARGET)
            return D3D12_BARRIER_SYNC_RENDER_TARGET;
        if(stage == alloy::PipelineStage::COMPUTE_SHADING)
            return D3D12_BARRIER_SYNC_COMPUTE_SHADING;
        if(stage == alloy::PipelineStage::RAYTRACING)
            return D3D12_BARRIER_SYNC_RAYTRACING;
        if(stage == alloy::PipelineStage::COPY)
            return D3D12_BARRIER_SYNC_COPY;
        if(stage == alloy::PipelineStage::RESOLVE)
            return D3D12_BARRIER_SYNC_RESOLVE;
        if(stage == alloy::PipelineStage::EXECUTE_INDIRECT)
            return D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
        //if(stage == alloy::PipelineStage::EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO){  }
        //if(stage == alloy::PipelineStage::BUILD_RAYTRACING_ACCELERATION_STRUCTURE){  }
        //if(stage == alloy::PipelineStage::COPY_RAYTRACING_ACCELERATION_STRUCTURE){  }
        return D3D12_BARRIER_SYNC_NONE;
    }

    D3D12_BARRIER_ACCESS AlToD3DResourceAccessEnhanced(ResourceAccess access) {
        if(access == ResourceAccess::None)
            return D3D12_BARRIER_ACCESS_NO_ACCESS;
        
        if(access == alloy::ResourceAccess::COMMON)
           return D3D12_BARRIER_ACCESS_COMMON;
        if(access == alloy::ResourceAccess::VERTEX_BUFFER)
           return D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
        if(access == alloy::ResourceAccess::CONSTANT_BUFFER)
           return D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
        if(access == alloy::ResourceAccess::INDEX_BUFFER)
           return D3D12_BARRIER_ACCESS_INDEX_BUFFER;
        if(access == alloy::ResourceAccess::RENDER_TARGET)
           return D3D12_BARRIER_ACCESS_RENDER_TARGET;
        if(access == alloy::ResourceAccess::UNORDERED_ACCESS)
           return D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
        if(access == alloy::ResourceAccess::DepthStencilWritable)
           return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
        if(access == alloy::ResourceAccess::DepthStencilReadOnly)
           return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
        if(access == alloy::ResourceAccess::SHADER_RESOURCE)
           return D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
        if(access == alloy::ResourceAccess::STREAM_OUTPUT)
           return D3D12_BARRIER_ACCESS_STREAM_OUTPUT;
        if(access == alloy::ResourceAccess::INDIRECT_ARGUMENT)
           return D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
        if(access == alloy::ResourceAccess::PREDICATION)
           return D3D12_BARRIER_ACCESS_PREDICATION;
        if(access == alloy::ResourceAccess::COPY_DEST)
           return D3D12_BARRIER_ACCESS_COPY_DEST;
        if(access == alloy::ResourceAccess::COPY_SOURCE)
           return D3D12_BARRIER_ACCESS_COPY_SOURCE;
        if(access == alloy::ResourceAccess::RESOLVE_DEST)
           return D3D12_BARRIER_ACCESS_RESOLVE_DEST;
        if(access == alloy::ResourceAccess::RESOLVE_SOURCE)
           return D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
        if(access == alloy::ResourceAccess::RAYTRACING_ACCELERATION_STRUCTURE_READ)
           return D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
        if(access == alloy::ResourceAccess::RAYTRACING_ACCELERATION_STRUCTURE_WRITE)
           return D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;
        if(access == alloy::ResourceAccess::SHADING_RATE_SOURCE)
           return D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE;
        //if(stage == alloy::PipelineStage::EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO){  }
        //if(stage == alloy::PipelineStage::BUILD_RAYTRACING_ACCELERATION_STRUCTURE){  }
        //if(stage == alloy::PipelineStage::COPY_RAYTRACING_ACCELERATION_STRUCTURE){  }
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
