#include "DXCCommandList.hpp"


#include "veldrid/common/Common.hpp"
#include "veldrid/Helpers.hpp"

#include "DXCDevice.hpp"
#include "D3DTypeCvt.hpp"
#include "D3DCommon.hpp"
#include "DXCTexture.hpp"
#include "DXCPipeline.hpp"
#include "DXCFrameBuffer.hpp"

namespace Veldrid
{

    sp<DXCCommandList> DXCCommandList::Make(const sp<DXCDevice>& dev) {

        auto pDev = dev->GetDevice();

        D3D12_COMMAND_LIST_TYPE cmdListType = D3D12_COMMAND_LIST_TYPE_DIRECT;

        ID3D12CommandAllocator* pAllocator;
        ThrowIfFailed(pDev->CreateCommandAllocator(cmdListType, IID_PPV_ARGS(&pAllocator)));

        
        ID3D12GraphicsCommandList* pCmdList;
        // Create the command list.
        ThrowIfFailed(pDev->CreateCommandList(0, cmdListType, pAllocator, nullptr, IID_PPV_ARGS(&pCmdList)));
        ThrowIfFailed(pCmdList->Close());

        

        if(dev->GetDevCaps().SupportEnhancedBarrier()) {
            ID3D12GraphicsCommandList7* pNewCmdList;
            pCmdList->QueryInterface(IID_PPV_ARGS(&pNewCmdList));
            pCmdList->Release();
            return sp(new DXCCommandList7(dev, pAllocator, pNewCmdList));
        }
        else if(dev->GetDevCaps().SupportMeshShader()) {
            ID3D12GraphicsCommandList6* pNewCmdList;
            pCmdList->QueryInterface(IID_PPV_ARGS(&pNewCmdList));
            pCmdList->Release();
            return sp(new DXCCommandList6(dev, pAllocator, pNewCmdList));
        }

        return sp(new DXCCommandList(dev, pAllocator, pCmdList));

    }

    //const VkAccessFlags READ_MASK =
    //    VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
    //    VK_ACCESS_INDEX_READ_BIT |
    //    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
    //    VK_ACCESS_UNIFORM_READ_BIT |
    //    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
    //    VK_ACCESS_SHADER_READ_BIT |
    //    //VK_ACCESS_SHADER_WRITE_BIT = 0x00000040,
    //    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
    //    //VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT = 0x00000100,
    //    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
    //    //VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT = 0x00000400,
    //    VK_ACCESS_TRANSFER_READ_BIT |
    //    //VK_ACCESS_TRANSFER_WRITE_BIT = 0x00001000,
    //    VK_ACCESS_HOST_READ_BIT |
    //    //VK_ACCESS_HOST_WRITE_BIT = 0x00004000,
    //    VK_ACCESS_MEMORY_READ_BIT |
    //    //VK_ACCESS_MEMORY_WRITE_BIT = 0x00010000,
    //    // Provided by VK_VERSION_1_3
    //    //VK_ACCESS_NONE |
    //    // Provided by VK_EXT_transform_feedback
    //    //VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT = 0x02000000,
    //    // Provided by VK_EXT_transform_feedback
    //    VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT |
    //    // Provided by VK_EXT_transform_feedback
    //    //VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT = 0x08000000,
    //    // Provided by VK_EXT_conditional_rendering
    //    VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT |
    //    // Provided by VK_EXT_blend_operation_advanced
    //    VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT |
    //    // Provided by VK_KHR_acceleration_structure
    //    VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR |
    //    // Provided by VK_KHR_acceleration_structure
    //    //VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR = 0x00400000,
    //    // Provided by VK_EXT_fragment_density_map
    //    VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT |
    //    // Provided by VK_KHR_fragment_shading_rate
    //    VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR |
    //    // Provided by VK_NV_device_generated_commands
    //    VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV;
    //    // Provided by VK_NV_device_generated_commands
    //    //VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV = 0x00040000,
    //    // Provided by VK_NV_shading_rate_image
//
    //constexpr bool _IsReadAccess(VkAccessFlags access) {
    //    return (READ_MASK & access) != 0;
    //}
//
    //constexpr bool _HasAccessHarzard(VkAccessFlags src, VkAccessFlags dst) {
    //    return (src != VK_ACCESS_NONE && dst != VK_ACCESS_NONE)
    //        && ((~READ_MASK)&(src | dst));
    //    //find anyone with a write access
    //}
//
    //void _DevResRegistry::RegisterBufferUsage(
    //    const sp<Buffer>& buffer,
    //    VkPipelineStageFlags stage,
    //    VkAccessFlags access
    //) {
    //    auto vkBuf = PtrCast<VulkanBuffer>(buffer.get());
    //    _res.insert(buffer);
//
    //    //Find usages
    //    auto res = _bufRefs.find(vkBuf);
    //    if (res != _bufRefs.end()) {
    //        //There is a access dependency
    //        auto& [thisBuf, prevRef] = *res;
    //        //And a potential hazard
    //        if (_HasAccessHarzard(prevRef.access, access)) {
    //            //Add one entry to sync infos
    //            _bufSyncs.push_back({ vkBuf, prevRef,{ stage, access} });
    //            //and clear old references
    //            prevRef.stage = stage;
    //            prevRef.access = access;
    //        }
    //        else {
    //            //gather all read references
    //            prevRef.stage |= stage;
    //            prevRef.access &= access;
    //        }
    //        
    //    }
    //    else {
    //        //This is a first time use
    //        _bufRefs.insert({ vkBuf, {stage, access} });
    //    }
    //}
//
    //void _DevResRegistry::RegisterTexUsage(
    //    const sp<Texture>& tex,
    //    VkImageLayout requiredLayout,
    //    VkPipelineStageFlags stage,
    //    VkAccessFlags access
    //) {
    //    auto vkTex = PtrCast<VulkanTexture>(tex.get());
    //    _res.insert(tex);
//
    //    //Find usages
    //    auto res = _texRefs.find(vkTex);
    //    if (vkTex->GetLayout() != requiredLayout) {
    //        //Transition needed if curr layout != required layout
    //        //Always add barrier
    //        TexRef* prevRef;
    //        if (res != _texRefs.end()) {
    //            //There is a access dependency
    //            auto& [thisTex, _prevRef] = *res;
    //            prevRef = &_prevRef;
    //            
    //        }
    //        else {
    //            const auto&[ entryIt, _insertRes] = _texRefs.insert({ 
    //                vkTex, 
    //                {
    //                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
    //                    0, 
    //                    vkTex->GetLayout()
    //                } 
    //            });
    //            std::pair<VulkanTexture* const, TexRef> & entry = *entryIt;
    //            prevRef = &entry.second;
    //        }
//
    //        //Add one entry to sync infos
    //        _texSyncs.push_back({ vkTex, *prevRef,{ stage, access, requiredLayout} });
    //        prevRef->stage = stage;
    //        prevRef->access = access;
    //        prevRef->layout = requiredLayout;
    //        vkTex->SetLayout(requiredLayout);
    //    }
    //    else {
    //        //Add barrier when there is a access hazard
    //        if (res != _texRefs.end()) {
    //            auto& [thisTex, prevRef] = *res;
    //            if (_HasAccessHarzard(prevRef.access, access)) {
    //                _texSyncs.push_back({ vkTex, prevRef,{ stage, access, requiredLayout} });
    //                prevRef.stage = stage;
    //                prevRef.access = access;
    //            }
    //            else {
    //                //There is a access dependency
    //                prevRef.stage |= stage;
    //                prevRef.access &= access;
    //            }
    //        }
    //        else {
    //            //This is a first time use
    //            _texRefs.insert({ vkTex, {stage, access, requiredLayout} });
    //        }
    //    }
    //}
//
    //void _DevResRegistry::ModifyTexUsage(
    //    const sp<Texture>& tex,
    //    VkImageLayout layout,
    //    VkPipelineStageFlags stage,
    //    VkAccessFlags access
    //) {
    //    auto vkTex = PtrCast<VulkanTexture>(tex.get());
    //    
    //    //Find usages
    //    auto res = _texRefs.find(vkTex);
    //    assert(res != _texRefs.end());
//
    //    auto& ref = (*res).second;
    //    ref.layout = layout;
    //    ref.access = access;
    //    ref.stage = stage;
    //}
//
//
    //bool _DevResRegistry::InsertPipelineBarrierIfNecessary(
    //    VkCommandBuffer cb
    //) {
    //    if (_bufSyncs.empty() && _texSyncs.empty()) {
    //        return false;
    //    }
//
    //    VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    //    VkPipelineStageFlags dstStageFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
//
    //    std::vector<VkBufferMemoryBarrier> bufBarriers;
    //    for (auto& [thisBuf, prevRef, currRef] : _bufSyncs) {
    //        srcStageFlags |= prevRef.stage;
    //        dstStageFlags |= currRef.stage;
    //        bufBarriers.push_back({VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER});
    //        auto& barrier = bufBarriers.back();
//
    //        barrier.srcAccessMask = prevRef.access;
    //        barrier.dstAccessMask = currRef.access;
    //        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //        barrier.buffer = thisBuf->GetHandle();
    //        //TODO : Currently includes the whole buffer. Maybe add finer grain control later.
    //        barrier.offset = 0;
    //        barrier.size = thisBuf->GetDesc().sizeInBytes;
    //    }
//
    //    std::vector<VkImageMemoryBarrier> imgBarriers;
    //    for (auto& [thisTex, prevRef, currRef] : _texSyncs) {
    //        srcStageFlags |= prevRef.stage;
    //        dstStageFlags |= currRef.stage;
    //        imgBarriers.push_back({ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER });
    //        auto& desc = thisTex->GetDesc();
    //        auto& barrier = imgBarriers.back();
//
    //        barrier.oldLayout = prevRef.layout;
    //        barrier.newLayout = currRef.layout;
    //        barrier.srcAccessMask = prevRef.access;
    //        barrier.dstAccessMask = currRef.access;
    //        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //        barrier.image = thisTex->GetHandle();
    //        auto& aspectMask = barrier.subresourceRange.aspectMask;
    //        if (desc.usage.depthStencil) {
    //            aspectMask = Helpers::FormatHelpers::IsStencilFormat(desc.format)
    //                ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
    //                : VK_IMAGE_ASPECT_DEPTH_BIT;
    //        }
    //        else {
    //            aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //        }
    //        barrier.subresourceRange.baseMipLevel = 0;
    //        barrier.subresourceRange.levelCount = desc.mipLevels;
    //        barrier.subresourceRange.baseArrayLayer = 0;
    //        barrier.subresourceRange.layerCount = desc.arrayLayers;
//
    //    }
//
    //    //VkMemoryBarrier{};
    //    //VkBufferMemoryBarrier{};
    //    //VkImageMemoryBarrier{};
    //    vkCmdPipelineBarrier(
    //        cb,
    //        srcStageFlags,
    //        dstStageFlags,
    //        0,
    //        0, nullptr,
    //        bufBarriers.size(), bufBarriers.data(),
    //        imgBarriers.size(), imgBarriers.data());
    //    
    //    _bufSyncs.clear();
    //    _texSyncs.clear();
//
    //    return true;
    //}


    #define CHK_RENDERPASS_BEGUN() DEBUGCODE(assert(_currentRenderPass != nullptr))
    #define CHK_RENDERPASS_ENDED() DEBUGCODE(assert(_currentRenderPass == nullptr))
    #define CHK_PIPELINE_SET() DEBUGCODE(assert(_currentPipeline != nullptr))

    DXCCommandList::~DXCCommandList(){
        _cmdList->Release();
        _cmdAlloc->Release();
    }
     
    void DXCCommandList::Begin(){

        _currentPipeline = nullptr;
        _resourceSets.clear();

        _currentRenderPass = nullptr;
        _rndPasses.clear();

        _devRes.clear();
        _miscResReg.clear();
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

        ThrowIfFailed(_cmdList->Close());
    }

    void DXCCommandList::SetPipeline(const sp<Pipeline>& pipeline){

        auto dxcPipeline = PtrCast<DXCPipelineBase>(pipeline.get());
        assert(dxcPipeline != _currentPipeline);
        _miscResReg.insert(pipeline);
        bool isComputePipeline = pipeline->IsComputePipeline();

        dxcPipeline->CmdBindPipeline(_cmdList);

        //ensure resource set counts
        //auto setCnt = vkPipeline->GetResourceSetCount();
        //if (setCnt > _resourceSets.size()) {
        //    _resourceSets.resize(setCnt, {});
        //}

        //Mark current pipeline
        _currentPipeline = dxcPipeline;
    }

    void DXCCommandList::BeginRenderPass(const sp<Framebuffer>& fb){
        CHK_RENDERPASS_ENDED();
        ////Record render pass
        //#TODO: really support render passes using ID3D12GraphicsCommandList4::BeginRenderPass.
        _rndPasses.emplace_back();
        _currentRenderPass = &_rndPasses.back();
        //
        auto dxcfb = PtrCast<DXCFrameBufferBase>(fb.get());
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
        rtvHandles.reserve(dxcfb->GetRTVCount());
        for(uint32_t i = 0; i < dxcfb->GetRTVCount(); i++) {
            rtvHandles.push_back(dxcfb->GetRTV(i));
        }
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
        if(dxcfb->HasDSV()) {
            dsvHandle = dxcfb->GetDSV();
        }
        _cmdList->OMSetRenderTargets(rtvHandles.size(), rtvHandles.data(), FALSE, dxcfb->HasDSV()?&dsvHandle : nullptr);


        _currentRenderPass->fb = RefRawPtr(dxcfb);
        //
        //_currentRenderPass->clearColorTargets.resize(
        //    vkfb->GetDesc().colorTargets.size(), {}
        //);
        //
        //vkfb->VisitAttachments([&](
        //    const sp<VulkanTexture>& vkTarget,
        //    VulkanFramebufferBase::VisitedAttachmentType type) {
        //
        //        switch (type)
        //        {
        //        case Veldrid::VulkanFramebufferBase::VisitedAttachmentType::ColorAttachment: {
        //            _resReg.RegisterTexUsage(vkTarget,
        //                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        //                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        //                VK_ACCESS_SHADER_WRITE_BIT
        //            );
        //        }break;
        //        case Veldrid::VulkanFramebufferBase::VisitedAttachmentType::DepthAttachment: {
        //            _resReg.RegisterTexUsage(vkTarget,
        //                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        //                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        //                VK_ACCESS_SHADER_WRITE_BIT
        //            );
        //        }break;
        //        case Veldrid::VulkanFramebufferBase::VisitedAttachmentType::DepthStencilAttachment: {
        //            _resReg.RegisterTexUsage(vkTarget,
        //                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        //                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        //                VK_ACCESS_SHADER_WRITE_BIT
        //            );
        //        }break;
        //        }
        //    }
        //);

        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);

        //Init attachment containers

        //auto attachmentCount = fb->GetDesc().GetAttachmentCount();
        //bool haveAnyAttachments = _currentFramebuffer.ColorTargets.Count > 0 || _currentFramebuffer.DepthTarget != null;
        //bool haveAllClearValues = _depthClearValue.HasValue || _currentFramebuffer.DepthTarget == null;
        //bool haveAnyClearValues = _depthClearValue.HasValue;
        //for (int i = 0; i < _currentFramebuffer.ColorTargets.Count; i++)
        //{
        //    if (!_validColorClearValues[i])
        //    {
        //        haveAllClearValues = false;
        //    }
        //    else
        //    {
        //        haveAnyClearValues = true;
        //    }
        //}
        
        //Push back render pass begin info into draw* commands
        //Start a renderpass as late as possible for barriers
        //to work correctly
        //VkRenderPassBeginInfo renderPassBI{};
        //renderPassBI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        //renderPassBI.renderArea = VkRect2D{(int)fb->GetDesc().GetWidth(), (int)fb->GetDesc().GetHeight()};
        //renderPassBI.framebuffer = vkfb->GetHandle();
        //
        //renderPassBI.renderPass = vkfb->GetRenderPassNoClear_Load();
        //vkCmdBeginRenderPass(_cmdBuf, &renderPassBI, VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);

        //if (!haveAnyAttachments || !haveAllClearValues)
        //{
        //    renderPassBI.renderPass = _newFramebuffer
        //        ? _currentFramebuffer.RenderPassNoClear_Init
        //        : _currentFramebuffer.RenderPassNoClear_Load;
        //    vkCmdBeginRenderPass(_cb, ref renderPassBI, VkSubpassContents.Inline);
        //    _activeRenderPass = renderPassBI.renderPass;
//
        //    if (haveAnyClearValues)
        //    {
        //        if (_depthClearValue.HasValue)
        //        {
        //            ClearDepthStencilCore(_depthClearValue.Value.depthStencil.depth, (byte)_depthClearValue.Value.depthStencil.stencil);
        //            _depthClearValue = null;
        //        }
//
        //        for (uint i = 0; i < _currentFramebuffer.ColorTargets.Count; i++)
        //        {
        //            if (_validColorClearValues[i])
        //            {
        //                _validColorClearValues[i] = false;
        //                VkClearValue vkClearValue = _clearValues[i];
        //                RgbaFloat clearColor = new RgbaFloat(
        //                    vkClearValue.color.float32_0,
        //                    vkClearValue.color.float32_1,
        //                    vkClearValue.color.float32_2,
        //                    vkClearValue.color.float32_3);
        //                ClearColorTarget(i, clearColor);
        //            }
        //        }
        //    }
        //}
        //else
        //{
        //    // We have clear values for every attachment.
        //    renderPassBI.renderPass = _currentFramebuffer.RenderPassClear;
        //    fixed (VkClearValue* clearValuesPtr = &_clearValues[0])
        //    {
        //        renderPassBI.clearValueCount = attachmentCount;
        //        renderPassBI.pClearValues = clearValuesPtr;
        //        if (_depthClearValue.HasValue)
        //        {
        //            _clearValues[_currentFramebuffer.ColorTargets.Count] = _depthClearValue.Value;
        //            _depthClearValue = null;
        //        }
        //        vkCmdBeginRenderPass(_cb, ref renderPassBI, VkSubpassContents.Inline);
        //        _activeRenderPass = _currentFramebuffer.RenderPassClear;
        //        Util.ClearArray(_validColorClearValues);
        //    }
        //}
//
        //_newFramebuffer = false;
//
//
        //    
        //if (_activeRenderPass.Handle != VkRenderPass.Null)
        //{
        //    EndCurrentRenderPass();
        //}
        //else if (!_currentFramebufferEverActive && _currentFramebuffer != null)
        //{
        //    // This forces any queued up texture clears to be emitted.
        //    BeginCurrentRenderPass();
        //    EndCurrentRenderPass();
        //}
//
        //if (_currentFramebuffer != null)
        //{
        //    _currentFramebuffer.TransitionToFinalLayout(_cb);
        //}
//
        //VkFramebufferBase vkFB = Util.AssertSubtype<Framebuffer, VkFramebufferBase>(fb);
        //_currentFramebuffer = vkFB;
        //_currentFramebufferEverActive = false;
        //_newFramebuffer = true;
        //Util.EnsureArrayMinimumSize(ref _scissorRects, Math.Max(1, (uint)vkFB.ColorTargets.Count));
        //uint clearValueCount = (uint)vkFB.ColorTargets.Count;
        //Util.EnsureArrayMinimumSize(ref _clearValues, clearValueCount + 1); // Leave an extra space for the depth value (tracked separately).
        //Util.ClearArray(_validColorClearValues);
        //Util.EnsureArrayMinimumSize(ref _validColorClearValues, clearValueCount);
        //_currentStagingInfo.Resources.Add(vkFB.RefCount);
//
        //if (fb is VkSwapchainFramebuffer scFB)
        //{
        //    _currentStagingInfo.Resources.Add(scFB.Swapchain.RefCount);
        //}
    }
    void DXCCommandList::EndRenderPass(){
        CHK_RENDERPASS_BEGUN();
        //vkCmdEndRenderPass(_cmdBuf);
        DEBUGCODE(_currentRenderPass = nullptr);


        //_currentFramebuffer.TransitionToIntermediateLayout(_cb);
        //_activeRenderPass = VkRenderPass.Null;

        //// Place a barrier between RenderPasses, so that color / depth outputs
        //// can be read in subsequent passes.
        //vkCmdPipelineBarrier(
        //    _cb,
        //    VkPipelineStageFlags.BottomOfPipe,
        //    VkPipelineStageFlags.TopOfPipe,
        //    VkDependencyFlags.None,
        //    0,
        //    null,
        //    0,
        //    null,
        //    0,
        //    null);
    }

    
    void DXCCommandList::SetVertexBuffer(
        std::uint32_t index, const sp<Buffer>& buffer, std::uint32_t offset
    ){
        //_resReg.RegisterBufferUsage(buffer,
        //    VkPipelineStageFlagBits::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        //    VkAccessFlagBits::VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
        //);
        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        
        CHK_PIPELINE_SET();
        
        _devRes.insert(buffer);

        auto gfxPipeline = PtrCast<DXCGraphicsPipeline>(_currentPipeline);
        auto& pipeDesc = gfxPipeline->GetDesc();

        auto dxcBuffer = PtrCast<DXCBuffer>(buffer.get());
        auto pRes = dxcBuffer->GetHandle();

        D3D12_VERTEX_BUFFER_VIEW view {};
        view.BufferLocation = pRes->GetGPUVirtualAddress() + offset;
        view.StrideInBytes = pipeDesc.shaderSet.vertexLayouts[0].stride;
        view.SizeInBytes = dxcBuffer->GetDesc().sizeInBytes;

        _cmdList->IASetVertexBuffers(index, 1, &view);
    }

    void DXCCommandList::SetIndexBuffer(
        const sp<Buffer>& buffer, IndexFormat format, std::uint32_t offset
    ){
        //_resReg.RegisterBufferUsage(buffer,
        //    VkPipelineStageFlagBits::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        //    VkAccessFlagBits::VK_ACCESS_INDEX_READ_BIT
        //);
        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        //VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(buffer.get());
        //vkCmdBindIndexBuffer(_cmdBuf, vkBuffer->GetHandle(), offset, Veldrid::VK::priv::VdToVkIndexFormat(format));
    
        _devRes.insert(buffer);
        
        auto dxcBuffer = PtrCast<DXCBuffer>(buffer.get());
        auto pRes = dxcBuffer->GetHandle();

        D3D12_INDEX_BUFFER_VIEW view{};
        view.Format = VdToD3DIndexFormat(format);
        view.BufferLocation = pRes->GetGPUVirtualAddress() + offset;
        view.SizeInBytes = dxcBuffer->GetDesc().sizeInBytes;

        _cmdList->IASetIndexBuffer(&view);
    }

    
    void DXCCommandList::SetGraphicsResourceSet(const sp<ResourceSet>& rs){
        CHK_PIPELINE_SET();
        //assert(slot < _resourceSets.size());
        assert(!_currentPipeline->IsComputePipeline());

        
        _devRes.insert(rs);

        auto d3dkrs = PtrCast<DXCResourceSet>(rs.get());

        auto heaps = d3dkrs->GetHeaps();

        _cmdList->SetDescriptorHeaps(heaps.size(), heaps.data());

        for(uint32_t i = 0; i < heaps.size(); i++) {

            _cmdList->SetGraphicsRootDescriptorTable(i, heaps[i]->GetGPUDescriptorHandleForHeapStart());
        }

        //auto& entry = _resourceSets[slot];
        //entry.isNewlyChanged = true;
        //entry.resSet = RefRawPtr(vkrs);
        //entry.offsets = dynamicOffsets;
    }
        
    void DXCCommandList::SetComputeResourceSet(const sp<ResourceSet>& rs){
        CHK_PIPELINE_SET();
        //assert(slot < _resourceSets.size());
        assert(_currentPipeline->IsComputePipeline());

        _devRes.insert(rs);

        auto d3dkrs = PtrCast<DXCResourceSet>(rs.get());

        auto heaps = d3dkrs->GetHeaps();

        _cmdList->SetDescriptorHeaps(heaps.size(), heaps.data());

        for(uint32_t i = 0; i < heaps.size(); i++) {

            _cmdList->SetComputeRootDescriptorTable(i, heaps[i]->GetGPUDescriptorHandleForHeapStart());
        }
    }

    
    void DXCCommandList::ClearColorTarget(
        std::uint32_t slot, 
        float r, float g, float b, float a
    ){
        CHK_RENDERPASS_BEGUN();

        if(_currentRenderPass->fb->GetRTVCount() < slot) {
            assert(false);
        } else {

            auto rtv = _currentRenderPass->fb->GetRTV(slot);

            float clearColor[4] = {r, g, b, a};

            _cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
        }

        //VkClearAttachment clearAttachment{};
        //clearAttachment.clearValue.color.float32[0] = r;
        //clearAttachment.clearValue.color.float32[1] = g;
        //clearAttachment.clearValue.color.float32[2] = b;
        //clearAttachment.clearValue.color.float32[3] = a;
        //
        ////if (_activeRenderPass != VkRenderPass.Null)
        //{
        //    auto& fb = _currentRenderPass->fb;
        //    clearAttachment.colorAttachment = slot;
        //    clearAttachment.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
        //
        //    auto colorTex = fb->GetDesc().colorTargets[slot].target;
        //    VkClearRect clearRect{};
        //    clearRect.baseArrayLayer = 0;
        //    clearRect.layerCount = 1;
        //    clearRect.rect = {0, 0, colorTex->GetDesc().width, colorTex->GetDesc().height};
        //
        //    vkCmdClearAttachments(_cmdBuf, 1, &clearAttachment, 1, &clearRect);
        //}
        //else
        //{
        //    // Queue up the clear value for the next RenderPass.
        //    _clearValues[index] = clearValue;
        //    _validColorClearValues[index] = true;
        //}
    }

    void DXCCommandList::ClearDepthStencil(float depth, std::uint8_t stencil){
        CHK_RENDERPASS_BEGUN();

        if(!_currentRenderPass->fb->HasDSV()) {
            assert(false);
        } else {
            D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH;
            if(_currentRenderPass->fb->DSVHasStencil()) {
                clearFlags |= D3D12_CLEAR_FLAG_STENCIL;
            }

            auto dsv = _currentRenderPass->fb->GetDSV();

            _cmdList->ClearDepthStencilView(
                dsv,
                clearFlags,
                depth, stencil,
                0, nullptr
            );
        }

        //VkClearAttachment clearAttachment{};
        //clearAttachment.clearValue.depthStencil.depth = depth;
        //clearAttachment.clearValue.depthStencil.stencil = stencil;
        //
        ////if (_activeRenderPass != VkRenderPass.Null)
        //{
        //    auto& fb = _currentRenderPass->fb;
        //    
        //    clearAttachment.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
        //    if(Helpers::FormatHelpers::IsStencilFormat(
        //        fb->GetDesc().depthTarget.target->GetDesc().format
        //    )){
        //        clearAttachment.aspectMask |= VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT;
        //    }                        
        //
        //    auto renderableWidth = fb->GetDesc().GetWidth();
        //    auto renderableHeight = fb->GetDesc().GetHeight();
        //    if (renderableWidth > 0 && renderableHeight > 0)
        //    {
        //        VkClearRect clearRect{};
        //        clearRect.baseArrayLayer = 0;
        //        clearRect.layerCount = 1;
        //        clearRect.rect = {0, 0, renderableWidth, renderableHeight};
        //
        //        vkCmdClearAttachments(_cmdBuf, 1, &clearAttachment, 1, &clearRect);
        //    }
        //}
        //else
        //{
        //    // Queue up the clear value for the next RenderPass.
        //    _depthClearValue = clearValue;
        //}
    }


    void DXCCommandList::SetViewport(std::uint32_t index, const Viewport& viewport){
        //#TODO: dx12 requires all viewports set as an atomic operation. Changes required.
        auto gd = PtrCast<DXCDevice>(dev.get());
        if (index == 0 || gd->GetFeatures().multipleViewports)
        {
            //bool flip = gd->SupportsFlippedYDirection();
            //float vpY = flip
            //    ? viewport.height + viewport.y
            //    : viewport.y;
            //float vpHeight = flip
            //    ? -viewport.height
            //    : viewport.height;
            
            D3D12_VIEWPORT d3dvp{};
            d3dvp.TopLeftX = viewport.x;
            d3dvp.TopLeftY = viewport.y;
            d3dvp.Width = viewport.width;
            d3dvp.Height = viewport.height;
            d3dvp.MinDepth = viewport.minDepth;
            d3dvp.MaxDepth = viewport.maxDepth;
            
            _cmdList->RSSetViewports(1, &d3dvp);
        }
    }
    void DXCCommandList::SetFullViewport(std::uint32_t index) {
        CHK_RENDERPASS_BEGUN();
        auto& fb = _currentRenderPass->fb;
        SetViewport(index, {0, 0, (float)fb->GetDesc().GetWidth(), (float)fb->GetDesc().GetHeight(), 0, 1});
    }
    void DXCCommandList::SetFullViewports() {
        CHK_RENDERPASS_BEGUN();
        auto& fb = _currentRenderPass->fb;
        SetViewport(0, {0, 0, (float)fb->GetDesc().GetWidth(), (float)fb->GetDesc().GetHeight(), 0, 1});
        for (unsigned index = 1; index < fb->GetDesc().colorTargets.size(); index++)
        {
            SetViewport(index, {0, 0, (float)fb->GetDesc().GetWidth(), (float)fb->GetDesc().GetHeight(), 0, 1});
        }
    }

    void DXCCommandList::SetScissorRect(
        std::uint32_t index, 
        std::uint32_t x, std::uint32_t y, 
        std::uint32_t width, std::uint32_t height
    ){
        
        //#TODO: dx12 requires all scissor rects set as an atomic operation. Changes required.
        auto gd = PtrCast<DXCDevice>(dev.get());
        if (index == 0 || gd->GetFeatures().multipleViewports) {

            D3D12_RECT scissor{(int)x, (int)y, x+width, y+height};
            //if (_scissorRects[index] != scissor)
            //{
            //    _scissorRects[index] = scissor;
            _cmdList->RSSetScissorRects(1, &scissor);
            //}
        }
    }
    void DXCCommandList::SetFullScissorRect(std::uint32_t index){
        CHK_RENDERPASS_BEGUN();
        auto& fb = _currentRenderPass->fb;
        SetScissorRect(index, 0, 0, fb->GetDesc().GetWidth(), fb->GetDesc().GetHeight());
    }
    void DXCCommandList::SetFullScissorRects() {
        CHK_RENDERPASS_BEGUN();
        auto& fb = _currentRenderPass->fb;

        SetScissorRect(0, 0, 0, fb->GetDesc().GetWidth(), fb->GetDesc().GetHeight());

        for (unsigned index = 1; index < fb->GetDesc().colorTargets.size(); index++)
        {
            SetScissorRect(index, 0, 0, fb->GetDesc().GetWidth(), fb->GetDesc().GetHeight());
        }
    }

    //void DXCCommandList::_RegisterResourceSetUsage(VkPipelineBindPoint bindPoint) {
    //    VkPipelineStageFlags accessStage = 0;
    //    switch (bindPoint)
    //    {
    //    case VK_PIPELINE_BIND_POINT_GRAPHICS:
    //        accessStage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    //        break;
    //    case VK_PIPELINE_BIND_POINT_COMPUTE:
    //        accessStage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    //        break;
    //    case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
    //        accessStage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    //        break;
    //    default:
    //        break;
    //    }
    //   
    //    for (auto& set : _resourceSets) {
    //        if (!set.isNewlyChanged) continue;
    //
    //        assert(set.resSet != nullptr); // all sets should be set already
    //        auto& desc = set.resSet->GetDesc();
    //        VulkanResourceLayout* vkLayout = PtrCast<VulkanResourceLayout>(desc.layout.get());
    //        for (unsigned i = 0; i < desc.boundResources.size(); i++) {
    //            auto& elem = vkLayout->GetDesc().elements[i];
    //            auto& res = desc.boundResources[i];
    //            auto type = elem.kind;
    //            bool writable = elem.options.writable;
    //            using _ResKind = IBindableResource::ResourceKind;
    //            switch (type) {
    //            case _ResKind::UniformBuffer: {
    //                auto* range = PtrCast<BufferRange>(res.get());
    //                auto* rangedVkBuffer = reinterpret_cast<VulkanBuffer*>(range->GetBufferObject());
    //                _resReg.RegisterBufferUsage(RefRawPtr(rangedVkBuffer), accessStage, VK_ACCESS_SHADER_READ_BIT);
    //            } break;
    //            case _ResKind::StorageBuffer: {
    //                VkAccessFlags accessFlags = VK_ACCESS_SHADER_READ_BIT;
    //                if(writable) {
    //                    accessFlags |= VK_ACCESS_SHADER_WRITE_BIT;
    //                }
    //                auto* range = PtrCast<BufferRange>(res.get());
    //                auto* rangedVkBuffer = reinterpret_cast<VulkanBuffer*>(range->GetBufferObject());
    //                _resReg.RegisterBufferUsage(RefRawPtr(rangedVkBuffer), accessStage, accessFlags);
    //            } break;
    //
    //            case _ResKind::Texture: {
    //                auto* vkTexView = PtrCast<VulkanTextureView>(res.get());
    //                auto vkTex = PtrCast<VulkanTexture>(vkTexView->GetTarget().get());
    //                if(writable) {
    //                    _resReg.RegisterTexUsage(
    //                        RefRawPtr(vkTex),
    //                        VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
    //                        accessStage,
    //                        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
    //                    );
    //
    //                } else {
    //                    _resReg.RegisterTexUsage(
    //                        RefRawPtr(vkTex),
    //                        VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //                        accessStage, VK_ACCESS_SHADER_READ_BIT);
    //                }
    //            }break;
    //            default:break;
    //            }
    //        }
    //    }
    //
    //    _resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
    //}

    //void DXCCommandList::_FlushNewResourceSets(
    //    VkPipelineBindPoint bindPoint
    //) {
    //    auto pipeline = _currentPipeline;
    //    auto resourceSetCount = pipeline->GetResourceSetCount();
    //
    //    std::vector<VkDescriptorSet> descriptorSets;
    //    descriptorSets.reserve(resourceSetCount);
    //    std::vector<std::uint32_t> dynamicOffsets;
    //    dynamicOffsets.reserve(pipeline->GetDynamicOffsetCount());
    //
    //    //Segment tracker, since newly bound res sets may not be continguous
    //    unsigned currentBatchFirstSet = 0;
    //    for (unsigned currentSlot = 0; currentSlot < resourceSetCount; currentSlot++)
    //    {
    //        bool batchEnded 
    //            =  !_resourceSets[currentSlot].isNewlyChanged
    //            || currentSlot == resourceSetCount - 1;
    //
    //        if (_resourceSets[currentSlot].isNewlyChanged)
    //        {
    //            //Flip the flag, since this set has been read
    //            _resourceSets[currentSlot].isNewlyChanged = false;
    //            // Increment ref count on first use of a set.
    //            _miscResReg.insert(_resourceSets[currentSlot].resSet);
    //
    //            VulkanResourceSet* vkSet = _resourceSets[currentSlot].resSet.get();
    //            auto& desc = vkSet->GetDesc();
    //            VulkanResourceLayout* vkLayout = PtrCast<VulkanResourceLayout>(desc.layout.get());
    //            
    //            descriptorSets.push_back(vkSet->GetHandle());
    //
    //            auto& curSetOffsets = _resourceSets[currentSlot].offsets;
    //            //for (unsigned i = 0; i < curSetOffsets.size(); i++)
    //            //{
    //            //    dynamicOffsets[currentBatchDynamicOffsetCount] = curSetOffsets.Get(i);
    //            //    currentBatchDynamicOffsetCount += 1;
    //            //}
    //            dynamicOffsets.insert(dynamicOffsets.end(), curSetOffsets.begin(), curSetOffsets.end());
    //        }
    //
    //        if (batchEnded)
    //        {
    //            if (!descriptorSets.empty())
    //            {
    //                // Flush current batch.
    //                vkCmdBindDescriptorSets(
    //                    _cmdBuf,
    //                    bindPoint,
    //                    pipeline->GetLayout(),
    //                    currentBatchFirstSet,
    //                    descriptorSets.size(),
    //                    descriptorSets.data(),
    //                    dynamicOffsets.size(),
    //                    dynamicOffsets.data());
    //
    //                descriptorSets.clear();
    //                dynamicOffsets.clear();
    //            }
    //
    //            currentBatchFirstSet = currentSlot + 1;
    //        }
    //    }
    //}

    //void DXCCommandList::PreDrawCommand(){
    //    //Run some checks
    //    //TODO:Handle renderpass end after begun but no draw command
    //    CHK_RENDERPASS_BEGUN();
    //    //We have a pipeline
    //    assert(_currentPipeline != nullptr);
    //    //And the pipeline is a graphics one
    //    assert(!_currentPipeline->IsComputePipeline());
    //
    //    //Register resource sets
    //    _RegisterResourceSetUsage(VK_PIPELINE_BIND_POINT_GRAPHICS);
    //
    //    //Begin renderpass
    //    VulkanFramebufferBase* vkfb = _currentRenderPass->fb.get();
    //    _currentRenderPass->fb = RefRawPtr(vkfb);
    //
    //    VkRenderPassBeginInfo renderPassBI{};
    //    renderPassBI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    //    renderPassBI.renderArea = VkRect2D{ {0, 0}, {vkfb->GetDesc().GetWidth(), vkfb->GetDesc().GetHeight()} };
    //    renderPassBI.framebuffer = vkfb->GetHandle();
    //
    //    renderPassBI.renderPass = vkfb->GetRenderPassNoClear_Load();
    //    vkCmdBeginRenderPass(_cmdBuf, &renderPassBI, VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);
    //
    //    //Clear values?        
    //    std::vector<VkClearAttachment> clearAttachments;
    //    for (unsigned i = 0; i < _currentRenderPass->clearColorTargets.size(); i++) {
    //        if (_currentRenderPass->clearColorTargets[i].has_value()) {
    //            clearAttachments.push_back({});
    //            VkClearAttachment& clearAttachment = clearAttachments.back();
    //            clearAttachment.clearValue.color = _currentRenderPass->clearColorTargets[i].value();
    //            ////if (_activeRenderPass != VkRenderPass.Null)
    //            //{
    //            //    auto& fb = _currentRenderPass->fb;
    //                clearAttachment.colorAttachment = i;
    //                clearAttachment.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
    //            //
    //                
    //            //
    //            //    vkCmdClearAttachments(_cmdBuf, 1, &clearAttachment, 1, &clearRect);
    //            //}
    //            //else
    //            //{
    //            //    // Queue up the clear value for the next RenderPass.
    //            //    _clearValues[index] = clearValue;
    //            //    _validColorClearValues[index] = true;
    //            //}
    //        }
    //    }
    //    if (_currentRenderPass->clearDSTarget.has_value()) {
    //        clearAttachments.push_back({});
    //        VkClearAttachment& clearAttachment = clearAttachments.back();
    //        clearAttachment.clearValue.depthStencil = _currentRenderPass->clearDSTarget.value();
    //        auto& fb = _currentRenderPass->fb;
    //        
    //        clearAttachment.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
    //        if(Helpers::FormatHelpers::IsStencilFormat(
    //            fb->GetDesc().depthTarget.target->GetDesc().format
    //        )){
    //            clearAttachment.aspectMask |= VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT;
    //        }     
    //       
    //    }
    //
    //    if (!clearAttachments.empty()) {
    //        VkClearRect clearRect{};
    //        clearRect.baseArrayLayer = 0;
    //        clearRect.layerCount = 1;
    //        clearRect.rect = { 0, 0, vkfb->GetDesc().GetWidth(), vkfb->GetDesc().GetHeight()};
    //        vkCmdClearAttachments(
    //            _cmdBuf, 
    //            clearAttachments.size(), clearAttachments.data(),
    //            1, &clearRect
    //        );
    //    }
    //
    //    //Bind resource sets
    //    _FlushNewResourceSets(VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS);
    //    
    //    //Begin render pass
    //
    //    //Might have image transition commands?
    //    //    TransitionImages(_preDrawSampledImages, VkImageLayout.ShaderReadOnlyOptimal);
    //    //    _preDrawSampledImages.Clear();
    //    //
    //    //    EnsureRenderPassActive();
    //    //
    //    //    FlushNewResourceSets(
    //    //        _currentGraphicsResourceSets,
    //    //        _graphicsResourceSetsChanged,
    //    //        _currentGraphicsPipeline.ResourceSetCount,
    //    //        VkPipelineBindPoint.Graphics,
    //    //        _currentGraphicsPipeline.PipelineLayout);
    //}

    void DXCCommandList::Draw(
        std::uint32_t vertexCount, std::uint32_t instanceCount,
        std::uint32_t vertexStart, std::uint32_t instanceStart
    ){
        //PreDrawCommand();
        _cmdList->DrawInstanced(vertexCount, instanceCount, vertexStart, instanceStart);
    }
    
    void DXCCommandList::DrawIndexed(
        std::uint32_t indexCount, std::uint32_t instanceCount, 
        std::uint32_t indexStart, std::uint32_t vertexOffset, 
        std::uint32_t instanceStart
    ){
        //PreDrawCommand();
        //vkCmdDrawIndexed(_cmdBuf, indexCount, instanceCount, indexStart, vertexOffset, instanceStart);
        
        _cmdList->DrawIndexedInstanced(indexCount, instanceCount, indexStart, vertexOffset, instanceStart);
    }
    
    void DXCCommandList::DrawIndirect(
        const sp<Buffer>& indirectBuffer, 
        std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride
    ){
        //#TODO: revisit vulkan drawindirect, parameters seem incomplete
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

    void DXCCommandList::DrawIndexedIndirect(
        const sp<Buffer>& indirectBuffer, 
        std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride
    ){
        
        //#TODO: revisit vulkan drawindexedindirect, parameters seem incomplete
        //_resReg.RegisterBufferUsage(indirectBuffer,
        //    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        //    VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        //);
        //PreDrawCommand();
        //
        //VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(indirectBuffer.get());
        //vkCmdDrawIndexedIndirect(_cmdBuf, vkBuffer->GetHandle(), offset, drawCount, stride);
    }

    
    void DXCCommandList::PreDispatchCommand() {
        ////Run some checks
        ////TODO:Handle renderpass end after begun but no draw command
        //CHK_RENDERPASS_ENDED();
        ////We have a pipeline
        //assert(_currentPipeline != nullptr);
        ////And the pipeline is a compute one
        //assert(_currentPipeline->IsComputePipeline());
        //
        ////Register resource sets
        //_RegisterResourceSetUsage(VK_PIPELINE_BIND_POINT_COMPUTE);
        //
        ////Bind resource sets
        //_FlushNewResourceSets(VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE);
        //
        ////Begin render pass
        
    }

    void DXCCommandList::Dispatch(
        std::uint32_t groupCountX, std::uint32_t groupCountY, std::uint32_t groupCountZ
    ){
        //PreDispatchCommand();
        //vkCmdDispatch(_cmdBuf, groupCountX, groupCountY, groupCountZ);
        _cmdList->Dispatch(groupCountX, groupCountY, groupCountZ);
    };

    void DXCCommandList::DispatchIndirect(const sp<Buffer>& indirectBuffer, std::uint32_t offset) {
        //#TODO: revisit vulkan DispatchIndirect, parameters seem incomplete
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

    static uint32_t _ComputeSubresource(uint32_t mipLevel, uint32_t mipLevelCount, uint32_t arrayLayer)
    {
        return ((arrayLayer * mipLevelCount) + mipLevel);
    };

    void DXCCommandList::ResolveTexture(const sp<Texture>& source, const sp<Texture>& destination) {
        
        auto* dxcSource = PtrCast<DXCTexture>(source.get());
        _devRes.insert(source);
        auto* dxcDestination = PtrCast<DXCTexture>(destination.get());
        _devRes.insert(destination);
        
        //TODO:Implement full image layout tracking and transition systems
        //vkSource.TransitionImageLayout(_cmdBuf, 0, 1, 0, 1, VkImageLayout.TransferSrcOptimal);
        //vkDestination.TransitionImageLayout(_cmdBuf, 0, 1, 0, 1, VkImageLayout.TransferDstOptimal);

        _cmdList->ResolveSubresource(
            dxcSource->GetHandle(),
            0,
            dxcDestination->GetHandle(),
            0,
            dxcDestination->GetHandle()->GetDesc().Format);
    }

    void DXCCommandList::CopyBufferToTexture(
        const sp<Buffer>& source,
        const Texture::Description& srcDesc,
        std::uint32_t srcX, std::uint32_t srcY, std::uint32_t srcZ,
        std::uint32_t srcMipLevel,
        std::uint32_t srcBaseArrayLayer,
        const sp<Texture>& destination,
        std::uint32_t dstX, std::uint32_t dstY, std::uint32_t dstZ,
        std::uint32_t dstMipLevel,
        std::uint32_t dstBaseArrayLayer,
        std::uint32_t width, std::uint32_t height, std::uint32_t depth,
        std::uint32_t layerCount
    ){
        CHK_RENDERPASS_ENDED();

        auto* srcDxcBuffer = PtrCast<DXCBuffer>(source.get());
        auto* dstDxcTexture = PtrCast<DXCTexture>(destination.get());

        //auto GetDimension = [](std::uint32_t largestLevelDimension, std::uint32_t mipLevel)->std::uint32_t{
        //    std::uint32_t ret = largestLevelDimension;
        //    ret >>= mipLevel;
        //    //for (std::uint32_t i = 0; i < mipLevel; i++)
        //    //{
        //    //    ret /= 2;
        //    //}
        //    return std::max(1U, ret);
        //};
//
        //VkSubresourceLayout srcLayout{};
        //{
        //    std::uint32_t subresource = srcBaseArrayLayer * srcDesc.mipLevels + srcMipLevel;
        //    std::uint32_t blockSize 
        //        = Helpers::FormatHelpers::IsCompressedFormat(srcDesc.format)
        //            ? 4u 
        //            : 1u;
        //    std::uint32_t mipWidth = GetDimension(srcDesc.width, srcMipLevel);
        //    std::uint32_t mipHeight = GetDimension(srcDesc.height, srcMipLevel);
        //    std::uint32_t mipDepth = GetDimension(srcDesc.depth, srcMipLevel);
        //    std::uint32_t rowPitch = Helpers::FormatHelpers::GetRowPitch(mipWidth, srcDesc.format);
        //    std::uint32_t depthPitch = Helpers::FormatHelpers::GetDepthPitch(rowPitch, mipHeight, srcDesc.format);
//
        //    
        //    srcLayout.rowPitch = rowPitch,
        //    srcLayout.depthPitch = depthPitch,
        //    srcLayout.arrayPitch = depthPitch,
        //    srcLayout.size = depthPitch,
        //    srcLayout.offset = Helpers::ComputeSubresourceOffset(srcDesc, srcMipLevel, srcBaseArrayLayer);
        //}
//
        //VkSubresourceLayout srcLayout = srcVkTexture.GetSubresourceLayout(
        //    srcVkTexture.CalculateSubresource(srcMipLevel, srcBaseArrayLayer));
        //VkImage dstImage = dstImg->GetHandle();
        //dstVkTexture.TransitionImageLayout(
        //    cb,
        //    dstMipLevel,
        //    1,
        //    dstBaseArrayLayer,
        //    layerCount,
        //    VkImageLayout.TransferDstOptimal);

        

        std::uint32_t mipWidth, mipHeight, mipDepth;
        Helpers::GetMipDimensions(
            srcDesc, srcMipLevel, mipWidth, mipHeight, mipDepth);
        
        std::uint32_t blockSize = Helpers::FormatHelpers::IsCompressedFormat(srcDesc.format) ? 4u : 1u;
        std::uint32_t bufferRowLength = std::max(mipWidth, blockSize);
        std::uint32_t bufferImageHeight = std::max(mipHeight, blockSize);
        std::uint32_t compressedX = srcX / blockSize;
        std::uint32_t compressedY = srcY / blockSize;
        std::uint32_t blockSizeInBytes = blockSize == 1
            ? Helpers::FormatHelpers::GetSizeInBytes(srcDesc.format)
            : Helpers::FormatHelpers::GetBlockSizeInBytes(srcDesc.format);
        std::uint32_t rowPitch = Helpers::FormatHelpers::GetRowPitch(bufferRowLength, srcDesc.format);
        std::uint32_t depthPitch = Helpers::FormatHelpers::GetDepthPitch(
            rowPitch, bufferImageHeight, srcDesc.format);

        std::uint32_t copyWidth = std::min(width, mipWidth);
        std::uint32_t copyheight = std::min(height, mipHeight);

        //VkBufferImageCopy regions{};
        //    regions.bufferOffset = Helpers::ComputeSubresourceOffset(srcDesc, srcMipLevel, srcBaseArrayLayer)
        //        + (srcZ * depthPitch)
        //        + (compressedY * rowPitch)
        //        + (compressedX * blockSizeInBytes);
        //    regions.bufferRowLength = bufferRowLength;
        //    regions.bufferImageHeight = bufferImageHeight;
        //    regions.imageExtent = {copyWidth, copyheight, depth};
        //    regions.imageOffset = {(int)dstX, (int)dstY, (int)dstZ};
        //
        //VkImageSubresourceLayers &dstSubresource = regions.imageSubresource;
        //
        //dstSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
        //dstSubresource.layerCount = layerCount;
        //dstSubresource.mipLevel = dstMipLevel;
        //dstSubresource.baseArrayLayer = dstBaseArrayLayer;
        //
        //vkCmdCopyBufferToImage(
        //    _cmdBuf, srcBuffer->GetHandle(), 
        //    dstImg->GetHandle(), VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regions);

        D3D12_TEXTURE_COPY_LOCATION dstSubresource{};
        dstSubresource.pResource = dstDxcTexture->GetHandle();
        dstSubresource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstSubresource.SubresourceIndex = _ComputeSubresource(
            srcMipLevel, dstDxcTexture->GetDesc().mipLevels, srcBaseArrayLayer);
   

        D3D12_TEXTURE_COPY_LOCATION srcSubresource{};
        srcSubresource.pResource = srcDxcBuffer->GetHandle();
        srcSubresource.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcSubresource.PlacedFootprint.Offset = 
            Helpers::ComputeSubresourceOffset(srcDesc, srcMipLevel, srcBaseArrayLayer)
                + (srcZ * depthPitch)
                + (compressedY * rowPitch)
                + (compressedX * blockSizeInBytes);
        srcSubresource.PlacedFootprint.Footprint.Format = VdToD3DPixelFormat(srcDesc.format);
        srcSubresource.PlacedFootprint.Footprint.Width = srcDesc.width;
        srcSubresource.PlacedFootprint.Footprint.Height = srcDesc.height;
        srcSubresource.PlacedFootprint.Footprint.Depth = srcDesc.depth;
        srcSubresource.PlacedFootprint.Footprint.RowPitch = rowPitch;

        D3D12_BOX srcRegion {};
        srcRegion.left = srcX;
        srcRegion.top = srcY;
        srcRegion.front = srcZ;
        srcRegion.right = srcX + width;
        srcRegion.bottom = srcY + height;
        srcRegion.back = srcZ + depth;

        //vkCmdCopyImageToBuffer(
        //    _cmdBuf,
        //    srcImage, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        //    dstBuffer->GetHandle(), layerCount, layers.data());

        _cmdList->CopyTextureRegion(&dstSubresource, dstX, dstY, dstZ, &srcSubresource, &srcRegion);

        //if ((dstVkTexture.Usage & TextureUsage.Sampled) != 0)
        //{
        //    dstVkTexture.TransitionImageLayout(
        //        cb,
        //        dstMipLevel,
        //        1,
        //        dstBaseArrayLayer,
        //        layerCount,
        //        VkImageLayout.ShaderReadOnlyOptimal);
        //}
            
    }

    
    void DXCCommandList::CopyTextureToBuffer(
        const sp<Texture>& source,
        std::uint32_t srcX, std::uint32_t srcY, std::uint32_t srcZ,
        std::uint32_t srcMipLevel,
        std::uint32_t srcBaseArrayLayer,
        const sp<Buffer>& destination,
        const Texture::Description& dstDesc,
        std::uint32_t dstX, std::uint32_t dstY, std::uint32_t dstZ,
        std::uint32_t dstMipLevel,
        std::uint32_t dstBaseArrayLayer,
        std::uint32_t width, std::uint32_t height, std::uint32_t depth,
        std::uint32_t layerCount
    ) {
        CHK_RENDERPASS_ENDED();

        _devRes.insert(source);
        _devRes.insert(destination);

        auto srcDxcTexture = PtrCast<DXCTexture>(source.get());
        auto dstDxcTexture = PtrCast<DXCTexture>(destination.get());

        auto srcTexture = PtrCast<DXCTexture>(source.get());
        auto srcImage = srcTexture->GetHandle();

        //TODO: Transition layout if necessary
        //srcVkTexture.TransitionImageLayout(
        //    cb,
        //    srcMipLevel,
        //    1,
        //    srcBaseArrayLayer,
        //    layerCount,
        //    VkImageLayout.TransferSrcOptimal);

        auto dstBuffer = PtrCast<DXCBuffer>(destination.get());

        //VkImageAspectFlags aspect = (srcVkTexture->GetDesc().usage.depthStencil)
        //    ? VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT
        //    : VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
        
        std::uint32_t mipWidth, mipHeight, mipDepth;
        Helpers::GetMipDimensions(dstDesc, dstMipLevel, mipWidth, mipHeight, mipDepth);
        std::uint32_t blockSize = Helpers::FormatHelpers::IsCompressedFormat(srcTexture->GetDesc().format) ? 4u : 1u;
        std::uint32_t bufferRowLength = std::max(mipWidth, blockSize);
        std::uint32_t bufferImageHeight = std::max(mipHeight, blockSize);
        std::uint32_t compressedDstX = dstX / blockSize;
        std::uint32_t compressedDstY = dstY / blockSize;
        std::uint32_t blockSizeInBytes = blockSize == 1
            ? Helpers::FormatHelpers::GetSizeInBytes(dstDesc.format)
            : Helpers::FormatHelpers::GetBlockSizeInBytes(dstDesc.format);
        std::uint32_t rowPitch = Helpers::FormatHelpers::GetRowPitch(bufferRowLength, dstDesc.format);
        std::uint32_t depthPitch = Helpers::FormatHelpers::GetDepthPitch(
            rowPitch, bufferImageHeight, dstDesc.format);

        //std::vector<VkBufferImageCopy> layers (layerCount);
        //for(unsigned layer = 0; layer < layerCount; layer++)
        //{
        //    //VkSubresourceLayout dstLayout = dstVkTexture.GetSubresourceLayout(
        //    //    dstVkTexture.CalculateSubresource(dstMipLevel, dstBaseArrayLayer + layer));
        //
        //    VkBufferImageCopy &region = layers[layer];
        //    
        //    region.bufferRowLength = bufferRowLength;
        //    region.bufferImageHeight = bufferImageHeight;
        //    region.bufferOffset =  Helpers::ComputeSubresourceOffset(dstDesc, dstMipLevel, dstBaseArrayLayer)//dstLayout.offset
        //        + (dstZ * depthPitch)
        //        + (compressedDstY * rowPitch)
        //        + (compressedDstX * blockSizeInBytes);
        //    region.imageExtent = { width, height, depth };
        //    region.imageOffset = { (int)srcX, (int)srcY, (int)srcZ };
        //
        //    VkImageSubresourceLayers& srcSubresource = region.imageSubresource;
        //    srcSubresource.aspectMask = aspect;
        //    srcSubresource.layerCount = 1;
        //    srcSubresource.mipLevel = srcMipLevel;
        //    srcSubresource.baseArrayLayer = srcBaseArrayLayer + layer;            
        //    
        //}

        D3D12_TEXTURE_COPY_LOCATION srcSubresource{};
        srcSubresource.pResource = srcDxcTexture->GetHandle();
        srcSubresource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcSubresource.SubresourceIndex = _ComputeSubresource(
            srcMipLevel, srcDxcTexture->GetDesc().mipLevels, srcBaseArrayLayer);
   

        D3D12_TEXTURE_COPY_LOCATION dstSubresource{};
        dstSubresource.pResource = dstDxcTexture->GetHandle();
        dstSubresource.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dstSubresource.PlacedFootprint.Offset = 
            Helpers::ComputeSubresourceOffset(dstDesc, dstMipLevel, dstBaseArrayLayer)//dstLayout.offset
                + (dstZ * depthPitch)
                + (compressedDstY * rowPitch)
                + (compressedDstX * blockSizeInBytes);
        dstSubresource.PlacedFootprint.Footprint.Format = VdToD3DPixelFormat(dstDesc.format);
        dstSubresource.PlacedFootprint.Footprint.Width = dstDesc.width;
        dstSubresource.PlacedFootprint.Footprint.Height = dstDesc.height;
        dstSubresource.PlacedFootprint.Footprint.Depth = dstDesc.depth;
        dstSubresource.PlacedFootprint.Footprint.RowPitch = rowPitch;

        D3D12_BOX srcRegion {};
        srcRegion.left = srcX;
        srcRegion.top = srcY;
        srcRegion.front = srcZ;
        srcRegion.right = srcX + width;
        srcRegion.bottom = srcY + height;
        srcRegion.back = srcZ + depth;

        //vkCmdCopyImageToBuffer(
        //    _cmdBuf,
        //    srcImage, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        //    dstBuffer->GetHandle(), layerCount, layers.data());

        _cmdList->CopyTextureRegion(&dstSubresource, dstX, dstY, dstZ, &srcSubresource, &srcRegion);


        //if ((srcVkTexture.Usage & TextureUsage.Sampled) != 0)
        //{
        //    srcVkTexture.TransitionImageLayout(
        //        cb,
        //        srcMipLevel,
        //        1,
        //        srcBaseArrayLayer,
        //        layerCount,
        //        VkImageLayout.ShaderReadOnlyOptimal);
        //}
    }

    void DXCCommandList::CopyBuffer(
        const sp<Buffer>& source, std::uint32_t sourceOffset,
        const sp<Buffer>& destination, std::uint32_t destinationOffset, 
        std::uint32_t sizeInBytes
    ){
        CHK_RENDERPASS_ENDED();
        //_resReg.RegisterBufferUsage(source,
        //    VK_PIPELINE_STAGE_TRANSFER_BIT,
        //    VK_ACCESS_TRANSFER_READ_BIT
        //);
        //_resReg.RegisterBufferUsage(destination,
        //    VK_PIPELINE_STAGE_TRANSFER_BIT,
        //    VK_ACCESS_TRANSFER_WRITE_BIT
        //);

        auto* srcDxcBuffer = PtrCast<DXCBuffer>(source.get());
        auto* dstDxcBuffer = PtrCast<DXCBuffer>(destination.get());

        _devRes.insert(source);
        _devRes.insert(destination);

        //VkMemoryBarrier barrier;
        //barrier.sType = VkStructureType.MemoryBarrier;
        //barrier.srcAccessMask = VkAccessFlags.TransferWrite;
        //barrier.dstAccessMask = VkAccessFlags.VertexAttributeRead;
        //barrier.pNext = null;
        //vkCmdPipelineBarrier(
        //    _cb,
        //    VkPipelineStageFlags.Transfer, VkPipelineStageFlags.VertexInput,
        //    VkDependencyFlags.None,
        //    1, ref barrier,
        //    0, null,
        //    0, null);

        _cmdList->CopyBufferRegion(
            dstDxcBuffer->GetHandle(), destinationOffset,
            srcDxcBuffer->GetHandle(), sourceOffset,
            sizeInBytes
        );
    }
                
    void DXCCommandList::CopyTexture(
        const sp<Texture>& source,
        std::uint32_t srcX, std::uint32_t srcY, std::uint32_t srcZ,
        std::uint32_t srcMipLevel,
        std::uint32_t srcBaseArrayLayer,
        const sp<Texture>& destination,
        std::uint32_t dstX, std::uint32_t dstY, std::uint32_t dstZ,
        std::uint32_t dstMipLevel,
        std::uint32_t dstBaseArrayLayer,
        std::uint32_t width, std::uint32_t height, std::uint32_t depth,
        std::uint32_t layerCount
    ){
        CHK_RENDERPASS_ENDED();

        

        //_resReg.RegisterTexUsage(source,
        //    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        //    VK_PIPELINE_STAGE_TRANSFER_BIT,
        //    VK_ACCESS_TRANSFER_READ_BIT
        //);
        //_resReg.RegisterTexUsage(destination,
        //    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        //    VK_PIPELINE_STAGE_TRANSFER_BIT,
        //    VK_ACCESS_TRANSFER_WRITE_BIT
        //);
//
        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);

        
        _devRes.insert(source);
        _devRes.insert(destination);

        auto srcDxcTexture = PtrCast<DXCTexture>(source.get());
        auto dstDxcTexture = PtrCast<DXCTexture>(destination.get());

        D3D12_TEXTURE_COPY_LOCATION srcSubresource{};
        srcSubresource.pResource = srcDxcTexture->GetHandle();
        srcSubresource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcSubresource.SubresourceIndex = _ComputeSubresource(
            srcMipLevel, srcDxcTexture->GetDesc().mipLevels, srcBaseArrayLayer);
   

        D3D12_TEXTURE_COPY_LOCATION dstSubresource{};
        dstSubresource.pResource = dstDxcTexture->GetHandle();
        dstSubresource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstSubresource.SubresourceIndex = _ComputeSubresource(
            dstMipLevel, dstDxcTexture->GetDesc().mipLevels, srcBaseArrayLayer);

        D3D12_BOX srcRegion {};
        srcRegion.left = srcX;
        srcRegion.top = srcY;
        srcRegion.front = srcZ;
        srcRegion.right = srcX + width;
        srcRegion.bottom = srcY + height;
        srcRegion.back = srcZ + depth;

        //TODO: Insert transition commands if necessary
        //srcVkTexture.TransitionImageLayout(
        //    cb,
        //    srcMipLevel,
        //    1,
        //    srcBaseArrayLayer,
        //    layerCount,
        //    VkImageLayout.TransferSrcOptimal);
//
        //dstVkTexture.TransitionImageLayout(
        //    cb,
        //    dstMipLevel,
        //    1,
        //    dstBaseArrayLayer,
        //    layerCount,
        //    VkImageLayout.TransferDstOptimal);

        _cmdList->CopyTextureRegion(&dstSubresource, dstX, dstY, dstZ, &srcSubresource, &srcRegion);

        //if ((srcVkTexture.Usage & TextureUsage.Sampled) != 0)
        //{
        //    srcVkTexture.TransitionImageLayout(
        //        cb,
        //        srcMipLevel,
        //        1,
        //        srcBaseArrayLayer,
        //        layerCount,
        //        VkImageLayout.ShaderReadOnlyOptimal);
        //}
//
        //if ((dstVkTexture.Usage & TextureUsage.Sampled) != 0)
        //{
        //    dstVkTexture.TransitionImageLayout(
        //        cb,
        //        dstMipLevel,
        //        1,
        //        dstBaseArrayLayer,
        //        layerCount,
        //        VkImageLayout.ShaderReadOnlyOptimal);
        //}
    }

    void DXCCommandList::GenerateMipmaps(const sp<Texture>& texture){
        //#TODO: remove this function as mipmap generation needs more flexiblity in infcanvas
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
        D3D12_BARRIER_SYNC flags {};
        if(stages[alloy::PipelineStage::TopOfPipe]){
            if(isStagesBefore)
                flags |= D3D12_BARRIER_SYNC_NONE;
            else
                flags |= D3D12_BARRIER_SYNC_ALL;
        }
        if(stages[alloy::PipelineStage::BottomOfPipe]) {
            if(isStagesBefore)
                flags |= D3D12_BARRIER_SYNC_ALL;
            else
                flags |= D3D12_BARRIER_SYNC_NONE;
        }
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
        
        //if(accesses[alloy::ResourceAccess::COMMON])
        //    flags |= D3D12_BARRIER_ACCESS_COMMON;
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
        if(accesses[alloy::ResourceAccess::DEPTH_STENCIL_WRITE])
            flags |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
        if(accesses[alloy::ResourceAccess::DEPTH_STENCIL_READ])
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
        if(accesses[alloy::ResourceAccess::NO_ACCESS])
            flags |= D3D12_BARRIER_ACCESS_NO_ACCESS;
        
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
    
    void DXCCommandList::Barrier(const alloy::BarrierDescription& descs) {

        std::vector<D3D12_RESOURCE_BARRIER> barriers{};
        
        auto syncAfter = _GetSyncStages(descs.stagesAfter, false);
        auto syncBefore = _GetSyncStages(descs.stagesBefore, true);

        for(auto& desc : descs.barriers) {
            barriers.emplace_back();
            auto& barrier = barriers.back();

            if(std::holds_alternative<alloy::MemoryBarrierDescription>(desc)) {
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barrier.UAV.pResource = nullptr;
            }
            else if(std::holds_alternative<alloy::BufferBarrierDescription>(desc)) {
                
                auto& barrierDesc = std::get<alloy::BufferBarrierDescription>(desc);
                _devRes.insert(barrierDesc.resource);
                auto dxcBuffer = PtrCast<DXCBuffer>(barrierDesc.resource.get());
                
                D3D12_GLOBAL_BARRIER dxcBarrierFlags{};
                dxcBarrierFlags.SyncAfter = syncAfter;
                dxcBarrierFlags.SyncBefore = syncBefore;
                _PopulateBarrierAccess(barrierDesc, dxcBarrierFlags);
                _EnnhancedToLegacyBarrier(dxcBarrierFlags, barrier);
                barrier.Transition.pResource = dxcBuffer->GetHandle();
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            }
            else /*if(std::holds_alternative<alloy::TextureBarrierDescription>(desc))*/ {

                auto& texDesc = std::get<alloy::TextureBarrierDescription>(desc);
                _devRes.insert(texDesc.resource);
                auto dxcTex = PtrCast<DXCTexture>(texDesc.resource.get());

                D3D12_GLOBAL_BARRIER dxcBarrierFlags{};
                dxcBarrierFlags.SyncAfter = syncAfter;
                dxcBarrierFlags.SyncBefore = syncBefore;
                _PopulateBarrierAccess(texDesc, dxcBarrierFlags);
                _EnnhancedToLegacyBarrier(dxcBarrierFlags, barrier);
                barrier.Transition.pResource = dxcTex->GetHandle();
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            }
        }


        _cmdList->ResourceBarrier(barriers.size(), barriers.data());

    }

    void DXCCommandList::PushDebugGroup(const std::string& name) {};

    void DXCCommandList::PopDebugGroup() {};

    void DXCCommandList::InsertDebugMarker(const std::string& name) {};

    static void _PopulateTextureBarrier(const alloy::TextureBarrierDescription& desc,
                                              D3D12_TEXTURE_BARRIER& barrier
    ) {

        auto _GetTexLayout = [](const alloy::TextureLayout& layout) -> D3D12_BARRIER_LAYOUT {
            switch(layout) {
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

        _PopulateBarrierAccess(desc, barrier);
        barrier.LayoutAfter = _GetTexLayout(desc.toLayout);
        barrier.LayoutBefore = _GetTexLayout(desc.fromLayout);
        barrier.Subresources.IndexOrFirstMipLevel = 0xffffffff;
        barrier.Subresources.NumMipLevels = 0;
        barrier.Subresources.FirstArraySlice = 0;
        barrier.Subresources.NumArraySlices = 0;
        barrier.Subresources.FirstPlane = 0;
        barrier.Subresources.NumPlanes = 0;
    }

    void DXCCommandList7::Barrier(const alloy::BarrierDescription& descs) {
        
        std::vector<D3D12_GLOBAL_BARRIER> memBarriers;
        std::vector<D3D12_BUFFER_BARRIER> bufBarriers;
        std::vector<D3D12_TEXTURE_BARRIER> texBarrier;

        auto syncAfter = _GetSyncStages(descs.stagesAfter, false);
        auto syncBefore = _GetSyncStages(descs.stagesBefore, true);

        for(auto& desc : descs.barriers) {
            if(std::holds_alternative<alloy::MemoryBarrierDescription>(desc)) {
                memBarriers.emplace_back();
                auto& barrier = memBarriers.back();
                barrier.SyncAfter = syncAfter;
                barrier.SyncBefore = syncBefore;
                _PopulateBarrierAccess(std::get<alloy::MemoryBarrierDescription>(desc), barrier);
            }
            else if(std::holds_alternative<alloy::BufferBarrierDescription>(desc)) {
                bufBarriers.emplace_back();
                auto& barrier = bufBarriers.back();
                barrier.SyncAfter = syncAfter;
                barrier.SyncBefore = syncBefore;
                auto& barrierDesc = std::get<alloy::BufferBarrierDescription>(desc);
                _devRes.insert(barrierDesc.resource);
                auto dxcBuffer = PtrCast<DXCBuffer>(barrierDesc.resource.get());
                _PopulateBarrierAccess(barrierDesc, barrier);
                barrier.pResource = dxcBuffer->GetHandle();
            }
            else /*if(std::holds_alternative<alloy::TextureBarrierDescription>(desc))*/ {
                texBarrier.emplace_back();
                auto& barrier = texBarrier.back();
                barrier.SyncAfter = syncAfter;
                barrier.SyncBefore = syncBefore;
                auto& texDesc = std::get<alloy::TextureBarrierDescription>(desc);
                _devRes.insert(texDesc.resource);
                auto dxcTex = PtrCast<DXCTexture>(texDesc.resource.get());
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

        if(!memBarriers.empty()) {
            barrierGrps.emplace_back();
            auto& grp = barrierGrps.back();
            grp.Type = D3D12_BARRIER_TYPE_TEXTURE;
            grp.NumBarriers = texBarrier.size();
            grp.pTextureBarriers = texBarrier.data();
        }
        
        GetCmdList()->Barrier(barrierGrps.size(), barrierGrps.data());
    }



} // namespace Veldrid

