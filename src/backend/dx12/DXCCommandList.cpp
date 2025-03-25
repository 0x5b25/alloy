#include "DXCCommandList.hpp"


#include "alloy/common/Common.hpp"
#include "alloy/Helpers.hpp"

#include "DXCDevice.hpp"
#include "D3DTypeCvt.hpp"
#include "D3DCommon.hpp"
#include "DXCTexture.hpp"
#include "DXCPipeline.hpp"
#include "DXCFrameBuffer.hpp"
#include "DXCBindableResource.hpp"

namespace alloy::dxc
{

    common::sp<DXCCommandList> DXCCommandList::Make( const common::sp<DXCDevice>& dev,
                                                     D3D12_COMMAND_LIST_TYPE type 
    ) {

        auto pDev = dev->GetDevice();

        //D3D12_COMMAND_LIST_TYPE cmdListType = D3D12_COMMAND_LIST_TYPE_DIRECT;

        ID3D12CommandAllocator* pAllocator;
        ThrowIfFailed(pDev->CreateCommandAllocator(type, IID_PPV_ARGS(&pAllocator)));

        
        ID3D12GraphicsCommandList* pCmdList;
        // Create the command list.
        ThrowIfFailed(pDev->CreateCommandList(0, type, pAllocator, nullptr, IID_PPV_ARGS(&pCmdList)));
        ThrowIfFailed(pCmdList->Close());

        

        if(dev->GetDevCaps().SupportEnhancedBarrier()) {
            ID3D12GraphicsCommandList7* pNewCmdList;
            pCmdList->QueryInterface(IID_PPV_ARGS(&pNewCmdList));
            pCmdList->Release();
            return common::sp(new DXCCommandList7(dev, pAllocator, pNewCmdList));
        }
        else if(dev->GetDevCaps().SupportMeshShader()) {
            ID3D12GraphicsCommandList6* pNewCmdList;
            pCmdList->QueryInterface(IID_PPV_ARGS(&pNewCmdList));
            pCmdList->Release();
            return common::sp(new DXCCommandList6(dev, pAllocator, pNewCmdList));
        }

        return common::sp(new DXCCommandList(dev, pAllocator, pCmdList));

    }

    #define CHK_RENDERPASS_BEGUN() DEBUGCODE(assert(_currentPass != nullptr))
    #define CHK_RENDERPASS_ENDED() DEBUGCODE(assert(_currentPass == nullptr))
    #define CHK_PIPELINE_SET() DEBUGCODE(assert(_currentPipeline != nullptr))

    
    ID3D12GraphicsCommandList* DXCCmdEncBase::GetCmdList() const {
        return static_cast<ID3D12GraphicsCommandList*>(cmdList->GetHandle());
    }

    void DXCCmdEncBase::RegisterResourceUsage(
        const std::span<ID3D12Resource* const>& resources, 
        D3D12_RESOURCE_STATES state
    ) {
        cmdList->RegisterResourceUsage(resources, state);
    }

    void DXCCmdEncBase::RegisterResourceSet(DXCResourceSet* d3drs) {
        auto dxcLayout = static_cast<DXCResourceLayout*>(d3drs->GetDesc().layout.get());
        auto& boundResources = d3drs->GetDesc().boundResources;
        auto& elemDescs = dxcLayout->GetDesc().shaderResources;
        auto& elems = d3drs->GetDesc().boundResources;

        std::vector<ID3D12Resource*> resReadOnly, resRW;

        for(unsigned i = 0; i < elemDescs.size(); i++) {
            
            auto& elemDesc = elemDescs[i];
            auto& elem = elems[i];

            using _ResKind = IBindableResource::ResourceKind;

            switch (elemDesc.kind) {
                case _ResKind::Texture: {
                    auto* texView = PtrCast<DXCTextureView>(elem.get());
                    auto texture = PtrCast<DXCTexture>(texView->GetTextureObject().get());
                    if(elemDesc.options.writable) {
                        resRW.push_back(texture->GetHandle());
                    } else {
                        resReadOnly.push_back(texture->GetHandle());
                    }
                } break;
                case _ResKind::UniformBuffer: {
                    auto* range = PtrCast<BufferRange>(elem.get());
                    auto buffer = PtrCast<DXCBuffer>(range->GetBufferObject());
                    resReadOnly.push_back(buffer->GetHandle());
                } break;
                case _ResKind::StorageBuffer: {
                    auto* range = PtrCast<BufferRange>(elem.get());
                    auto buffer = PtrCast<DXCBuffer>(range->GetBufferObject());
                    resRW.push_back(buffer->GetHandle());
                    
                } break;
                default:
                    // Samplers don't need to be registered for usage
                    break;
            }
        }

        if(!resReadOnly.empty()) {
            RegisterResourceUsage(resReadOnly, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
        }

        if(!resRW.empty()) {
            RegisterResourceUsage(resRW, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
    }

    void DXCRenderCmdEnc::SetPipeline(const common::sp<IGfxPipeline>& pipeline){

        auto dxcPipeline = PtrCast<DXCGraphicsPipeline>(pipeline.get());
        assert(dxcPipeline != _currentPipeline);
        resources.insert(pipeline);

        dxcPipeline->CmdBindPipeline(GetCmdList());

        //ensure resource set counts
        //auto setCnt = vkPipeline->GetResourceSetCount();
        //if (setCnt > _resourceSets.size()) {
        //    _resourceSets.resize(setCnt, {});
        //}

        //Mark current pipeline
        _currentPipeline = dxcPipeline;
    }
    
    void DXCRenderCmdEnc::SetVertexBuffer(
        std::uint32_t index, const common::sp<BufferRange>& buffer
    ){
        //_resReg.RegisterBufferUsage(buffer,
        //    VkPipelineStageFlagBits::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        //    VkAccessFlagBits::VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
        //);
        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        
        CHK_PIPELINE_SET();
        
        resources.insert(buffer);

        auto gfxPipeline = PtrCast<DXCGraphicsPipeline>(_currentPipeline);
        auto& pipeDesc = gfxPipeline->GetDesc();

        auto dxcBuffer = PtrCast<DXCBuffer>(buffer->GetBufferObject());
        auto pRes = dxcBuffer->GetHandle();

        ID3D12Resource* resList[] = {pRes};

        RegisterResourceUsage(resList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        D3D12_VERTEX_BUFFER_VIEW view {};
        view.BufferLocation = pRes->GetGPUVirtualAddress() + buffer->GetShape().GetOffsetInBytes();
        view.StrideInBytes = pipeDesc.shaderSet.vertexLayouts[index].stride;
        view.SizeInBytes = buffer->GetShape().GetSizeInBytes();

        GetCmdList()->IASetVertexBuffers(index, 1, &view);
    }

    void DXCRenderCmdEnc::SetIndexBuffer(
        const common::sp<BufferRange>& buffer, IndexFormat format
    ){
        //_resReg.RegisterBufferUsage(buffer,
        //    VkPipelineStageFlagBits::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        //    VkAccessFlagBits::VK_ACCESS_INDEX_READ_BIT
        //);
        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        //VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(buffer.get());
        //vkCmdBindIndexBuffer(_cmdBuf, vkBuffer->GetHandle(), offset, alloy::VK::priv::VdToVkIndexFormat(format));
    
        resources.insert(buffer);
        
        auto dxcBuffer = PtrCast<DXCBuffer>(buffer->GetBufferObject());
        auto pRes = dxcBuffer->GetHandle();

        ID3D12Resource* resList[] = {pRes};

        RegisterResourceUsage(resList, D3D12_RESOURCE_STATE_INDEX_BUFFER);

        D3D12_INDEX_BUFFER_VIEW view{};
        view.Format = VdToD3DIndexFormat(format);
        view.BufferLocation = pRes->GetGPUVirtualAddress() + buffer->GetShape().GetOffsetInBytes();
        view.SizeInBytes = buffer->GetShape().GetSizeInBytes();

        GetCmdList()->IASetIndexBuffer(&view);
    }

    
    void DXCRenderCmdEnc::SetGraphicsResourceSet(const common::sp<IResourceSet>& rs){
        CHK_PIPELINE_SET();
        //assert(slot < _resourceSets.size());

        auto dxcLayout = _currentPipeline->GetPipelineLayout();

        resources.insert(rs);

        auto d3dkrs = PtrCast<DXCResourceSet>(rs.get());

        RegisterResourceSet(d3dkrs);

        auto& heaps = d3dkrs->GetHeaps();

        GetCmdList()->SetDescriptorHeaps(heaps.size(), heaps.data());

        for(uint32_t i = 0; i < heaps.size(); i++) {
            GetCmdList()->SetGraphicsRootDescriptorTable(i, heaps[i]->GetGPUDescriptorHandleForHeapStart());
        }

        //auto& entry = _resourceSets[slot];
        //entry.isNewlyChanged = true;
        //entry.resSet = RefRawPtr(vkrs);
        //entry.offsets = dynamicOffsets;
    }

            
    void DXCRenderCmdEnc::SetPushConstants(
        std::uint32_t pushConstantIndex,
        const std::span<uint32_t>& data,
        std::uint32_t destOffsetIn32BitValues
    ) {
        CHK_PIPELINE_SET();
        //assert(slot < _resourceSets.size());

        auto dxcLayout = _currentPipeline->GetPipelineLayout();

        auto argBase = dxcLayout->GetHeapCount();

        GetCmdList()->SetGraphicsRoot32BitConstants(
            pushConstantIndex + argBase,
            data.size(),
            data.data(),
            destOffsetIn32BitValues
        );
    }

    void DXCComputeCmdEnc::SetPipeline(const common::sp<IComputePipeline>& pipeline){

        auto dxcPipeline = PtrCast<DXCComputePipeline>(pipeline.get());
        assert(dxcPipeline != _currentPipeline);
        resources.insert(pipeline);

        dxcPipeline->CmdBindPipeline(GetCmdList());

        //ensure resource set counts
        //auto setCnt = vkPipeline->GetResourceSetCount();
        //if (setCnt > _resourceSets.size()) {
        //    _resourceSets.resize(setCnt, {});
        //}

        //Mark current pipeline
        _currentPipeline = dxcPipeline;
    }
        
    void DXCComputeCmdEnc::SetComputeResourceSet(const common::sp<IResourceSet>& rs){
        CHK_PIPELINE_SET();
        //assert(slot < _resourceSets.size());

        auto dxcLayout = _currentPipeline->GetPipelineLayout();

        resources.insert(rs);

        auto d3dkrs = PtrCast<DXCResourceSet>(rs.get());

        RegisterResourceSet(d3dkrs);

        auto& heaps = d3dkrs->GetHeaps();

        GetCmdList()->SetDescriptorHeaps(heaps.size(), heaps.data());

        for(uint32_t i = 0; i < heaps.size(); i++) {
            GetCmdList()->SetComputeRootDescriptorTable(i, heaps[i]->GetGPUDescriptorHandleForHeapStart());
        }
    }

        
    void DXCComputeCmdEnc::SetPushConstants(
        std::uint32_t pushConstantIndex,
        const std::span<uint32_t>& data,
        std::uint32_t destOffsetIn32BitValues
    ) {
        CHK_PIPELINE_SET();
        //assert(slot < _resourceSets.size());

        auto dxcLayout = _currentPipeline->GetPipelineLayout();

        auto argBase = dxcLayout->GetHeapCount();


        GetCmdList()->SetComputeRoot32BitConstants(
            pushConstantIndex,
            data.size(),
            data.data(),
            destOffsetIn32BitValues
        );
    }


    void DXCRenderCmdEnc::SetViewports(const std::span<Viewport>& viewport){
        ///#TODO: dx12 requires all viewports set as an atomic operation. Changes required.

        if (viewport.size() == 1 || dev->GetFeatures().multipleViewports)
        {
            //bool flip = gd->SupportsFlippedYDirection();
            //float vpY = flip
            //    ? viewport.height + viewport.y
            //    : viewport.y;
            //float vpHeight = flip
            //    ? -viewport.height
            //    : viewport.height;
            std::vector<D3D12_VIEWPORT> vps(viewport.size());
            for(uint32_t i = 0; i < viewport.size(); i++) {
                vps[i].TopLeftX = viewport[i].x;
                vps[i].TopLeftY = viewport[i].y;
                vps[i].Width    = viewport[i].width;
                vps[i].Height   = viewport[i].height;
                vps[i].MinDepth = viewport[i].minDepth;
                vps[i].MaxDepth = viewport[i].maxDepth;
            }
            GetCmdList()->RSSetViewports(vps.size(), vps.data());
        }
    }
    void DXCRenderCmdEnc::SetFullViewports() {
        auto rtCnt = _fb.colorTargetActions.size();
        //auto desc = _fb->GetDesc();

        std::vector<D3D12_VIEWPORT> vps;
        vps.reserve(rtCnt);

        for(auto& rt : _fb.colorTargetActions) {
            auto& texDesc = rt.target->GetTexture().GetTextureObject()->GetDesc();
            D3D12_VIEWPORT vp {
                .TopLeftX = 0,
                .TopLeftY = 0,
                .Width    = (float)texDesc.width,
                .Height   = (float)texDesc.height,
                .MinDepth = 0,
                .MaxDepth = 1,
            };

            vps.push_back(vp);
        }
        GetCmdList()->RSSetViewports(vps.size(), vps.data());
    }

    void DXCRenderCmdEnc::SetScissorRects(const std::span<Rect>& rects
    ){
        
        ///#TODO: dx12 requires all scissor rects set as an atomic operation. Changes required.
        if (rects.size() == 1 || dev->GetFeatures().multipleViewports) {

            
            std::vector<D3D12_RECT> srs(rects.size());
            for(uint32_t i = 0; i < rects.size(); i++) {
                srs[i].left = rects[i].x;
                srs[i].top = rects[i].y;
                srs[i].right = rects[i].x + rects[i].width;
                srs[i].bottom = rects[i].y + rects[i].height;
            }
            //if (_scissorRects[index] != scissor)
            //{
            //    _scissorRects[index] = scissor;
            GetCmdList()->RSSetScissorRects(srs.size(), srs.data());
            //}
        }
    }
    void DXCRenderCmdEnc::SetFullScissorRects(){
        
        auto rtCnt = _fb.colorTargetActions.size();

        std::vector<D3D12_RECT> srs;
        srs.reserve(rtCnt);
        for(auto& rt : _fb.colorTargetActions) {
            auto& texDesc = rt.target->GetTexture().GetTextureObject()->GetDesc();
            D3D12_RECT sr {
                .left = 0,
                .top = 0,
                .right    =  (LONG)texDesc.width,
                .bottom   =  (LONG)texDesc.height,
            };


            srs.push_back(sr);
        }
        
        GetCmdList()->RSSetScissorRects(srs.size(), srs.data());
    }

    void DXCRenderCmdEnc::Draw(
        std::uint32_t vertexCount, std::uint32_t instanceCount,
        std::uint32_t vertexStart, std::uint32_t instanceStart
    ){
        //PreDrawCommand();
        GetCmdList()->DrawInstanced(vertexCount, instanceCount, vertexStart, instanceStart);
    }
    
    void DXCRenderCmdEnc::DrawIndexed(
        std::uint32_t indexCount, std::uint32_t instanceCount, 
        std::uint32_t indexStart, std::uint32_t vertexOffset, 
        std::uint32_t instanceStart
    ){
        //PreDrawCommand();
        //vkCmdDrawIndexed(_cmdBuf, indexCount, instanceCount, indexStart, vertexOffset, instanceStart);
        
        GetCmdList()->DrawIndexedInstanced(indexCount, instanceCount, indexStart, vertexOffset, instanceStart);
    }
    
#if 0
    void DXCRenderCmdEnc::DrawIndirect(
        const sp<Buffer>& indirectBuffer, 
        std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride
    ){
        ///#TODO: revisit vulkan drawindirect, parameters seem incomplete
        //_resReg.RegisterBufferUsage(indirectBuffer,
        //    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        //    VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        //    );
        ////_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        //PreDrawCommand();
        //auto dxcBuffer = PtrCast<DXCBuffer>(indirectBuffer.get());
        //_cmdList->ExecuteIndirect()
        //vkCmdDrawIndirect(_cmdBuf, vkBuffer->GetHandle(), offset, drawCount, stride);
    }

    void DXCRenderCmdEnc::DrawIndexedIndirect(
        const sp<Buffer>& indirectBuffer, 
        std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride
    ){
        
        ///#TODO: revisit vulkan drawindexedindirect, parameters seem incomplete
        //_resReg.RegisterBufferUsage(indirectBuffer,
        //    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        //    VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        //);
        //PreDrawCommand();
        //
        //VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(indirectBuffer.get());
        //vkCmdDrawIndexedIndirect(_cmdBuf, vkBuffer->GetHandle(), offset, drawCount, stride);
    }
#endif
    

    void DXCComputeCmdEnc::Dispatch(
        std::uint32_t groupCountX, std::uint32_t groupCountY, std::uint32_t groupCountZ
    ){
        //PreDispatchCommand();
        //vkCmdDispatch(_cmdBuf, groupCountX, groupCountY, groupCountZ);
        GetCmdList()->Dispatch(groupCountX, groupCountY, groupCountZ);
    };

#if 0
    void DXCCommandList::DispatchIndirect(const sp<Buffer>& indirectBuffer, std::uint32_t offset) {
        ///#TODO: revisit vulkan DispatchIndirect, parameters seem incomplete
        //The name of the stage flag is slightly confusing, but the spec is 
        //otherwisely very clear it aplies to compute :
        //
        //    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT specifies the stage of the 
        //    pipeline where VkDrawIndirect* / VkDispatchIndirect * / VkTraceRaysIndirect * 
        //    data structures are consumed.
        //
        //and also :
        //
        //    For the compute pipeline, the following stages occur in this order :
        //
        //    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
        //    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        //

        //_resReg.RegisterBufferUsage(indirectBuffer,
        //    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        //    VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        //);
        //
        //PreDispatchCommand();
        //
        //VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(indirectBuffer.get());
        //vkCmdDispatchIndirect(_cmdBuf, vkBuffer->GetHandle(), offset);
    };
#endif
    //static uint32_t _ComputeSubresource(uint32_t mipLevel, uint32_t mipLevelCount, uint32_t arrayLayer)
    //{
    //    return ((arrayLayer * mipLevelCount) + mipLevel);
    //};

    void DXCTransferCmdEnc::ResolveTexture(const common::sp<ITexture>& source,
                                           const common::sp<ITexture>& destination) {
        
        auto* dxcSource = PtrCast<DXCTexture>(source.get());
        resources.insert(source);
        ID3D12Resource* resList[] = {dxcSource->GetHandle()};
        RegisterResourceUsage(resList, D3D12_RESOURCE_STATE_COPY_SOURCE);
        auto* dxcDestination = PtrCast<DXCTexture>(destination.get());
        resources.insert(destination);
        resList[0] = dxcDestination->GetHandle();
        RegisterResourceUsage(resList, D3D12_RESOURCE_STATE_COPY_SOURCE);
        
        //TODO:Implement full image layout tracking and transition systems
        //vkSource.TransitionImageLayout(_cmdBuf, 0, 1, 0, 1, VkImageLayout.TransferSrcOptimal);
        //vkDestination.TransitionImageLayout(_cmdBuf, 0, 1, 0, 1, VkImageLayout.TransferDstOptimal);

        GetCmdList()->ResolveSubresource(
            dxcSource->GetHandle(),
            0,
            dxcDestination->GetHandle(),
            0,
            dxcDestination->GetHandle()->GetDesc().Format);
    }

    void DXCTransferCmdEnc::CopyBufferToTexture(
        const common::sp<BufferRange>& src,
        std::uint32_t srcBytesPerRow,
        std::uint32_t srcBytesPerImage,
        const common::sp<ITexture>& dst,
        const Point3D& dstOrigin,
        std::uint32_t dstMipLevel,
        std::uint32_t dstBaseArrayLayer,
        const Size3D& copySize
    ){

        auto* srcDxcBuffer = PtrCast<DXCBuffer>(src->GetBufferObject());
        auto* dstDxcTexture = PtrCast<DXCTexture>(dst.get());

        resources.insert(src);
        ID3D12Resource* resList[] = {srcDxcBuffer->GetHandle()};
        RegisterResourceUsage(resList, D3D12_RESOURCE_STATE_COPY_SOURCE);
        resources.insert(dst);
        resList[0] = dstDxcTexture->GetHandle();
        RegisterResourceUsage(resList, D3D12_RESOURCE_STATE_COPY_SOURCE);

        auto& srcDesc = dst->GetDesc(); 


        D3D12_TEXTURE_COPY_LOCATION dstSubresource{};
        dstSubresource.pResource = dstDxcTexture->GetHandle();
        dstSubresource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstSubresource.SubresourceIndex = DXCTexture::ComputeSubresource(
            dstMipLevel, dstDxcTexture->GetDesc().mipLevels, dstBaseArrayLayer);
   

        D3D12_TEXTURE_COPY_LOCATION srcSubresource{};
        srcSubresource.pResource = srcDxcBuffer->GetHandle();
        srcSubresource.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcSubresource.PlacedFootprint.Offset = src->GetShape().GetOffsetInBytes();
        srcSubresource.PlacedFootprint.Footprint.Format = VdToD3DPixelFormat(srcDesc.format);
        srcSubresource.PlacedFootprint.Footprint.Width = copySize.width;
        srcSubresource.PlacedFootprint.Footprint.Height = copySize.height;
        srcSubresource.PlacedFootprint.Footprint.Depth = copySize.depth;
        srcSubresource.PlacedFootprint.Footprint.RowPitch = srcBytesPerRow;

        D3D12_BOX srcRegion {};
        srcRegion.left = 0;
        srcRegion.top = 0;
        srcRegion.front = 0;
        srcRegion.right = copySize.width;
        srcRegion.bottom = copySize.height;
        srcRegion.back = copySize.depth;

        GetCmdList()->CopyTextureRegion(&dstSubresource, dstOrigin.x, dstOrigin.y, dstOrigin.z, 
                                   &srcSubresource, &srcRegion);

    }

    
    void DXCTransferCmdEnc::CopyTextureToBuffer(
        const common::sp<ITexture>& src,
        const Point3D& srcOrigin,
        std::uint32_t srcMipLevel,
        std::uint32_t srcBaseArrayLayer,
        const common::sp<BufferRange>& dst,
        std::uint32_t dstBytesPerRow,
        std::uint32_t dstBytesPerImage,
        const Size3D& copySize
    ) {


        auto srcImage = PtrCast<DXCTexture>(src.get());
        auto dstBuffer = PtrCast<DXCBuffer>(dst->GetBufferObject());

        resources.insert(src);
        ID3D12Resource* resList[] = {srcImage->GetHandle()};
        RegisterResourceUsage(resList, D3D12_RESOURCE_STATE_COPY_SOURCE);
        resources.insert(dst);
        resList[0] = dstBuffer->GetHandle();
        RegisterResourceUsage(resList, D3D12_RESOURCE_STATE_COPY_SOURCE);

        auto srcDesc = srcImage->GetDesc();

        D3D12_TEXTURE_COPY_LOCATION srcSubresource{};
        srcSubresource.pResource = srcImage->GetHandle();
        srcSubresource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcSubresource.SubresourceIndex = DXCTexture::ComputeSubresource(
            srcMipLevel, srcImage->GetDesc().mipLevels, srcBaseArrayLayer);
   

        D3D12_TEXTURE_COPY_LOCATION dstSubresource{};
        dstSubresource.pResource = dstBuffer->GetHandle();
        dstSubresource.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dstSubresource.PlacedFootprint.Offset = dst->GetShape().GetOffsetInBytes();
        dstSubresource.PlacedFootprint.Footprint.Format = VdToD3DPixelFormat(srcDesc.format);
        dstSubresource.PlacedFootprint.Footprint.Width = copySize.width;
        dstSubresource.PlacedFootprint.Footprint.Height = copySize.height;
        dstSubresource.PlacedFootprint.Footprint.Depth = copySize.depth;
        dstSubresource.PlacedFootprint.Footprint.RowPitch = dstBytesPerRow;

        D3D12_BOX srcRegion {};
        srcRegion.left  = srcOrigin.x;
        srcRegion.top   = srcOrigin.y;
        srcRegion.front = srcOrigin.z;
        srcRegion.right  = srcOrigin.x + copySize.width;
        srcRegion.bottom = srcOrigin.y + copySize.height;
        srcRegion.back   = srcOrigin.z + copySize.depth;


        GetCmdList()->CopyTextureRegion(&dstSubresource, 0, 0, 0, &srcSubresource, &srcRegion);
    }

    void DXCTransferCmdEnc::CopyBuffer(
        const common::sp<BufferRange>& source,
        const common::sp<BufferRange>& destination,
        std::uint32_t sizeInBytes
    ){

        auto* srcDxcBuffer = PtrCast<DXCBuffer>(source->GetBufferObject());
        auto* dstDxcBuffer = PtrCast<DXCBuffer>(destination->GetBufferObject());
        
        resources.insert(source);
        ID3D12Resource* resList[] = {srcDxcBuffer->GetHandle()};
        RegisterResourceUsage(resList, D3D12_RESOURCE_STATE_COPY_SOURCE);
        resources.insert(destination);
        resList[0] = dstDxcBuffer->GetHandle();
        RegisterResourceUsage(resList, D3D12_RESOURCE_STATE_COPY_SOURCE);

        GetCmdList()->CopyBufferRegion(
            dstDxcBuffer->GetHandle(), destination->GetShape().GetOffsetInBytes(),
            srcDxcBuffer->GetHandle(), source->GetShape().GetOffsetInBytes(),
            sizeInBytes
        );
    }
                
    void DXCTransferCmdEnc::CopyTexture(
        const common::sp<ITexture>& src,
        const Point3D& srcOrigin,
        std::uint32_t srcMipLevel,
        std::uint32_t srcBaseArrayLayer,
        const common::sp<ITexture>& dst,
        const Point3D& dstOrigin,
        std::uint32_t dstMipLevel,
        std::uint32_t dstBaseArrayLayer,
        const Size3D& copySize
    ){

        auto srcDxcTexture = PtrCast<DXCTexture>(src.get());
        auto dstDxcTexture = PtrCast<DXCTexture>(dst.get());
        
        resources.insert(src);
        ID3D12Resource* resList[] = {srcDxcTexture->GetHandle()};
        RegisterResourceUsage(resList, D3D12_RESOURCE_STATE_COPY_SOURCE);
        resources.insert(dst);
        resList[0] = dstDxcTexture->GetHandle();
        RegisterResourceUsage(resList, D3D12_RESOURCE_STATE_COPY_SOURCE);

        D3D12_TEXTURE_COPY_LOCATION srcSubresource{};
        srcSubresource.pResource = srcDxcTexture->GetHandle();
        srcSubresource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcSubresource.SubresourceIndex = DXCTexture::ComputeSubresource(
            srcMipLevel, srcDxcTexture->GetDesc().mipLevels, srcBaseArrayLayer);
   

        D3D12_TEXTURE_COPY_LOCATION dstSubresource{};
        dstSubresource.pResource = dstDxcTexture->GetHandle();
        dstSubresource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstSubresource.SubresourceIndex = DXCTexture::ComputeSubresource(
            dstMipLevel, dstDxcTexture->GetDesc().mipLevels, srcBaseArrayLayer);

        D3D12_BOX srcRegion {};
        srcRegion.left  = srcOrigin.x;
        srcRegion.top   = srcOrigin.y;
        srcRegion.front = srcOrigin.z;
        srcRegion.right  = srcOrigin.x + copySize.width;
        srcRegion.bottom = srcOrigin.y + copySize.height;
        srcRegion.back   = srcOrigin.z + copySize.depth;

        GetCmdList()->CopyTextureRegion(&dstSubresource, dstOrigin.x, dstOrigin.y, dstOrigin.z, 
                                   &srcSubresource, &srcRegion);

    }

    void DXCTransferCmdEnc::GenerateMipmaps(const common::sp<ITexture>& texture){
        ///#TODO: remove this function as mipmap generation needs more flexiblity in infcanvas
    }

    DXCCommandList::~DXCCommandList(){
        for(auto* p : _passes) {
            delete p;
        }
        _cmdList->Release();
        _cmdAlloc->Release();
    }
     
    void DXCCommandList::Begin(){

        for(auto* p : _passes) {
            delete p;
        }

        _passes.clear();
        _currentPass = nullptr;

        _devRes.clear();
        // Command list allocators can only be reset when the associated 
        // command lists have finished execution on the GPU; apps should use 
        // fences to determine GPU execution progress.
        ThrowIfFailed(_cmdAlloc->Reset());

        // However, when ExecuteCommandList() is called on a particular command 
        // list, that command list can then be reset at any time and must be before 
        // re-recording.
        ThrowIfFailed(_cmdList->Reset(_cmdAlloc, nullptr));

    }
    void DXCCommandList::End(){

        if(_currentPass != nullptr) {
            assert(false);
        }

        ThrowIfFailed(_cmdList->Close());
    }


    IRenderCommandEncoder& DXCCommandList::BeginRenderPass(const RenderPassAction& actions) {
        CHK_RENDERPASS_ENDED();
        ////Record render pass

        //auto dxcfb = common::SPCast<DXCFrameBufferBase>(fb);

        DXCRenderCmdEnc* pNewEnc = new DXCRenderCmdEnc(_dev.get(), this, actions);

        ///#TODO: really support render passes using ID3D12GraphicsCommandList4::BeginRenderPass.
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;
        //

        
        auto colorAttachmentCnt = actions.colorTargetActions.size();

        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
        rtvHandles.reserve(colorAttachmentCnt);
        for(auto& action : actions.colorTargetActions) {
            auto dxcView = common::PtrCast<DXCRenderTargetBase>(action.target.get());
            rtvHandles.push_back(dxcView->GetHandle());
        }
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle { };
        if(actions.depthTargetAction.has_value()) {
            auto dxcView 
                = common::PtrCast<DXCRenderTargetBase>(actions.depthTargetAction->target.get());
            dsvHandle = dxcView->GetHandle();
        }
        else if(actions.stencilTargetAction.has_value()) {
            auto dxcView 
                = common::PtrCast<DXCRenderTargetBase>(actions.stencilTargetAction->target.get());
            dsvHandle = dxcView->GetHandle();
        }

        _cmdList->OMSetRenderTargets(rtvHandles.size(), rtvHandles.data(), FALSE, 
            dsvHandle.ptr?&dsvHandle : nullptr);

        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);

        for(auto& action : actions.colorTargetActions) {

            auto dxcView = common::PtrCast<DXCRenderTargetBase>(action.target.get());

            if(action.loadAction == alloy::LoadAction::Clear){
                auto rtv = dxcView->GetHandle();

                float clearColor[4] = {action.clearColor.r, 
                                       action.clearColor.g,
                                       action.clearColor.b, 
                                       action.clearColor.a};

                _cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
            }
        }

        
        D3D12_CLEAR_FLAGS clearFlags = (D3D12_CLEAR_FLAGS)0;
        float depthClear = 0;
        uint8_t stencilClear = 0;

        if(actions.depthTargetAction.has_value()) {
            
            if(actions.depthTargetAction->loadAction == alloy::LoadAction::Clear) {
                clearFlags |= D3D12_CLEAR_FLAG_DEPTH;
                depthClear = actions.depthTargetAction->clearDepth;
            }
        }
        
        if(actions.stencilTargetAction.has_value()) {
            if(actions.stencilTargetAction->loadAction == alloy::LoadAction::Clear) {
                clearFlags |= D3D12_CLEAR_FLAG_STENCIL;
                stencilClear = actions.stencilTargetAction->clearStencil;
            }
        }


        if(clearFlags != 0) {
            _cmdList->ClearDepthStencilView(
                dsvHandle,
                clearFlags,
                depthClear, stencilClear,
                0, nullptr
            );
        }
        

        
        return *pNewEnc;
    }

    IComputeCommandEncoder& DXCCommandList::BeginComputePass() {
        
        CHK_RENDERPASS_ENDED();
        ////Record render pass

        auto* pNewEnc = new DXCComputeCmdEnc(_dev.get(), this);
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;

        return *pNewEnc;
    }
    ITransferCommandEncoder& DXCCommandList::BeginTransferPass() {
        
        CHK_RENDERPASS_ENDED();
        ////Record render pass

        auto* pNewEnc = new DXCTransferCmdEnc(_dev.get(), this);
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;

        return *pNewEnc;
    }
    //virtual IBaseCommandEncoder* BeginWithBasicEncoder() = 0;

    void DXCCommandList::EndPass() {
        
        CHK_RENDERPASS_BEGUN();
        _currentPass->EndPass();
        _currentPass = nullptr;
    }

    void DXCCommandList::RegisterResourceUsage(
        const std::span<ID3D12Resource* const>& resources, 
        D3D12_RESOURCE_STATES state
    ) {

        std::vector<D3D12_RESOURCE_BARRIER> barriers;

        for(auto r : resources) {
            D3D12_RESOURCE_STATES prevState = D3D12_RESOURCE_STATE_COMMON;
            auto it = _resState.find(r);
            if(it != _resState.end()) {
                prevState = it->second;
            } else {
                _resState[r] = state;
            }

            if(prevState == state) {
                if(prevState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
                    auto& b = barriers.emplace_back();
                    b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                    b.UAV.pResource = r;
                }
            } else {
                auto& b = barriers.emplace_back();
                b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b.Transition.StateBefore = prevState;
                b.Transition.StateAfter = state;
                b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                b.Transition.pResource = r;
                
                // Update the resource state
                _resState[r] = state;
            }
        }

        if (!barriers.empty()) {
            _cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
        }
    }

    static D3D12_RESOURCE_STATES _EnnhancedToLegacyBarrierFlags(
        const D3D12_BARRIER_SYNC& sync,
        const D3D12_BARRIER_ACCESS& access
    ){
        D3D12_RESOURCE_STATES legacyState{};

        //if(sync & D3D12_BARRIER_SYNC_RESOLVE) { 
            if(access & D3D12_BARRIER_ACCESS_RESOLVE_SOURCE)
                legacyState |= D3D12_RESOURCE_STATE_RESOLVE_SOURCE; 
            if(access & D3D12_BARRIER_ACCESS_RESOLVE_DEST)
                legacyState |= D3D12_RESOURCE_STATE_RESOLVE_DEST; 
        //}

        //if(sync & D3D12_BARRIER_SYNC_COPY) { 
            if(access & D3D12_BARRIER_ACCESS_COPY_SOURCE)
                legacyState |= D3D12_RESOURCE_STATE_COPY_SOURCE; 
            if(access & D3D12_BARRIER_ACCESS_COPY_DEST)
                legacyState |= D3D12_RESOURCE_STATE_COPY_DEST; 
        //}

        //if(sync & D3D12_BARRIER_SYNC_RAYTRACING) {
            if(access & ( D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ
                        | D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE))
                legacyState |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        //}

        //if(sync & D3D12_BARRIER_SYNC_EXECUTE_INDIRECT) {
        if(access & D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT)
            legacyState |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        //}

        //if(sync & D3D12_BARRIER_SYNC_RENDER_TARGET) {
        if(access & D3D12_BARRIER_ACCESS_RENDER_TARGET)
            legacyState |= D3D12_RESOURCE_STATE_RENDER_TARGET;
        //}

        //if(sync & D3D12_BARRIER_SYNC_DEPTH_STENCIL) {
            if(access & D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE)
                legacyState |= D3D12_RESOURCE_STATE_DEPTH_WRITE;   
            if(access & D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ)
                legacyState |= D3D12_RESOURCE_STATE_DEPTH_READ;
        //}

        //Infer from access flags
        if(access & D3D12_BARRIER_ACCESS_UNORDERED_ACCESS) {
            legacyState |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }

        if(access & D3D12_BARRIER_ACCESS_SHADER_RESOURCE) {
            legacyState |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            //if(sync & D3D12_BARRIER_SYNC_PIXEL_SHADING) {
                legacyState |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            //}
        }

        if(access & D3D12_BARRIER_ACCESS_INDEX_BUFFER) {
            legacyState |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
        }

        if(access & (D3D12_BARRIER_ACCESS_VERTEX_BUFFER | D3D12_BARRIER_ACCESS_CONSTANT_BUFFER)) {
            legacyState |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        }

        if(access & D3D12_BARRIER_ACCESS_STREAM_OUTPUT) {
            legacyState |= D3D12_RESOURCE_STATE_STREAM_OUT;
        }

        return legacyState;
    };

    static D3D12_BARRIER_SYNC _GetSyncStages(const alloy::PipelineStages& stages, bool isStagesBefore) {
        D3D12_BARRIER_SYNC flags = D3D12_BARRIER_SYNC_NONE;

        if(stages[alloy::PipelineStage::All])
            return D3D12_BARRIER_SYNC_ALL;

        //if(stages[alloy::PipelineStage::TopOfPipe]){
        //    if(isStagesBefore)
        //        flags |= D3D12_BARRIER_SYNC_NONE;
        //    else
        //        flags |= D3D12_BARRIER_SYNC_ALL;
        //}
        //if(stages[alloy::PipelineStage::BottomOfPipe]) {
        //    if(isStagesBefore)
        //        flags |= D3D12_BARRIER_SYNC_ALL;
        //    else
        //        flags |= D3D12_BARRIER_SYNC_NONE;
        //}

        if(stages[alloy::PipelineStage::Draw])
            flags |= D3D12_BARRIER_SYNC_DRAW;
        if(stages[alloy::PipelineStage::NonPixelShading])
            flags |= D3D12_BARRIER_SYNC_NON_PIXEL_SHADING;
        if(stages[alloy::PipelineStage::AllShading])
            flags |= D3D12_BARRIER_SYNC_ALL_SHADING;


        if(stages[alloy::PipelineStage::INPUT_ASSEMBLER])
            flags |= D3D12_BARRIER_SYNC_INDEX_INPUT;
        if(stages[alloy::PipelineStage::VERTEX_SHADING])
            flags |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
        if(stages[alloy::PipelineStage::PIXEL_SHADING])
            flags |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
        if(stages[alloy::PipelineStage::DEPTH_STENCIL])
            flags |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
        if(stages[alloy::PipelineStage::RENDER_TARGET])
            flags |= D3D12_BARRIER_SYNC_RENDER_TARGET;
        if(stages[alloy::PipelineStage::COMPUTE_SHADING])
            flags |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
        if(stages[alloy::PipelineStage::RAYTRACING])
            flags |= D3D12_BARRIER_SYNC_RAYTRACING;
        if(stages[alloy::PipelineStage::COPY])
            flags |= D3D12_BARRIER_SYNC_COPY;
        if(stages[alloy::PipelineStage::RESOLVE])
            flags |= D3D12_BARRIER_SYNC_RESOLVE;
        if(stages[alloy::PipelineStage::EXECUTE_INDIRECT])
            flags |= D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
        //if(stages[alloy::PipelineStage::EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO]){  }
        //if(stages[alloy::PipelineStage::BUILD_RAYTRACING_ACCELERATION_STRUCTURE]){  }
        //if(stages[alloy::PipelineStage::COPY_RAYTRACING_ACCELERATION_STRUCTURE]){  }
        return flags;
    };

    static D3D12_BARRIER_ACCESS _GetAccessFlags(const alloy::ResourceAccesses& accesses) {
        D3D12_BARRIER_ACCESS flags {};

        if(!accesses)
            return D3D12_BARRIER_ACCESS_NO_ACCESS;
        
        if(accesses[alloy::ResourceAccess::COMMON])
            flags |= D3D12_BARRIER_ACCESS_COMMON;
        if(accesses[alloy::ResourceAccess::VERTEX_BUFFER])
            flags |= D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
        if(accesses[alloy::ResourceAccess::CONSTANT_BUFFER])
            flags |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
        if(accesses[alloy::ResourceAccess::INDEX_BUFFER])
            flags |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
        if(accesses[alloy::ResourceAccess::RENDER_TARGET])
            flags |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
        if(accesses[alloy::ResourceAccess::UNORDERED_ACCESS])
            flags |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
        if(accesses[alloy::ResourceAccess::DepthStencilWritable])
            flags |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
        if(accesses[alloy::ResourceAccess::DepthStencilReadOnly])
            flags |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
        if(accesses[alloy::ResourceAccess::SHADER_RESOURCE])
            flags |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
        if(accesses[alloy::ResourceAccess::STREAM_OUTPUT])
            flags |= D3D12_BARRIER_ACCESS_STREAM_OUTPUT;
        if(accesses[alloy::ResourceAccess::INDIRECT_ARGUMENT])
            flags |= D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
        if(accesses[alloy::ResourceAccess::PREDICATION])
            flags |= D3D12_BARRIER_ACCESS_PREDICATION;
        if(accesses[alloy::ResourceAccess::COPY_DEST])
            flags |= D3D12_BARRIER_ACCESS_COPY_DEST;
        if(accesses[alloy::ResourceAccess::COPY_SOURCE])
            flags |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
        if(accesses[alloy::ResourceAccess::RESOLVE_DEST])
            flags |= D3D12_BARRIER_ACCESS_RESOLVE_DEST;
        if(accesses[alloy::ResourceAccess::RESOLVE_SOURCE])
            flags |= D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
        if(accesses[alloy::ResourceAccess::RAYTRACING_ACCELERATION_STRUCTURE_READ])
            flags |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
        if(accesses[alloy::ResourceAccess::RAYTRACING_ACCELERATION_STRUCTURE_WRITE])
            flags |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;
        if(accesses[alloy::ResourceAccess::SHADING_RATE_SOURCE])
            flags |= D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE;
        //if(stages[alloy::PipelineStage::EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO]){  }
        //if(stages[alloy::PipelineStage::BUILD_RAYTRACING_ACCELERATION_STRUCTURE]){  }
        //if(stages[alloy::PipelineStage::COPY_RAYTRACING_ACCELERATION_STRUCTURE]){  }
        return flags;
    };

    template<typename T>
    static void _PopulateBarrierAccess(const alloy::MemoryBarrierDescription& desc,
                                              T& barrier
    ) {
        barrier.AccessAfter = _GetAccessFlags(desc.accessAfter);
        barrier.AccessBefore = _GetAccessFlags(desc.accessBefore);
    }

    static void _EnnhancedToLegacyBarrier(
        const D3D12_GLOBAL_BARRIER& data,
        D3D12_RESOURCE_BARRIER& barrier
    ) {
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.StateBefore = _EnnhancedToLegacyBarrierFlags(data.SyncBefore, data.AccessBefore);
        barrier.Transition.StateAfter= _EnnhancedToLegacyBarrierFlags(data.SyncAfter, data.AccessAfter);
    }
    
    void DXCCommandList::Barrier(const std::vector<alloy::BarrierDescription>& descs) {

        std::vector<D3D12_RESOURCE_BARRIER> barriers{};
        
        for(auto& desc : descs) {
            auto syncAfter = _GetSyncStages(desc.memBarrier.stagesAfter, false);
            auto syncBefore = _GetSyncStages(desc.memBarrier.stagesBefore, true);

            D3D12_RESOURCE_BARRIER barrier { };
            bool isBarrierNecessary = true;

            if (!syncAfter || !syncBefore) {
                isBarrierNecessary = false;
            }else if(std::holds_alternative<alloy::MemoryBarrierResource>(desc.resourceInfo)) {
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barrier.UAV.pResource = nullptr;
            }
            else {
                if (std::holds_alternative<alloy::BufferBarrierResource>(desc.resourceInfo)) {

                    auto& barrierDesc = std::get<alloy::BufferBarrierResource>(desc.resourceInfo);
                    _devRes.insert(barrierDesc.resource);
                    auto dxcBuffer = PtrCast<DXCBuffer>(barrierDesc.resource.get());

                    D3D12_GLOBAL_BARRIER dxcBarrierFlags{};
                    dxcBarrierFlags.SyncAfter = syncAfter;
                    dxcBarrierFlags.SyncBefore = syncBefore;
                    _PopulateBarrierAccess(desc.memBarrier, dxcBarrierFlags);
                    _EnnhancedToLegacyBarrier(dxcBarrierFlags, barrier);
                    barrier.Transition.pResource = dxcBuffer->GetHandle();
                    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                }
                else /*if(std::holds_alternative<alloy::TextureBarrierDescription>(desc))*/ {

                    auto& texDesc = std::get<alloy::TextureBarrierResource>(desc.resourceInfo);
                    _devRes.insert(texDesc.resource);
                    auto dxcTex = common::PtrCast<DXCTexture>(texDesc.resource.get());

                    D3D12_GLOBAL_BARRIER dxcBarrierFlags{};
                    dxcBarrierFlags.SyncAfter = syncAfter;
                    dxcBarrierFlags.SyncBefore = syncBefore;
                    _PopulateBarrierAccess(desc.memBarrier, dxcBarrierFlags);
                    _EnnhancedToLegacyBarrier(dxcBarrierFlags, barrier);
                    barrier.Transition.pResource = dxcTex->GetHandle();
                    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                }

                if (barrier.Transition.StateBefore == barrier.Transition.StateAfter) {
                    isBarrierNecessary = false;
                }
            }

            if (isBarrierNecessary) {
                barriers.push_back(barrier);
            }

        }

        if(!barriers.empty())
            _cmdList->ResourceBarrier(barriers.size(), barriers.data());

    }

    void DXCCommandList::PushDebugGroup(const std::string& name) {};

    void DXCCommandList::PopDebugGroup() {};

    void DXCCommandList::InsertDebugMarker(const std::string& name) {};

    static void _PopulateTextureBarrier(const alloy::TextureBarrierResource& desc,
                                              D3D12_TEXTURE_BARRIER& barrier
    ) {

        auto _GetTexLayout = [](const alloy::TextureLayout& layout) -> D3D12_BARRIER_LAYOUT {
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
        };

        barrier.LayoutAfter = _GetTexLayout(desc.toLayout);
        barrier.LayoutBefore = _GetTexLayout(desc.fromLayout);
        barrier.Subresources.IndexOrFirstMipLevel = 0xffffffff;
        barrier.Subresources.NumMipLevels = 0;
        barrier.Subresources.FirstArraySlice = 0;
        barrier.Subresources.NumArraySlices = 0;
        barrier.Subresources.FirstPlane = 0;
        barrier.Subresources.NumPlanes = 0;
    }

    void DXCCommandList7::Barrier(const std::vector<alloy::BarrierDescription>& descs) {
        
        std::vector<D3D12_GLOBAL_BARRIER> memBarriers;
        std::vector<D3D12_BUFFER_BARRIER> bufBarriers;
        std::vector<D3D12_TEXTURE_BARRIER> texBarrier;

        for(auto& desc : descs) {
            auto syncAfter = _GetSyncStages(desc.memBarrier.stagesAfter, false);
            auto syncBefore = _GetSyncStages(desc.memBarrier.stagesBefore, true);

            if(std::holds_alternative<alloy::MemoryBarrierResource>(desc.resourceInfo)) {
                memBarriers.emplace_back();
                auto& barrier = memBarriers.back();
                barrier.SyncAfter = syncAfter;
                barrier.SyncBefore = syncBefore;
                _PopulateBarrierAccess(desc.memBarrier, barrier);
            }
            else if(std::holds_alternative<alloy::BufferBarrierResource>(desc.resourceInfo)) {
                bufBarriers.emplace_back();
                auto& barrier = bufBarriers.back();
                barrier.SyncAfter = syncAfter;
                barrier.SyncBefore = syncBefore;
                auto& barrierDesc = std::get<alloy::BufferBarrierResource>(desc.resourceInfo);
                _devRes.insert(barrierDesc.resource);
                auto dxcBuffer = PtrCast<DXCBuffer>(barrierDesc.resource.get());
                _PopulateBarrierAccess(desc.memBarrier, barrier);
                barrier.pResource = dxcBuffer->GetHandle();
            }
            else /*if(std::holds_alternative<alloy::TextureBarrierDescription>(desc))*/ {
                texBarrier.emplace_back();
                auto& barrier = texBarrier.back();
                barrier.SyncAfter = syncAfter;
                barrier.SyncBefore = syncBefore;
                auto& texDesc = std::get<alloy::TextureBarrierResource>(desc.resourceInfo);
                _devRes.insert(texDesc.resource);
                auto dxcTex = common::PtrCast<DXCTexture>(texDesc.resource.get());
                
                _PopulateBarrierAccess(desc.memBarrier, barrier);
                _PopulateTextureBarrier(texDesc, barrier);
                barrier.pResource = dxcTex->GetHandle();
            }
        }

        std::vector<D3D12_BARRIER_GROUP> barrierGrps;
        if(!memBarriers.empty()) {
            barrierGrps.emplace_back();
            auto& grp = barrierGrps.back();
            grp.Type = D3D12_BARRIER_TYPE_GLOBAL;
            grp.NumBarriers = memBarriers.size();
            grp.pGlobalBarriers = memBarriers.data();
        }

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
        
        GetCmdList()->Barrier(barrierGrps.size(), barrierGrps.data());
    }



} // namespace alloy
