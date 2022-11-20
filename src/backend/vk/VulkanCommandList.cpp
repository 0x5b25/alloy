#include "VulkanCommandList.hpp"

#include "veldrid/common/Common.hpp"
#include "veldrid/Helpers.hpp"

#include "VkCommon.hpp"
#include "VkTypeCvt.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTexture.hpp"

namespace Veldrid{
    const VkAccessFlags READ_MASK =
        VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
        VK_ACCESS_INDEX_READ_BIT |
        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
        VK_ACCESS_UNIFORM_READ_BIT |
        VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
        VK_ACCESS_SHADER_READ_BIT |
        //VK_ACCESS_SHADER_WRITE_BIT = 0x00000040,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        //VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT = 0x00000100,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
        //VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT = 0x00000400,
        VK_ACCESS_TRANSFER_READ_BIT |
        //VK_ACCESS_TRANSFER_WRITE_BIT = 0x00001000,
        VK_ACCESS_HOST_READ_BIT |
        //VK_ACCESS_HOST_WRITE_BIT = 0x00004000,
        VK_ACCESS_MEMORY_READ_BIT |
        //VK_ACCESS_MEMORY_WRITE_BIT = 0x00010000,
        // Provided by VK_VERSION_1_3
        //VK_ACCESS_NONE |
        // Provided by VK_EXT_transform_feedback
        //VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT = 0x02000000,
        // Provided by VK_EXT_transform_feedback
        VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT |
        // Provided by VK_EXT_transform_feedback
        //VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT = 0x08000000,
        // Provided by VK_EXT_conditional_rendering
        VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT |
        // Provided by VK_EXT_blend_operation_advanced
        VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT |
        // Provided by VK_KHR_acceleration_structure
        VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR |
        // Provided by VK_KHR_acceleration_structure
        //VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR = 0x00400000,
        // Provided by VK_EXT_fragment_density_map
        VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT |
        // Provided by VK_KHR_fragment_shading_rate
        VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR |
        // Provided by VK_NV_device_generated_commands
        VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV;
        // Provided by VK_NV_device_generated_commands
        //VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV = 0x00040000,
        // Provided by VK_NV_shading_rate_image

    constexpr bool _IsReadAccess(VkAccessFlags access) {
        return (READ_MASK & access) != 0;
    }

    constexpr bool _HasAccessHarzard(VkAccessFlags src, VkAccessFlags dst) {
        return (src != VK_ACCESS_NONE && dst != VK_ACCESS_NONE)
            && ((~READ_MASK)&(src | dst));
        //find anyone with a write access
    }

    void _DevResRegistry::RegisterBufferUsage(
        const sp<Buffer>& buffer,
        VkPipelineStageFlags stage,
        VkAccessFlags access
    ) {
        auto vkBuf = PtrCast<VulkanBuffer>(buffer.get());
        _res.insert(buffer);

        //Find usages
        auto res = _bufRefs.find(vkBuf);
        if (res != _bufRefs.end()) {
            //There is a access dependency
            auto& [thisBuf, prevRef] = *res;
            //And a potential hazard
            if (_HasAccessHarzard(prevRef.access, access)) {
                //Add one entry to sync infos
                _bufSyncs.push_back({ vkBuf, prevRef,{ stage, access} });
                //and clear old references
                prevRef.stage = stage;
                prevRef.access = access;
            }
            else {
                //gather all read references
                prevRef.stage |= stage;
                prevRef.access |= access;
            }
            
        }
        else {
            //This is a first time use
            _bufRefs.insert({ vkBuf, {stage, access} });
        }
    }

    void _DevResRegistry::RegisterTexUsage(
        const sp<Texture>& tex,
        VkImageLayout requiredLayout,
        VkPipelineStageFlags stage,
        VkAccessFlags access
    ) {
        auto vkTex = PtrCast<VulkanTexture>(tex.get());
        _res.insert(tex);

        //Find usages
        auto res = _texRefs.find(vkTex);
        if (vkTex->GetLayout() != requiredLayout) {
            //Transition needed if curr layout != required layout
            //Always add barrier
            TexRef* prevRef;
            if (res != _texRefs.end()) {
                //There is a access dependency
                auto& [thisTex, _prevRef] = *res;
                prevRef = &_prevRef;
                
            }
            else {
                auto&[ entryIt, _insertRes] = _texRefs.insert({ 
                    vkTex, 
                    {
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
                        0, 
                        vkTex->GetLayout()
                    } 
                });
                std::pair<VulkanTexture* const, TexRef> & entry = *entryIt;
                prevRef = &entry.second;
            }

            //Add one entry to sync infos
            _texSyncs.push_back({ vkTex, *prevRef,{ stage, access, requiredLayout} });
            prevRef->stage = stage;
            prevRef->access = access;
            prevRef->layout = requiredLayout;
            vkTex->SetLayout(requiredLayout);
        }
        else {
            //Add barrier when there is a access hazard
            if (res != _texRefs.end()) {
                auto& [thisTex, prevRef] = *res;
                if (_HasAccessHarzard(prevRef.access, access)) {
                    _texSyncs.push_back({ vkTex, prevRef,{ stage, access, requiredLayout} });
                    prevRef.stage = stage;
                    prevRef.access = access;
                }
                else {
                    //There is a access dependency
                    prevRef.stage |= stage;
                    prevRef.access |= access;
                }
            }
            else {
                //This is a first time use
                _texRefs.insert({ vkTex, {stage, access, requiredLayout} });
            }
        }
    }

    void _DevResRegistry::ModifyTexUsage(
        const sp<Texture>& tex,
        VkImageLayout layout,
        VkPipelineStageFlags stage,
        VkAccessFlags access
    ) {
        auto vkTex = PtrCast<VulkanTexture>(tex.get());
        
        //Find usages
        auto res = _texRefs.find(vkTex);
        assert(res != _texRefs.end());

        auto& ref = (*res).second;
        ref.layout = layout;
        ref.access = access;
        ref.stage = stage;
    }


    bool _DevResRegistry::InsertPipelineBarrierIfNecessary(
        VkCommandBuffer cb
    ) {
        if (_bufSyncs.empty() && _texSyncs.empty()) {
            return false;
        }

        VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStageFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

        std::vector<VkBufferMemoryBarrier> bufBarriers;
        for (auto& [thisBuf, prevRef, currRef] : _bufSyncs) {
            srcStageFlags |= prevRef.stage;
            dstStageFlags |= currRef.stage;
            bufBarriers.push_back({VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER});
            auto& barrier = bufBarriers.back();

            barrier.srcAccessMask = prevRef.access;
            barrier.dstAccessMask = currRef.access;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer = thisBuf->GetHandle();
            //TODO : Currently includes the whole buffer. Maybe add finer grain control later.
            barrier.offset = 0;
            barrier.size = thisBuf->GetDesc().sizeInBytes;
        }

        std::vector<VkImageMemoryBarrier> imgBarriers;
        for (auto& [thisTex, prevRef, currRef] : _texSyncs) {
            srcStageFlags |= prevRef.stage;
            dstStageFlags |= currRef.stage;
            imgBarriers.push_back({ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER });
            auto& desc = thisTex->GetDesc();
            auto& barrier = imgBarriers.back();

            barrier.oldLayout = prevRef.layout;
            barrier.newLayout = currRef.layout;
            barrier.srcAccessMask = prevRef.access;
            barrier.dstAccessMask = currRef.access;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = thisTex->GetHandle();
            auto& aspectMask = barrier.subresourceRange.aspectMask;
            if (desc.usage.depthStencil) {
                aspectMask = Helpers::FormatHelpers::IsStencilFormat(desc.format)
                    ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
                    : VK_IMAGE_ASPECT_DEPTH_BIT;
            }
            else {
                aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            }
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = desc.mipLevels;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = desc.arrayLayers;

        }

        //VkMemoryBarrier{};
        //VkBufferMemoryBarrier{};
        //VkImageMemoryBarrier{};
        vkCmdPipelineBarrier(
            cb,
            srcStageFlags,
            dstStageFlags,
            0,
            0, nullptr,
            bufBarriers.size(), bufBarriers.data(),
            imgBarriers.size(), imgBarriers.data());
        
        _bufSyncs.clear();
        _texSyncs.clear();

        return true;
    }


    #define CHK_RENDERPASS_BEGUN() DEBUGCODE(assert(_currentRenderPass != nullptr))
    #define CHK_RENDERPASS_ENDED() DEBUGCODE(assert(_currentRenderPass != nullptr))
    #define CHK_PIPELINE_SET() DEBUGCODE(assert(_currentRenderPass != nullptr))

    sp<CommandList> VulkanCommandList::Make(const sp<VulkanDevice>& dev){
        auto* vkDev = PtrCast<VulkanDevice>(dev.get());

        auto cmdPool = vkDev->GetCmdPool();
        auto vkCmdBuf = cmdPool->AllocateBuffer();

        auto cmdBuf = new VulkanCommandList(dev);
        cmdBuf->_cmdBuf = vkCmdBuf;
        cmdBuf->_cmdPool = std::move(cmdPool);

        return sp(cmdBuf);
    }

    VulkanCommandList::~VulkanCommandList(){
        _cmdPool->FreeBuffer(_cmdBuf);
    }
     
    void VulkanCommandList::Begin(){
        
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(_cmdBuf, &beginInfo);

    }
    void VulkanCommandList::End(){
        vkEndCommandBuffer(_cmdBuf);
    }

    void VulkanCommandList::SetPipeline(const sp<Pipeline>& pipeline){
        CHK_RENDERPASS_BEGUN();
        assert(_currentRenderPass->pipeline != pipeline);
        VulkanPipelineBase* vkPipeline = PtrCast<VulkanPipelineBase>(pipeline.get());
        _currentRenderPass->pipeline = RefRawPtr(vkPipeline);

        auto setCnt = vkPipeline->GetResourceSetCount();

        //ensure resource set counts
        if (setCnt > _resourceSets.size()) {
            _resourceSets.resize(setCnt, {});
        }

        if(vkPipeline->IsComputePipeline()){
            vkCmdBindPipeline(
                _cmdBuf, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline->GetHandle());
        } else {
            vkCmdBindPipeline(
                _cmdBuf, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline->GetHandle());
        }
    }

    void VulkanCommandList::BeginRenderPass(const sp<Framebuffer>& fb){
        CHK_RENDERPASS_ENDED();
        //Record render pass
        _rndPasses.emplace_back();
        _currentRenderPass = &_rndPasses.back();

        VulkanFramebufferBase* vkfb = PtrCast<VulkanFramebufferBase>(fb.get());
        _currentRenderPass->fb = RefRawPtr(vkfb);

        _currentRenderPass->clearColorTargets.resize(
            vkfb->GetDesc().colorTargets.size(), {}
        );

        vkfb->VisitAttachments([&](
            const sp<VulkanTexture>& vkTarget,
            VulkanFramebufferBase::VisitedAttachmentType type) {

                switch (type)
                {
                case Veldrid::VulkanFramebufferBase::VisitedAttachmentType::ColorAttachment: {
                    _resReg.RegisterTexUsage(vkTarget,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_SHADER_WRITE_BIT
                    );
                }break;
                case Veldrid::VulkanFramebufferBase::VisitedAttachmentType::DepthAttachment: {
                    _resReg.RegisterTexUsage(vkTarget,
                        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_SHADER_WRITE_BIT
                    );
                }break;
                case Veldrid::VulkanFramebufferBase::VisitedAttachmentType::DepthStencilAttachment: {
                    _resReg.RegisterTexUsage(vkTarget,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_SHADER_WRITE_BIT
                    );
                }break;
                }
            }
        );

        _resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);

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
    void VulkanCommandList::EndRenderPass(){
        CHK_RENDERPASS_BEGUN();
        vkCmdEndRenderPass(_cmdBuf);
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

    
    void VulkanCommandList::SetVertexBuffer(
        std::uint32_t index, const sp<Buffer>& buffer, std::uint32_t offset
    ){
        _resReg.RegisterBufferUsage(buffer,
            VkPipelineStageFlagBits::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            VkAccessFlagBits::VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
        );
        _resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(buffer.get());
        std::uint64_t offset64 = offset;
        vkCmdBindVertexBuffers(_cmdBuf, index, 1, &(vkBuffer->GetHandle()), &offset64);
    }

    void VulkanCommandList::SetIndexBuffer(
        const sp<Buffer>& buffer, IndexFormat format, std::uint32_t offset
    ){
        _resReg.RegisterBufferUsage(buffer,
            VkPipelineStageFlagBits::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            VkAccessFlagBits::VK_ACCESS_INDEX_READ_BIT
        );
        _resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(buffer.get());
        vkCmdBindIndexBuffer(_cmdBuf, vkBuffer->GetHandle(), offset, VdToVkIndexFormat(format));
    }

    
    void VulkanCommandList::SetGraphicsResourceSet(
        std::uint32_t slot, 
        const sp<ResourceSet>& rs, 
        const std::vector<std::uint32_t>& dynamicOffsets
    ){
        CHK_PIPELINE_SET();
        assert(slot < _resourceSets.size());
        assert(!_currentRenderPass->pipeline->IsComputePipeline());

        auto vkrs = PtrCast<VulkanResourceSet>(rs.get());

        auto& entry = _resourceSets[slot];
        entry.isNewlyChanged = true;
        entry.resSet = RefRawPtr(vkrs);
        entry.offsets = dynamicOffsets;
    }
        
    void VulkanCommandList::SetComputeResourceSet(
        std::uint32_t slot, 
        const sp<ResourceSet>& rs, 
        const std::vector<std::uint32_t>& dynamicOffsets
    ){
        CHK_PIPELINE_SET();
        assert(slot < _resourceSets.size());
        assert(!_currentRenderPass->pipeline->IsComputePipeline());

        auto vkrs = PtrCast<VulkanResourceSet>(rs.get());

        auto& entry = _resourceSets[slot];
        entry.isNewlyChanged = true;
        entry.resSet = RefRawPtr(vkrs);
        entry.offsets = dynamicOffsets;
    }

    
    void VulkanCommandList::ClearColorTarget(
        std::uint32_t slot, 
        float r, float g, float b, float a
    ){
        CHK_RENDERPASS_BEGUN();

        auto& clearValue = _currentRenderPass->clearColorTargets[slot];
        VkClearColorValue clearColor{};

        clearColor.float32[0] = r;
        clearColor.float32[1] = g;
        clearColor.float32[2] = b;
        clearColor.float32[3] = a;

        clearValue = clearColor;

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

    void VulkanCommandList::ClearDepthStencil(float depth, std::uint8_t stencil){
        CHK_RENDERPASS_BEGUN();

        VkClearDepthStencilValue clearDS{};
        clearDS.depth = depth;
        clearDS.stencil = stencil;
        _currentRenderPass->clearDSTarget = clearDS;

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


    void VulkanCommandList::SetViewport(std::uint32_t index, const Viewport& viewport){
        
        auto gd = PtrCast<VulkanDevice>(dev.get());
        if (index == 0 || gd->GetFeatures().multipleViewports)
        {
            bool flip = gd->SupportsFlippedYDirection();
            float vpY = flip
                ? viewport.height + viewport.y
                : viewport.y;
            float vpHeight = flip
                ? -viewport.height
                : viewport.height;
            
            VkViewport vkViewport{};
            vkViewport.x = viewport.x;
            vkViewport.y = vpY;
            vkViewport.width = viewport.width;
            vkViewport.height = vpHeight;
            vkViewport.minDepth = viewport.minDepth;
            vkViewport.maxDepth = viewport.maxDepth;
            
            vkCmdSetViewport(_cmdBuf, index, 1, &vkViewport);
        }
    }
    void VulkanCommandList::SetFullViewport(std::uint32_t index) {
        CHK_RENDERPASS_BEGUN();
        auto& fb = _currentRenderPass->fb;
        SetViewport(index, {0, 0, (float)fb->GetDesc().GetWidth(), (float)fb->GetDesc().GetHeight(), 0, 1});
    }
    void VulkanCommandList::SetFullViewports() {
        CHK_RENDERPASS_BEGUN();
        auto& fb = _currentRenderPass->fb;
        SetViewport(0, {0, 0, (float)fb->GetDesc().GetWidth(), (float)fb->GetDesc().GetHeight(), 0, 1});
        for (unsigned index = 1; index < fb->GetDesc().colorTargets.size(); index++)
        {
            SetViewport(index, {0, 0, (float)fb->GetDesc().GetWidth(), (float)fb->GetDesc().GetHeight(), 0, 1});
        }
    }

    void VulkanCommandList::SetScissorRect(
        std::uint32_t index, 
        std::uint32_t x, std::uint32_t y, 
        std::uint32_t width, std::uint32_t height
    ){
        auto gd = PtrCast<VulkanDevice>(dev.get());
        if (index == 0 || gd->GetFeatures().multipleViewports) {

            VkRect2D scissor{(int)x, (int)y, (int)width, (int)height};
            //if (_scissorRects[index] != scissor)
            //{
            //    _scissorRects[index] = scissor;
                vkCmdSetScissor(_cmdBuf, index, 1, &scissor);
            //}
        }
    }
    void VulkanCommandList::SetFullScissorRect(std::uint32_t index){
        CHK_RENDERPASS_BEGUN();
        auto& fb = _currentRenderPass->fb;
        SetScissorRect(index, 0, 0, fb->GetDesc().GetWidth(), fb->GetDesc().GetHeight());
    }
    void VulkanCommandList::SetFullScissorRects() {
        CHK_RENDERPASS_BEGUN();
        auto& fb = _currentRenderPass->fb;

        SetScissorRect(0, 0, 0, fb->GetDesc().GetWidth(), fb->GetDesc().GetHeight());

        for (unsigned index = 1; index < fb->GetDesc().colorTargets.size(); index++)
        {
            SetScissorRect(index, 0, 0, fb->GetDesc().GetWidth(), fb->GetDesc().GetHeight());
        }
    }

    void VulkanCommandList::_RegisterResourceSetUsage() {
        auto accessStage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        for (auto& set : _resourceSets) {
            if (!set.isNewlyChanged) continue;

            assert(set.resSet != nullptr); // all sets should be set already
            auto& desc = set.resSet->GetDesc();
            VulkanResourceLayout* vkLayout = PtrCast<VulkanResourceLayout>(desc.layout.get());
            for (unsigned i = 0; i < desc.boundResources.size(); i++) {
                auto& elem = vkLayout->GetDesc().elements[i];
                auto& res = desc.boundResources[i];
                auto type = elem.kind;
                using _ResKind = ResourceLayout::Description::ElementDescription::ResourceKind;
                switch (type) {
                case _ResKind::UniformBuffer:
                case _ResKind::StructuredBufferReadOnly: {
                    auto* range = PtrCast<BufferRange>(res.get());
                    auto* rangedVkBuffer = reinterpret_cast<VulkanBuffer*>(range->GetBufferObject());
                    _resReg.RegisterBufferUsage(RefRawPtr(rangedVkBuffer), accessStage, VK_ACCESS_SHADER_READ_BIT);
                } break;
                case _ResKind::StructuredBufferReadWrite: {
                    auto* range = PtrCast<BufferRange>(res.get());
                    auto* rangedVkBuffer = reinterpret_cast<VulkanBuffer*>(range->GetBufferObject());
                    _resReg.RegisterBufferUsage(RefRawPtr(rangedVkBuffer), accessStage,
                        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
                } break;

                case _ResKind::TextureReadOnly: {
                    auto* vkTexView = PtrCast<VulkanTextureView>(res.get());
                    auto vkTex = PtrCast<VulkanTexture>(vkTexView->GetTarget().get());
                    _resReg.RegisterTexUsage(
                        RefRawPtr(vkTex),
                        VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        accessStage, VK_ACCESS_SHADER_READ_BIT);
                }break;

                case _ResKind::TextureReadWrite: {
                    auto* vkTexView = PtrCast<VulkanTextureView>(res.get());
                    auto vkTex = PtrCast<VulkanTexture>(vkTexView->GetTarget().get());
                    _resReg.RegisterTexUsage(
                        RefRawPtr(vkTex),
                        VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                        accessStage,
                        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
                    );
                }break;
                default:break;
                }
            }
        }

        _resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
    }

    void VulkanCommandList::_FlushNewResourceSets(
        VkPipelineBindPoint bindPoint
    ) {
        auto pipeline = _currentRenderPass->pipeline;
        auto resourceSetCount = pipeline->GetResourceSetCount();

        std::vector<VkDescriptorSet> descriptorSets;
        descriptorSets.reserve(resourceSetCount);
        std::vector<std::uint32_t> dynamicOffsets;
        dynamicOffsets.reserve(pipeline->GetDynamicOffsetCount());

        //Segment tracker, since newly bound res sets may not be continguous
        unsigned currentBatchFirstSet = 0;
        for (unsigned currentSlot = 0; currentSlot < resourceSetCount; currentSlot++)
        {
            bool batchEnded 
                =  !_resourceSets[currentSlot].isNewlyChanged
                || currentSlot == resourceSetCount - 1;

            if (_resourceSets[currentSlot].isNewlyChanged)
            {
                //Flip the flag, since this set has been read
                _resourceSets[currentSlot].isNewlyChanged = false;
                // Increment ref count on first use of a set.
                _miscResReg.insert(_resourceSets[currentSlot].resSet);

                VulkanResourceSet* vkSet = _resourceSets[currentSlot].resSet.get();
                auto& desc = vkSet->GetDesc();
                VulkanResourceLayout* vkLayout = PtrCast<VulkanResourceLayout>(desc.layout.get());
                
                descriptorSets.push_back(vkSet->GetHandle());

                auto& curSetOffsets = _resourceSets[currentSlot].offsets;
                //for (unsigned i = 0; i < curSetOffsets.size(); i++)
                //{
                //    dynamicOffsets[currentBatchDynamicOffsetCount] = curSetOffsets.Get(i);
                //    currentBatchDynamicOffsetCount += 1;
                //}
                dynamicOffsets.insert(dynamicOffsets.end(), curSetOffsets.begin(), curSetOffsets.end());
            }

            if (batchEnded)
            {
                if (!descriptorSets.empty())
                {
                    // Flush current batch.
                    vkCmdBindDescriptorSets(
                        _cmdBuf,
                        bindPoint,
                        pipeline->GetLayout(),
                        currentBatchFirstSet,
                        descriptorSets.size(),
                        descriptorSets.data(),
                        dynamicOffsets.size(),
                        dynamicOffsets.data());

                    descriptorSets.clear();
                    dynamicOffsets.clear();
                }

                currentBatchFirstSet = currentSlot + 1;
            }
        }
    }

    void VulkanCommandList::PreDrawCommand(){
        //TODO:Handle renderpass end after begun but no draw command
        CHK_RENDERPASS_BEGUN();

        //Register resource sets
        _RegisterResourceSetUsage();

        //Begin renderpass
        VulkanFramebufferBase* vkfb = _currentRenderPass->fb.get();
        _currentRenderPass->fb = RefRawPtr(vkfb);

        VkRenderPassBeginInfo renderPassBI{};
        renderPassBI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBI.renderArea = VkRect2D{ {0, 0}, {vkfb->GetDesc().GetWidth(), vkfb->GetDesc().GetHeight()} };
        renderPassBI.framebuffer = vkfb->GetHandle();

        renderPassBI.renderPass = vkfb->GetRenderPassNoClear_Load();
        vkCmdBeginRenderPass(_cmdBuf, &renderPassBI, VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);

        //Clear values?        
        std::vector<VkClearAttachment> clearAttachments;
        for (unsigned i = 0; i < _currentRenderPass->clearColorTargets.size(); i++) {
            if (_currentRenderPass->clearColorTargets[i].has_value()) {
                clearAttachments.push_back({});
                VkClearAttachment& clearAttachment = clearAttachments.back();
                clearAttachment.clearValue.color = _currentRenderPass->clearColorTargets[i].value();
                ////if (_activeRenderPass != VkRenderPass.Null)
                //{
                //    auto& fb = _currentRenderPass->fb;
                    clearAttachment.colorAttachment = i;
                    clearAttachment.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
                //
                    
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
        }
        if (_currentRenderPass->clearDSTarget.has_value()) {
            clearAttachments.push_back({});
            VkClearAttachment& clearAttachment = clearAttachments.back();
            clearAttachment.clearValue.depthStencil = _currentRenderPass->clearDSTarget.value();
            auto& fb = _currentRenderPass->fb;
            
            clearAttachment.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
            if(Helpers::FormatHelpers::IsStencilFormat(
                fb->GetDesc().depthTarget.target->GetDesc().format
            )){
                clearAttachment.aspectMask |= VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT;
            }     
            
            auto renderableWidth = fb->GetDesc().GetWidth();
            auto renderableHeight = fb->GetDesc().GetHeight();
            if (renderableWidth > 0 && renderableHeight > 0)
            {
                VkClearRect clearRect{};
                clearRect.baseArrayLayer = 0;
                clearRect.layerCount = 1;
                clearRect.rect = {0, 0, renderableWidth, renderableHeight};
            
                vkCmdClearAttachments(_cmdBuf, 1, &clearAttachment, 1, &clearRect);
            }
        }

        if (!clearAttachments.empty()) {
            VkClearRect clearRect{};
            clearRect.baseArrayLayer = 0;
            clearRect.layerCount = 1;
            clearRect.rect = { 0, 0, vkfb->GetDesc().GetWidth(), vkfb->GetDesc().GetHeight()};
            vkCmdClearAttachments(
                _cmdBuf, 
                clearAttachments.size(), clearAttachments.data(),
                1, &clearRect
            );
        }

        //Bind resource sets
        _FlushNewResourceSets(VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS);
        
        //Begin render pass

        //Might have image transition commands?
        //    TransitionImages(_preDrawSampledImages, VkImageLayout.ShaderReadOnlyOptimal);
        //    _preDrawSampledImages.Clear();
        //
        //    EnsureRenderPassActive();
        //
        //    FlushNewResourceSets(
        //        _currentGraphicsResourceSets,
        //        _graphicsResourceSetsChanged,
        //        _currentGraphicsPipeline.ResourceSetCount,
        //        VkPipelineBindPoint.Graphics,
        //        _currentGraphicsPipeline.PipelineLayout);
    }

    void VulkanCommandList::Draw(
        std::uint32_t vertexCount, std::uint32_t instanceCount,
        std::uint32_t vertexStart, std::uint32_t instanceStart
    ){
        PreDrawCommand();
        vkCmdDraw(_cmdBuf, vertexCount, instanceCount, vertexStart, instanceStart);
    }
    
    void VulkanCommandList::DrawIndexed(
        std::uint32_t indexCount, std::uint32_t instanceCount, 
        std::uint32_t indexStart, std::uint32_t vertexOffset, 
        std::uint32_t instanceStart
    ){
        PreDrawCommand();
        vkCmdDrawIndexed(_cmdBuf, indexCount, instanceCount, indexStart, vertexOffset, instanceStart);
    }
    
    void VulkanCommandList::DrawIndirect(
        const sp<Buffer>& indirectBuffer, 
        std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride
    ){
        _resReg.RegisterBufferUsage(indirectBuffer,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT
            );
        _resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        PreDrawCommand();
        VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(indirectBuffer.get());
        _devRes.insert(indirectBuffer);
        vkCmdDrawIndirect(_cmdBuf, vkBuffer->GetHandle(), offset, drawCount, stride);
    }

    void VulkanCommandList::DrawIndexedIndirect(
        const sp<Buffer>& indirectBuffer, 
        std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride
    ){
        PreDrawCommand();
        VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(indirectBuffer.get());
        _devRes.insert(indirectBuffer);
        vkCmdDrawIndexedIndirect(_cmdBuf, vkBuffer->GetHandle(), offset, drawCount, stride);
    }

    
    void VulkanCommandList::Dispatch(
        std::uint32_t groupCountX, std::uint32_t groupCountY, std::uint32_t groupCountZ) {};

    void VulkanCommandList::DispatchIndirect(const sp<Buffer>& indirectBuffer, std::uint32_t offset) {};

    void VulkanCommandList::ResolveTexture(const sp<Texture>& source, const sp<Texture>& destination) {
        
        VulkanTexture* vkSource = PtrCast<VulkanTexture>(source.get());
        _devRes.insert(source);
        VulkanTexture* vkDestination = PtrCast<VulkanTexture>(destination.get());
        _devRes.insert(destination);

        VkImageAspectFlags aspectFlags = (source->GetDesc().usage.depthStencil)
            ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
            : VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageResolve region{};
        
        region.extent = { source->GetDesc().width, source->GetDesc().height, source->GetDesc().depth };
        region.srcSubresource.layerCount = 1;
        region.srcSubresource.baseArrayLayer = 0;
        region.srcSubresource.mipLevel = 0;
        region.srcSubresource.aspectMask = aspectFlags;
        
        region.dstSubresource.layerCount = 1;
        region.dstSubresource.baseArrayLayer = 0;
        region.dstSubresource.mipLevel = 0;
        region.dstSubresource.aspectMask = aspectFlags;
        
        //TODO:Implement full image layout tracking and transition systems
        //vkSource.TransitionImageLayout(_cmdBuf, 0, 1, 0, 1, VkImageLayout.TransferSrcOptimal);
        //vkDestination.TransitionImageLayout(_cmdBuf, 0, 1, 0, 1, VkImageLayout.TransferDstOptimal);

        vkCmdResolveImage(
            _cmdBuf,
            vkSource->GetHandle(),
            VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            vkDestination->GetHandle(),
            VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region);

        if (vkDestination->GetDesc().usage.sampled)
        {
            //vkDestination.TransitionImageLayout(_cmdBuf, 0, 1, 0, 1, VkImageLayout.ShaderReadOnlyOptimal);
        }
    }

    void VulkanCommandList::UpdateBuffer(
        const sp<Buffer>& buffer,
        std::uint32_t bufferOffsetInBytes,
        void* source,
        std::uint32_t sizeInBytes
    ) {
        //VkBuffer stagingBuffer = GetStagingBuffer(sizeInBytes);
        //    _gd.UpdateBuffer(stagingBuffer, 0, source, sizeInBytes);
        //    CopyBuffer(stagingBuffer, 0, buffer, bufferOffsetInBytes, sizeInBytes);
        
    }



    void VulkanCommandList::CopyBufferToTexture(
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

        _resReg.RegisterBufferUsage(source,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT
        );

        _resReg.RegisterTexUsage(destination,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT
        );

        _resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);

        auto* srcBuffer = PtrCast<VulkanBuffer>(source.get());
        auto* dstImg = PtrCast<VulkanTexture>(destination.get());

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

        VkBufferImageCopy regions{};
            regions.bufferOffset = Helpers::ComputeSubresourceOffset(srcDesc, srcMipLevel, srcBaseArrayLayer)
                + (srcZ * depthPitch)
                + (compressedY * rowPitch)
                + (compressedX * blockSizeInBytes);
            regions.bufferRowLength = bufferRowLength;
            regions.bufferImageHeight = bufferImageHeight;
            regions.imageExtent = {copyWidth, copyheight, depth};
            regions.imageOffset = {(int)dstX, (int)dstY, (int)dstZ};

        VkImageSubresourceLayers &dstSubresource = regions.imageSubresource;
        
        dstSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
        dstSubresource.layerCount = layerCount;
        dstSubresource.mipLevel = dstMipLevel;
        dstSubresource.baseArrayLayer = dstBaseArrayLayer;

        vkCmdCopyBufferToImage(
            _cmdBuf, srcBuffer->GetHandle(), 
            dstImg->GetHandle(), VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regions);

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

    
    void VulkanCommandList::CopyTextureToBuffer(
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

        _resReg.RegisterTexUsage(source,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT
        );
        _resReg.RegisterBufferUsage(destination,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT
        );
        
        _resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);

        auto srcVkTexture = PtrCast<VulkanTexture>(source.get());
        VkImage srcImage = srcVkTexture->GetHandle();

        //TODO: Transition layout if necessary
        //srcVkTexture.TransitionImageLayout(
        //    cb,
        //    srcMipLevel,
        //    1,
        //    srcBaseArrayLayer,
        //    layerCount,
        //    VkImageLayout.TransferSrcOptimal);

        VulkanBuffer* dstBuffer = PtrCast<VulkanBuffer>(destination.get());

        VkImageAspectFlags aspect = (srcVkTexture->GetDesc().usage.depthStencil)
            ? VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT
            : VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
        
        std::uint32_t mipWidth, mipHeight, mipDepth;
        Helpers::GetMipDimensions(dstDesc, dstMipLevel, mipWidth, mipHeight, mipDepth);
        std::uint32_t blockSize = Helpers::FormatHelpers::IsCompressedFormat(srcVkTexture->GetDesc().format) ? 4u : 1u;
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

        std::vector<VkBufferImageCopy> layers (layerCount);
        for(unsigned layer = 0; layer < layerCount; layer++)
        {
            //VkSubresourceLayout dstLayout = dstVkTexture.GetSubresourceLayout(
            //    dstVkTexture.CalculateSubresource(dstMipLevel, dstBaseArrayLayer + layer));

            VkBufferImageCopy &region = layers[layer];
            
            region.bufferRowLength = bufferRowLength;
            region.bufferImageHeight = bufferImageHeight;
            region.bufferOffset =  Helpers::ComputeSubresourceOffset(dstDesc, dstMipLevel, dstBaseArrayLayer)//dstLayout.offset
                + (dstZ * depthPitch)
                + (compressedDstY * rowPitch)
                + (compressedDstX * blockSizeInBytes);
            region.imageExtent = { width, height, depth };
            region.imageOffset = { (int)srcX, (int)srcY, (int)srcZ };

            VkImageSubresourceLayers& srcSubresource = region.imageSubresource;
            srcSubresource.aspectMask = aspect;
            srcSubresource.layerCount = 1;
            srcSubresource.mipLevel = srcMipLevel;
            srcSubresource.baseArrayLayer = srcBaseArrayLayer + layer;            
            
        }

        vkCmdCopyImageToBuffer(
            _cmdBuf,
            srcImage, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dstBuffer->GetHandle(), layerCount, layers.data());

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

    void VulkanCommandList::CopyBuffer(
        const sp<Buffer>& source, std::uint32_t sourceOffset,
        const sp<Buffer>& destination, std::uint32_t destinationOffset, 
        std::uint32_t sizeInBytes
    ){
        CHK_RENDERPASS_ENDED();
        _resReg.RegisterBufferUsage(source,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT
        );
        _resReg.RegisterBufferUsage(destination,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT
        );

        auto* srcVkBuffer = PtrCast<VulkanBuffer>(source.get());
        auto* dstVkBuffer = PtrCast<VulkanBuffer>(destination.get());

        VkBufferCopy region{};
        region.srcOffset = sourceOffset,
        region.dstOffset = destinationOffset,
        region.size = sizeInBytes;

        vkCmdCopyBuffer(_cmdBuf, srcVkBuffer->GetHandle(), dstVkBuffer->GetHandle(), 1, &region);

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
    }
                
    void VulkanCommandList::CopyTexture(
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

        _resReg.RegisterTexUsage(source,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT
        );
        _resReg.RegisterTexUsage(destination,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT
        );

        _resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);

        auto srcVkTexture = PtrCast<VulkanTexture>(source.get());
        auto dstVkTexture = PtrCast<VulkanTexture>(destination.get());

        VkImageSubresourceLayers srcSubresource{};
        srcSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
        srcSubresource.layerCount = layerCount;
        srcSubresource.mipLevel = srcMipLevel;
        srcSubresource.baseArrayLayer = srcBaseArrayLayer;

        VkImageSubresourceLayers dstSubresource{};
        dstSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
        dstSubresource.layerCount = layerCount;
        dstSubresource.mipLevel = dstMipLevel;
        dstSubresource.baseArrayLayer = dstBaseArrayLayer;

        VkImageCopy region {};
        region.srcOffset = { (int)srcX, (int)srcY, (int)srcZ };
        region.dstOffset = { (int)dstX, (int)dstY, (int)dstZ };
        region.srcSubresource = srcSubresource;
        region.dstSubresource = dstSubresource;
        region.extent = { width, height, depth };

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

        vkCmdCopyImage(
            _cmdBuf,
            srcVkTexture->GetHandle(),
            VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dstVkTexture->GetHandle(),
            VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region);

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

    void VulkanCommandList::GenerateMipmaps(const sp<Texture>& texture){
        CHK_RENDERPASS_ENDED();
        auto vkDev = PtrCast<VulkanDevice>(dev.get());
        auto vkTex = PtrCast<VulkanTexture>(texture.get());
        _devRes.insert(texture);

        _resReg.RegisterTexUsage(texture,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT
        );

        _resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);

        auto layerCount = vkTex->GetDesc().arrayLayers;
        if (vkTex->GetDesc().usage.cubemap) {
            layerCount *= 6;
        }

        VkImageBlit region;

        auto width = vkTex->GetDesc().width;
        auto height = vkTex->GetDesc().height;
        auto depth = vkTex->GetDesc().depth;

        VkFormatProperties vkFormatProps;
        vkGetPhysicalDeviceFormatProperties(
            vkDev->PhysicalDev(), VdToVkPixelFormat(vkTex->GetDesc().format), &vkFormatProps);
        
        VkFilter filter 
            = (vkFormatProps.optimalTilingFeatures 
                & VkFormatFeatureFlagBits::VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0
                ? VkFilter::VK_FILTER_LINEAR
                : VkFilter::VK_FILTER_NEAREST;
                
            
        for (unsigned level = 1; level < vkTex->GetDesc().mipLevels; level++) {
            //vkTex.TransitionImageLayoutNonmatching(_cb, level - 1, 1, 0, layerCount, VkImageLayout.TransferSrcOptimal);
            //vkTex.TransitionImageLayoutNonmatching(_cb, level, 1, 0, layerCount, VkImageLayout.TransferDstOptimal);

            VkImage deviceImage = vkTex->GetHandle();
            auto mipWidth = std::max(width >> 1, 1U);
            auto mipHeight = std::max(height >> 1, 1U);
            auto mipDepth = std::max(depth >> 1, 1U);

            
            region.srcSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.baseArrayLayer = 0;
            region.srcSubresource.layerCount = layerCount;
            region.srcSubresource.mipLevel = level - 1;
            region.srcOffsets[0] = {};
            region.srcOffsets[1] = { (int)width, (int)height, (int)depth };
            region.dstOffsets[0] = {};
            region.dstOffsets[1] = { (int)mipWidth, (int)mipHeight, (int)mipDepth };
             
            region.dstSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
            region.dstSubresource.baseArrayLayer = 0;
            region.dstSubresource.layerCount = layerCount;
            region.dstSubresource.mipLevel = level;

            //if (!_filters.TryGetValue(format, out VkFilter filter))
            if (level > 1) {
                //Beyond first transition, barrier needed
                VkImageMemoryBarrier memBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
                memBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                memBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                memBarrier.image = deviceImage;
                memBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                memBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                vkCmdPipelineBarrier(_cmdBuf,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &memBarrier
                );
            }
            vkCmdBlitImage(
                _cmdBuf,
                deviceImage, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                deviceImage, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                1, &region,
                filter);

            width = mipWidth;
            height = mipHeight;
            depth = mipDepth;
        }

        //if ((vkTex.Usage & TextureUsage.Sampled) != 0)
        //{
        //    vkTex.TransitionImageLayoutNonmatching(_cb, 0, vkTex.MipLevels, 0, layerCount, VkImageLayout.ShaderReadOnlyOptimal);
        //}
        _resReg.ModifyTexUsage(
            texture,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT
        );
    }

    void VulkanCommandList::PushDebugGroup(const std::string& name) {};

    void VulkanCommandList::PopDebugGroup() {};

    void VulkanCommandList::InsertDebugMarker(const std::string& name) {};


}
