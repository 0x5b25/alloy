#include "VulkanCommandList.hpp"

#include "alloy/common/Common.hpp"
#include "alloy/Helpers.hpp"

#include "VkCommon.hpp"
#include "VkTypeCvt.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTexture.hpp"
//#include "VulkanResourceBarrier.hpp"

#include <ranges>

namespace alloy::vk{
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

    void VkCmdEncBase::RegisterBufferUsage(
        VulkanBuffer* buffer,
        const VulkanCommandList::BufferState& state
    ) {

        //Find usages
        auto res = firstState.buffers.find(buffer);
        if (res == firstState.buffers.end()) {
            //This is a first time use
            firstState.buffers.insert({ buffer, state });
        }
        
        lastState.buffers[buffer] = state;
    }

    void VkCmdEncBase::RegisterTexUsage(
        VulkanTexture* tex,
        const VulkanCommandList::TextureState& state
    ) {

        //Find usages
        auto res = firstState.textures.find(tex);
        if (res == firstState.textures.end()) {
            //This is a first time use
            firstState.textures.insert({ tex, state });
        }
        
        lastState.textures[tex] = state;
    }

    
    void VkCmdEncBase::RegisterResourceSet(VulkanResourceSet* vkrs) {
        auto vkLayout = static_cast<VulkanResourceLayout*>(vkrs->GetDesc().layout.get());
        auto& boundResources = vkrs->GetDesc().boundResources;
        auto& elems = vkLayout->GetDesc().shaderResources;
        auto& bindings = vkLayout->GetBindings();


        for(auto& b : bindings) {
            for(auto& s : b.sets) {
                for(auto elemIdx : s.elementIdInList) {
                    auto& elem = elems[elemIdx];
                    auto& resource = boundResources[elemIdx];

                    //#TODO: calculate first access stages
                    VkPipelineStageFlagBits firstAccessBit = 
                        VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;

                    using _ResKind = IBindableResource::ResourceKind;

                    switch (elem.kind) {
                        case _ResKind::Texture: {
                            auto* vkTexView = PtrCast<VulkanTextureView>(boundResources[elemIdx].get());
                            auto texture = PtrCast<VulkanTexture>(vkTexView->GetTextureObject().get());
                            VulkanCommandList::TextureState state{};
                            state.access = VK_ACCESS_SHADER_READ_BIT;
                            if(elem.options.writable)
                                state.access |= VK_ACCESS_SHADER_WRITE_BIT;
                            state.stage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT; // Adjust as needed
                            state.layout = elem.options.writable ? 
                                VK_IMAGE_LAYOUT_GENERAL : 
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            RegisterTexUsage(texture, state);
                            break;
                        }
                        case _ResKind::UniformBuffer: {
                            auto* range = PtrCast<BufferRange>(boundResources[elemIdx].get());
                            auto buffer = PtrCast<VulkanBuffer>(range->GetBufferObject());
                            VulkanCommandList::BufferState state{};
                            state.access = VK_ACCESS_UNIFORM_READ_BIT;
                            state.stage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
                            RegisterBufferUsage(buffer, state);
                            break;
                        }
                        case _ResKind::StorageBuffer: {
                            auto* range = PtrCast<BufferRange>(boundResources[elemIdx].get());
                            auto buffer = PtrCast<VulkanBuffer>(range->GetBufferObject());
                            VulkanCommandList::BufferState state{};
                            state.access = VK_ACCESS_SHADER_READ_BIT;
                            state.stage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
                            RegisterBufferUsage(buffer, state);
                            break;
                        }
                        default:
                            // Samplers don't need to be registered for usage
                            break;
                    }
                }
            }
        }
    }
#if 0
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

        VkMemoryBarrier{};
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
#endif

    #define CHK_RENDERPASS_BEGUN() DEBUGCODE(assert(_currentPass != nullptr))
    #define CHK_RENDERPASS_ENDED() DEBUGCODE(assert(_currentPass == nullptr))
    #define CHK_PIPELINE_SET() DEBUGCODE(assert(_currentPipeline != nullptr))

    //sp<CommandList> VulkanCommandList::Make(const sp<VulkanDevice>& dev){
    //    auto* vkDev = PtrCast<VulkanDevice>(dev.get());
    //
    //    auto cmdPool = vkDev->GetCmdPool();
    //    auto vkCmdBuf = cmdPool->AllocateBuffer();
    //
    //    sp<IGraphicsDevice> _dev(dev);
    //    auto cmdBuf = new VulkanCommandList(_dev);
    //    cmdBuf->_cmdBuf = vkCmdBuf;
    //    cmdBuf->_cmdPool = std::move(cmdPool);
    //
    //    return sp<CommandList>(cmdBuf);
    //}

    VulkanCommandList::VulkanCommandList(
        const common::sp<VulkanDevice>& dev,
        VkCommandBuffer cmdBuf,
        common::sp<_CmdPoolContainer>&& alloc
    )
        : _dev(dev)
        , _cmdBuf(cmdBuf)
        , _cmdPool(std::move(alloc))
    { }

    VulkanCommandList::~VulkanCommandList(){
        for(auto* p : _passes) {
            delete p;
        }
        _cmdPool->FreeBuffer(_cmdBuf);
    }
     
    void VulkanCommandList::Begin(){

        for(auto* p : _passes) {
            delete p;
        }

        _passes.clear();
        _currentPass = nullptr;
        
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_DEV_CALL(_dev, vkBeginCommandBuffer(_cmdBuf, &beginInfo));

    }
    void VulkanCommandList::End(){
        if(_currentPass != nullptr) {
            assert(false);
        }

        VK_DEV_CALL(_dev, vkEndCommandBuffer(_cmdBuf));
    }

    void VkRenderCmdEnc::SetPipeline(const common::sp<IGfxPipeline>& pipeline){
        auto vkPipeline = PtrCast<VulkanGraphicsPipeline>(pipeline.get());
                
        resources.insert(pipeline);
        recordedCmds.emplace_back([this, vkPipeline](VkCommandBuffer cmdList){
            assert(vkPipeline != _currentPipeline);
            VK_DEV_CALL(dev,
                vkCmdBindPipeline(cmdList,
                                  VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  vkPipeline->GetHandle()));
            
            //Mark current pipeline
            _currentPipeline = vkPipeline;
        });
    }

    void VkComputeCmdEnc::SetPipeline(const common::sp<IComputePipeline>& pipeline){
        auto vkPipeline = PtrCast<VulkanComputePipeline>(pipeline.get());
                
        resources.insert(pipeline);
        recordedCmds.emplace_back([this, vkPipeline](VkCommandBuffer cmdList){
            assert(vkPipeline != _currentPipeline);
            VK_DEV_CALL(dev, vkCmdBindPipeline(
                cmdList, 
                VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, 
                vkPipeline->GetHandle()));


            //Mark current pipeline
            _currentPipeline = vkPipeline;
        });
    }


    
    void VkRenderCmdEnc::EndPass() {
        recordedCmds.emplace_back([this](VkCommandBuffer cmdList){
            VK_DEV_CALL(dev, vkCmdEndRenderingKHR(cmdList));
        });

        super::EndPass();
    }

    
    void VkRenderCmdEnc::SetVertexBuffer(
        std::uint32_t index, const common::sp<BufferRange>& buffer
    ){
        resources.insert(buffer);
        VulkanCommandList::BufferState state {};
        state.access = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        state.stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        RegisterBufferUsage(
            PtrCast<VulkanBuffer>(buffer->GetBufferObject()),
            state
        );
        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(buffer->GetBufferObject());
        std::uint64_t offset64 = buffer->GetShape().GetOffsetInBytes();
        
        recordedCmds.emplace_back([this, index, vkBuffer, offset64](VkCommandBuffer cmdList){
            VK_DEV_CALL(dev,
                vkCmdBindVertexBuffers(cmdList, index, 1, &(vkBuffer->GetHandle()), &offset64));
        });
    }

    void VkRenderCmdEnc::SetIndexBuffer(
        const common::sp<BufferRange>& buffer, IndexFormat format
    ){
        VulkanCommandList::BufferState state {};
        state.access = VK_ACCESS_INDEX_READ_BIT;
        state.stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        RegisterBufferUsage(
            PtrCast<VulkanBuffer>(buffer->GetBufferObject()),
            state
        );
        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(buffer->GetBufferObject());
        auto offset = buffer->GetShape().GetOffsetInBytes();
        recordedCmds.emplace_back([this, vkBuffer, offset, format](VkCommandBuffer cmdList){
            VK_DEV_CALL(dev,
                vkCmdBindIndexBuffer(cmdList,
                                     vkBuffer->GetHandle(),
                                     offset,
                                     alloy::vk::VdToVkIndexFormat(format)));
        });
    }

    
    void VkRenderCmdEnc::SetGraphicsResourceSet(
        const common::sp<IResourceSet>& rs
    ){
        resources.insert(rs);
        auto vkrs = PtrCast<VulkanResourceSet>(rs.get());

        RegisterResourceSet(vkrs);

        recordedCmds.emplace_back([this, vkrs](VkCommandBuffer cmdList){
            CHK_PIPELINE_SET();

            auto& dss = vkrs->GetHandle();

            auto pipeline = _currentPipeline;

            auto resourceSetCount = pipeline->GetResourceSetCount();
            assert(resourceSetCount == dss.size());

            std::vector<VkDescriptorSet> descriptorSets;
            descriptorSets.reserve(resourceSetCount);
            for(auto& ds : dss)
                descriptorSets.push_back(ds.GetHandle());

            if (!descriptorSets.empty())
            {
                // Flush current batch.
                VK_DEV_CALL(dev,
                vkCmdBindDescriptorSets(
                    cmdList,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->GetLayout(),
                    0,
                    descriptorSets.size(),
                    descriptorSets.data(),
                    0,
                    nullptr));
            }
        
        });
        //auto& entry = _resourceSets[slot];
        //entry.isNewlyChanged = true;
        //entry.resSet = RefRawPtr(vkrs);
        //entry.offsets = dynamicOffsets;
    }

    
    void VkRenderCmdEnc::SetPushConstants( std::uint32_t pushConstantIndex,
                                           const std::span<uint32_t>& data,
                                           std::uint32_t destOffsetIn32BitValues
    ) {
        std::vector<uint32_t> savedData {data.begin(), data.end()};
        recordedCmds.emplace_back([=, savedData = std::move(savedData)](VkCommandBuffer cmdList){
            CHK_PIPELINE_SET();
            auto pipeline = _currentPipeline;
            auto& pcs = pipeline->GetPushConstants();
            assert(pcs.size() > pushConstantIndex);

            auto& pc = pcs[pushConstantIndex];
            assert(pc.sizeInDwords >= destOffsetIn32BitValues + savedData.size());

            auto offset = pc.offsetInDwords;

            VK_DEV_CALL(dev,
            vkCmdPushConstants(
                cmdList,
                pipeline->GetLayout(),
                VK_SHADER_STAGE_ALL_GRAPHICS,
                (offset + destOffsetIn32BitValues) * 4,
                savedData.size() * 4,
                savedData.data()));
        });
    }

    void VkComputeCmdEnc::SetComputeResourceSet(
        const common::sp<IResourceSet>& rs
    ){
        
        resources.insert(rs);
        auto vkrs = PtrCast<VulkanResourceSet>(rs.get());
        
        RegisterResourceSet(vkrs);

        recordedCmds.emplace_back([this, vkrs](VkCommandBuffer cmdList){
            CHK_PIPELINE_SET();

            auto pipeline = _currentPipeline;

            auto& dss = vkrs->GetHandle();

            auto resourceSetCount = pipeline->GetResourceSetCount();
            assert(resourceSetCount == dss.size());

            std::vector<VkDescriptorSet> descriptorSets;
            descriptorSets.reserve(resourceSetCount);
            for(auto& ds : dss)
                descriptorSets.push_back(ds.GetHandle());

            if (!descriptorSets.empty())
            {
                // Flush current batch.
                VK_DEV_CALL(dev,
                    vkCmdBindDescriptorSets(
                        cmdList,
                        VK_PIPELINE_BIND_POINT_COMPUTE,
                        pipeline->GetLayout(),
                        0,
                        descriptorSets.size(),
                        descriptorSets.data(),
                        0,
                        nullptr));
            }
        });
        

        //auto& entry = _resourceSets[slot];
        //entry.isNewlyChanged = true;
        //entry.resSet = RefRawPtr(vkrs);
        //entry.offsets = dynamicOffsets;
    }

    
    
    void VkComputeCmdEnc::SetPushConstants( std::uint32_t pushConstantIndex,
                                           const std::span<uint32_t>& data,
                                           std::uint32_t destOffsetIn32BitValues
    ) {
        
        std::vector<uint32_t> savedData {data.begin(), data.end()};
        recordedCmds.emplace_back([=, savedData = std::move(savedData)](VkCommandBuffer cmdList){
            CHK_PIPELINE_SET();
            auto pipeline = _currentPipeline;
            auto& pcs = pipeline->GetPushConstants();
            assert(pcs.size() > pushConstantIndex);

            auto& pc = pcs[pushConstantIndex];
            assert(pc.sizeInDwords >= destOffsetIn32BitValues + savedData.size());

            auto offset = pc.offsetInDwords;

            VK_DEV_CALL(dev,
            vkCmdPushConstants(
                cmdList,
                pipeline->GetLayout(),
                VK_SHADER_STAGE_COMPUTE_BIT,
                (offset + destOffsetIn32BitValues) * 4,
                savedData.size() * 4,
                savedData.data()));
        });
    }

#if 0
    void VulkanCommandList::ClearColorTarget(
        std::uint32_t slot, 
        float r, float g, float b, float a
    ){
        CHK_RENDERPASS_BEGUN();

        //auto& clearValue = _currentRenderPass->clearColorTargets[slot];
        //VkClearColorValue clearColor{};
        //
        //clearColor.float32[0] = r;
        //clearColor.float32[1] = g;
        //clearColor.float32[2] = b;
        //clearColor.float32[3] = a;
        //
        //clearValue = clearColor;

        VkClearAttachment clearAttachment{};
        clearAttachment.clearValue.color.float32[0] = r;
        clearAttachment.clearValue.color.float32[1] = g;
        clearAttachment.clearValue.color.float32[2] = b;
        clearAttachment.clearValue.color.float32[3] = a;
        
        //if (_activeRenderPass != VkRenderPass.Null)
        {
            auto& fb = _currentRenderPass->fb;
            clearAttachment.colorAttachment = slot;
            clearAttachment.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
        
            auto colorTex = fb->GetDesc().colorTargets[slot].target;
            VkClearRect clearRect{};
            clearRect.baseArrayLayer = 0;
            clearRect.layerCount = 1;
            clearRect.rect = {0, 0, colorTex->GetDesc().width, colorTex->GetDesc().height};
        
            vkCmdClearAttachments(_cmdBuf, 1, &clearAttachment, 1, &clearRect);
        }
        //else
        //{
        //    // Queue up the clear value for the next RenderPass.
        //    _clearValues[index] = clearValue;
        //    _validColorClearValues[index] = true;
        //}
    }

    void VulkanCommandList::ClearDepthStencil(float depth, std::uint8_t stencil){
        CHK_RENDERPASS_BEGUN();

        //VkClearDepthStencilValue clearDS{};
        //clearDS.depth = depth;
        //clearDS.stencil = stencil;
        //_currentRenderPass->clearDSTarget = clearDS;

        VkClearAttachment clearAttachment{};
        clearAttachment.clearValue.depthStencil.depth = depth;
        clearAttachment.clearValue.depthStencil.stencil = stencil;
        
        //if (_activeRenderPass != VkRenderPass.Null)
        {
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
        //else
        //{
        //    // Queue up the clear value for the next RenderPass.
        //    _depthClearValue = clearValue;
        //}
    }
#endif

    void VkRenderCmdEnc::SetViewports(const std::span<Viewport>& viewport){
        
        std::vector<Viewport> savedData {viewport.begin(), viewport.end()};
        
        recordedCmds.emplace_back(
            [=, viewport = std::move(savedData)](VkCommandBuffer cmdList){

            if (viewport.size() == 1 || dev->GetFeatures().multipleViewports)
            {
                bool flip = dev->SupportsFlippedYDirection();
                std::vector<VkViewport> vkViewports{};
                for(auto& v : viewport) {
                    float vpY = flip
                        ? v.height + v.y
                        : v.y;
                    float vpHeight = flip
                        ? -v.height
                        : v.height;

                    auto& vkViewport = vkViewports.emplace_back();
                    vkViewport.x = v.x;
                    vkViewport.y = vpY;
                    vkViewport.width = v.width;
                    vkViewport.height = vpHeight;
                    vkViewport.minDepth = v.minDepth;
                    vkViewport.maxDepth = v.maxDepth;
                }
                VK_DEV_CALL(dev,
                    vkCmdSetViewport(cmdList, 0, vkViewports.size(), vkViewports.data()));
            }

        });
    }
    void VkRenderCmdEnc::SetFullViewports() {

        std::vector<Viewport> vps;
        //auto desc = _fb->GetDesc();

        //SetViewport(0, {0, 0, (float)fb->GetDesc().GetWidth(), (float)fb->GetDesc().GetHeight(), 0, 1});
        for (auto& ct : _fb.colorTargetActions)
        {
            auto& ctdesc = ct.target->GetTexture().GetTextureObject()->GetDesc();
            auto& vp = vps.emplace_back();
            vp.x = 0;
            vp.y = 0;
            vp.width = (float)ctdesc.width;
            vp.height = (float)ctdesc.height;
            vp.minDepth = 0;
            vp.maxDepth = ctdesc.depth;
        }


        SetViewports(vps);
    }

    void VkRenderCmdEnc::SetScissorRects(const std::span<Rect>& rects)
    {
        
        std::vector<Rect> savedData {rects.begin(), rects.end()};
        
        recordedCmds.emplace_back(
            [=, rects = std::move(savedData)](VkCommandBuffer cmdList){
            if (rects.size() == 1 || dev->GetFeatures().multipleViewports) {

                std::vector<VkRect2D> scissors;
                for(auto& rect : rects) {
                    auto& vkRect = scissors.emplace_back();
                    vkRect.offset = {(int)rect.x, (int)rect.y};
                    vkRect.extent = {rect.width, rect.height};
                //if (_scissorRects[index] != scissor)
                //{
                //    _scissorRects[index] = scissor;

                }

                VK_DEV_CALL(dev, vkCmdSetScissor(cmdList, 0, scissors.size(), scissors.data()));
                //}
            }
        });
    }

    void VkRenderCmdEnc::SetFullScissorRects() {
        std::vector<Rect> rects;
        for(auto& ct : _fb.colorTargetActions) {
            auto& ctDesc = ct.target->GetTexture().GetTextureObject()->GetDesc();
            rects.emplace_back(0, 0, ctDesc.width, ctDesc.height);
        }

        SetScissorRects(rects);
    }

    void VkRenderCmdEnc::Draw(
        std::uint32_t vertexCount, std::uint32_t instanceCount,
        std::uint32_t vertexStart, std::uint32_t instanceStart
    ){
        //PreDrawCommand();
        recordedCmds.emplace_back([=](VkCommandBuffer cmdList){
            VK_DEV_CALL(dev,
                vkCmdDraw(cmdList, vertexCount, instanceCount, vertexStart, instanceStart));
        });
    }
    
    void VkRenderCmdEnc::DrawIndexed(
        std::uint32_t indexCount, std::uint32_t instanceCount, 
        std::uint32_t indexStart, std::uint32_t vertexOffset, 
        std::uint32_t instanceStart
    ){
        //PreDrawCommand();
        recordedCmds.emplace_back([=](VkCommandBuffer cmdList){
            VK_DEV_CALL(dev,
                vkCmdDrawIndexed(
                    cmdList, 
                    indexCount, 
                    instanceCount, 
                    indexStart, 
                    vertexOffset, 
                    instanceStart));
        });
    }
#if 0
    void VulkanCommandList::DrawIndirect(
        const sp<Buffer>& indirectBuffer, 
        std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride
    ){
        _resReg.RegisterBufferUsage(indirectBuffer,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT
            );
        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        //PreDrawCommand();
        VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(indirectBuffer.get());
        vkCmdDrawIndirect(_cmdBuf, vkBuffer->GetHandle(), offset, drawCount, stride);
    }

    void VulkanCommandList::DrawIndexedIndirect(
        const sp<Buffer>& indirectBuffer, 
        std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride
    ){
        _resReg.RegisterBufferUsage(indirectBuffer,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        );
        //PreDrawCommand();

        VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(indirectBuffer.get());
        vkCmdDrawIndexedIndirect(_cmdBuf, vkBuffer->GetHandle(), offset, drawCount, stride);
    }
#endif
    

    void VkComputeCmdEnc::Dispatch(
        std::uint32_t groupCountX, std::uint32_t groupCountY, std::uint32_t groupCountZ
    ){
        
        recordedCmds.emplace_back([=](VkCommandBuffer cmdList){
            VK_DEV_CALL(dev, vkCmdDispatch(cmdList, groupCountX, groupCountY, groupCountZ));
        });
    };

#if 0
    void VulkanCommandList::DispatchIndirect(const sp<Buffer>& indirectBuffer, std::uint32_t offset) {
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

        _resReg.RegisterBufferUsage(indirectBuffer,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        );
        
        PreDispatchCommand();

        VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(indirectBuffer.get());
        vkCmdDispatchIndirect(_cmdBuf, vkBuffer->GetHandle(), offset);
    };
#endif

    void VkTransferCmdEnc::ResolveTexture(const common::sp<ITexture>& source, const common::sp<ITexture>& destination)
    {
        VulkanTexture* vkSource = PtrCast<VulkanTexture>(source.get());
        resources.insert(source);
        VulkanTexture* vkDestination = PtrCast<VulkanTexture>(destination.get());
        resources.insert(destination);

        auto resolveStage = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                /*VK_PIPELINE_STAGE_RESOLVE_BIT |*/ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        
        VulkanCommandList::TextureState sourceState{}, destinationState{};
        sourceState.access = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
        sourceState.stage = resolveStage;
        sourceState.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        RegisterTexUsage(vkSource, sourceState);

        destinationState.access = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        destinationState.stage = resolveStage;
        destinationState.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        RegisterTexUsage(vkDestination, destinationState);

        

        recordedCmds.emplace_back([=](VkCommandBuffer cmdList){
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

            VK_DEV_CALL(dev,vkCmdResolveImage(
                cmdList,
                vkSource->GetHandle(),
                VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                vkDestination->GetHandle(),
                VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &region));

            if (vkDestination->GetDesc().usage.sampled)
            {
                //vkDestination.TransitionImageLayout(_cmdBuf, 0, 1, 0, 1, VkImageLayout.ShaderReadOnlyOptimal);
            }
        });
    }

    void VkTransferCmdEnc::CopyBufferToTexture(
        const common::sp<BufferRange>& src,
        std::uint32_t srcBytesPerRow,
        std::uint32_t srcBytesPerImage,
        const common::sp<ITexture>& dst,
        const Point3D& dstOrigin,
        std::uint32_t dstMipLevel,
        std::uint32_t dstBaseArrayLayer,
        const Size3D& copySize
    ){

        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        resources.insert(src);
        resources.insert(dst);

        auto* srcBuffer = PtrCast<VulkanBuffer>(src->GetBufferObject());
        auto* dstImg = PtrCast<VulkanTexture>(dst.get());

        VulkanCommandList::BufferState srcState{};
        srcState.access = VK_ACCESS_TRANSFER_READ_BIT;
        srcState.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        RegisterBufferUsage(srcBuffer, srcState);

        VulkanCommandList::TextureState dstState{};
        dstState.access = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstState.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstState.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        RegisterTexUsage(dstImg, dstState);

        recordedCmds.emplace_back([=](VkCommandBuffer cmdList){

            VkBufferImageCopy regions{};
            regions.bufferOffset = src->GetShape().GetOffsetInBytes();
            regions.bufferRowLength = srcBytesPerRow;
            regions.bufferImageHeight = srcBytesPerImage / srcBytesPerRow;
            regions.imageExtent = {copySize.width, copySize.height, copySize.depth};
            regions.imageOffset = {(int)dstOrigin.x, (int)dstOrigin.y, (int)dstOrigin.z};

            VkImageSubresourceLayers &dstSubresource = regions.imageSubresource;

            dstSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
            dstSubresource.layerCount = 1;
            dstSubresource.mipLevel = dstMipLevel;
            dstSubresource.baseArrayLayer = dstBaseArrayLayer;

            VK_DEV_CALL(dev,vkCmdCopyBufferToImage(
                cmdList, srcBuffer->GetHandle(), 
                dstImg->GetHandle(), VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regions));
        
        });

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

    
    void VkTransferCmdEnc::CopyTextureToBuffer(
        const common::sp<ITexture>& src,
        const Point3D& srcOrigin,
        std::uint32_t srcMipLevel,
        std::uint32_t srcBaseArrayLayer,
        const common::sp<BufferRange>& dst,
        std::uint32_t dstBytesPerRow,
        std::uint32_t dstBytesPerImage,
        const Size3D& copySize
    ) {
 
        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        resources.insert(src);
        resources.insert(dst);

        
        auto srcVkTexture = PtrCast<VulkanTexture>(src.get());
        auto* dstBuffer = PtrCast<VulkanBuffer>(dst->GetBufferObject());

        VulkanCommandList::TextureState srcState{};
        srcState.access = VK_ACCESS_TRANSFER_READ_BIT;
        srcState.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        srcState.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        RegisterTexUsage(srcVkTexture, srcState);

        VulkanCommandList::BufferState dstState{};
        dstState.access = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstState.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        RegisterBufferUsage(dstBuffer, dstState);

        recordedCmds.emplace_back([=](VkCommandBuffer cmdList){

            VkImage srcImage = srcVkTexture->GetHandle();

            //TODO: Transition layout if necessary
            //srcVkTexture.TransitionImageLayout(
            //    cb,
            //    srcMipLevel,
            //    1,
            //    srcBaseArrayLayer,
            //    layerCount,
            //    VkImageLayout.TransferSrcOptimal);

            VulkanBuffer* dstBuffer = PtrCast<VulkanBuffer>(dst->GetBufferObject());

            VkImageAspectFlags aspect = (srcVkTexture->GetDesc().usage.depthStencil)
                ? VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT
                : VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;

            VkBufferImageCopy region {};

            region.bufferRowLength = dstBytesPerRow;
            region.bufferImageHeight = dstBytesPerImage / dstBytesPerRow;
            region.bufferOffset =  dst->GetShape().GetSizeInBytes();
            region.imageExtent = { copySize.width, copySize.height, copySize.depth };
            region.imageOffset = { (int)srcOrigin.x, (int)srcOrigin.y, (int)srcOrigin.z };

            VkImageSubresourceLayers& srcSubresource = region.imageSubresource;
            srcSubresource.aspectMask = aspect;
            srcSubresource.layerCount = 1;
            srcSubresource.mipLevel = srcMipLevel;
            srcSubresource.baseArrayLayer = srcBaseArrayLayer;            


            VK_DEV_CALL(dev,vkCmdCopyImageToBuffer(
                cmdList,
                srcImage, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                dstBuffer->GetHandle(), 1, &region));

        });

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

    void VkTransferCmdEnc::CopyBuffer(
        const common::sp<BufferRange>& source,
        const common::sp<BufferRange>& destination,
        std::uint32_t sizeInBytes
    ){
        resources.insert(source);
        resources.insert(destination);

        auto* srcVkBuffer = PtrCast<VulkanBuffer>(source->GetBufferObject());
        auto* dstVkBuffer = PtrCast<VulkanBuffer>(destination->GetBufferObject());

        
        VulkanCommandList::BufferState srcState{};
        srcState.access = VK_ACCESS_TRANSFER_READ_BIT;
        srcState.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        RegisterBufferUsage(srcVkBuffer, srcState);

        VulkanCommandList::BufferState dstState{};
        dstState.access = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstState.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        RegisterBufferUsage(dstVkBuffer, dstState);

        recordedCmds.emplace_back([=](VkCommandBuffer cmdList){

            VkBufferCopy region{};
            region.srcOffset = source->GetShape().GetOffsetInBytes(),
            region.dstOffset = destination->GetShape().GetOffsetInBytes(),
            region.size = sizeInBytes;

            VK_DEV_CALL(dev, 
                vkCmdCopyBuffer(
                    cmdList, 
                    srcVkBuffer->GetHandle(), 
                    dstVkBuffer->GetHandle(), 
                    1, 
                    &region));

        });

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
                
    void VkTransferCmdEnc::CopyTexture(
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

        resources.insert(src);
        resources.insert(dst);

        auto srcVkTexture = PtrCast<VulkanTexture>(src.get());
        auto dstVkTexture = PtrCast<VulkanTexture>(dst.get());
        
        VulkanCommandList::TextureState srcState{};
        srcState.access = VK_ACCESS_TRANSFER_READ_BIT;
        srcState.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        srcState.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        RegisterTexUsage(srcVkTexture, srcState);

        
        VulkanCommandList::TextureState dstState{};
        dstState.access = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstState.stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstState.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        RegisterTexUsage(dstVkTexture, dstState);

        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        

        recordedCmds.emplace_back([=](VkCommandBuffer cmdList){


            VkImageSubresourceLayers srcSubresource{};
            srcSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
            srcSubresource.layerCount = 1;
            srcSubresource.mipLevel = srcMipLevel;
            srcSubresource.baseArrayLayer = srcBaseArrayLayer;

            VkImageSubresourceLayers dstSubresource{};
            dstSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
            dstSubresource.layerCount = 1;
            dstSubresource.mipLevel = dstMipLevel;
            dstSubresource.baseArrayLayer = dstBaseArrayLayer;

            VkImageCopy region {};
            region.srcOffset = { (int)srcOrigin.x, (int)srcOrigin.y, (int)srcOrigin.z };
            region.dstOffset = { (int)dstOrigin.x, (int)dstOrigin.y, (int)dstOrigin.z };
            region.srcSubresource = srcSubresource;
            region.dstSubresource = dstSubresource;
            region.extent = { copySize.width, copySize.height, copySize.depth };

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

            VK_DEV_CALL(dev,
                vkCmdCopyImage(
                    cmdList,
                    srcVkTexture->GetHandle(),
                    VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    dstVkTexture->GetHandle(),
                    VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &region));
        
        });

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

    void VkTransferCmdEnc::GenerateMipmaps(const common::sp<ITexture>& texture){
#if 0
        auto vkTex = PtrCast<VulkanTexture>(texture.get());
        resources.insert(texture);


        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);

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
            vkDev->PhysicalDev(), alloy::VK::priv::VdToVkPixelFormat(vkTex->GetDesc().format), &vkFormatProps);
        
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
#endif
    }

    void VulkanCommandList::PushDebugGroup(const std::string& name) {};

    void VulkanCommandList::PopDebugGroup() {};

    void VulkanCommandList::InsertDebugMarker(const std::string& name) {};

    
    void VulkanCommandList::Barrier(const std::vector<alloy::BarrierDescription>& barriers) {
#if 0
        for(auto& desc : barriers) {
            if(std::holds_alternative<alloy::MemoryBarrierResource>(desc.resourceInfo)) {
            }
            else if(std::holds_alternative<alloy::BufferBarrierResource>(desc.resourceInfo)) {
                auto& barrierDesc = std::get<alloy::BufferBarrierResource>(desc.resourceInfo);
                _devRes.insert(barrierDesc.resource);
            }
            else if(std::holds_alternative<alloy::TextureBarrierResource>(desc.resourceInfo)) {
                auto& texDesc = std::get<alloy::TextureBarrierResource>(desc.resourceInfo);
                _devRes.insert(texDesc.resource);
            }
        }

        BindBarrier(this, barriers);
#endif
    }

    void VulkanCommandList::TransitionTextureToDefaultLayout(
        const std::vector<common::sp<ITexture>>& textures
    ) {
        CHK_RENDERPASS_ENDED();
        
        auto* dummyPass = new VkCmdEncBase(_dev.get(), _cmdBuf);
        //Record render pass
        _passes.push_back(dummyPass);
        _currentPass = dummyPass;

        for(auto& t : textures) {
            auto vkTex = PtrCast<VulkanTexture>(t.get());
            dummyPass->resources.insert(t);

            TextureState state {};
            state.stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
            state.access = VK_ACCESS_2_MEMORY_READ_BIT_KHR 
                         | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;
            state.layout = VK_IMAGE_LAYOUT_GENERAL;

            dummyPass->RegisterTexUsage(vkTex, state);
        }


        EndPass();
    }

    VkRenderCmdEnc::VkRenderCmdEnc(VulkanDevice* dev,
                    VkCommandBuffer cmdList,
                    const RenderPassAction& fb )
        : VkCmdEncBase{ dev, cmdList }
        , _currentPipeline(nullptr)
        , _fb(fb)
    { 
        for (auto& ctAct : fb.colorTargetActions)
        {
            auto& vkTexView = ctAct.target->GetTexture();
            auto vkColorTex = PtrCast<VulkanTexture>(vkTexView.GetTextureObject().get());
            
            VulkanCommandList::TextureState state{};
            state.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            if(ctAct.loadAction == LoadAction::Load)
                state.access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            state.stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            state.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            RegisterTexUsage(vkColorTex, state);
        };

        if (fb.depthTargetAction.has_value())
        {
            auto& vkTexView = fb.depthTargetAction->target->GetTexture();
            auto vkDepthTex = PtrCast<VulkanTexture>(vkTexView.GetTextureObject().get());

            VulkanCommandList::TextureState state{};
            state.stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT 
                        | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

            if(fb.depthTargetAction->storeAction != alloy::StoreAction::DontCare)
            {
                state.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                             | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                state.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            } else {
                state.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                state.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            }
            RegisterTexUsage(vkDepthTex, state);
        }

        if(fb.stencilTargetAction.has_value())
        {
            auto& vkTexView = fb.depthTargetAction->target->GetTexture();
            auto vkDepthTex = PtrCast<VulkanTexture>(vkTexView.GetTextureObject().get());

            VulkanCommandList::TextureState state{};
            state.stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT 
                        | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

            if(fb.depthTargetAction->storeAction != alloy::StoreAction::DontCare)
            {
                state.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                             | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                state.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            } else {
                state.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                state.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            }
            RegisterTexUsage(vkDepthTex, state);
        }

        recordedCmds.emplace_back(
            [dev, actions = fb](VkCommandBuffer cmdList) {
                auto _Vd2VkLoadOp = [](alloy::LoadAction load) {
                    switch(load) {
                        case alloy::LoadAction::Load : return VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
                        case alloy::LoadAction::Clear : return VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
                    }
                    return VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                };

                auto _Vd2VkStoreOp = [](alloy::StoreAction store) {
                    switch(store) {
                        case alloy::StoreAction::Store : return  VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
                    }
                    return VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE;
                };


                uint32_t width = 0, height = 0;


                unsigned colorAttachmentCount = actions.colorTargetActions.size();

                std::vector<VkRenderingAttachmentInfoKHR> colorAttachmentRefs{};
                for (auto& ctAct : actions.colorTargetActions)
                {
                    //auto vkRT = common::PtrCast<VulkanRenderTarget>(ctAct.target.get());
                    auto vkTexView = common::PtrCast<VulkanTextureView>(&ctAct.target->GetTexture());
                    auto vkColorTex = common::PtrCast<VulkanTexture>(vkTexView->GetTextureObject().get());

                    auto& texDesc = vkColorTex->GetDesc();

                    width = std::max(width, texDesc.width);
                    height = std::max(height, texDesc.height);

                    auto& colorAttachmentDesc 
                        = colorAttachmentRefs.emplace_back(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR);

                    colorAttachmentDesc.imageView = vkTexView->GetHandle();
                    colorAttachmentDesc.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    colorAttachmentDesc.resolveMode = VK_RESOLVE_MODE_NONE;
                    colorAttachmentDesc.resolveImageView = nullptr;
                    colorAttachmentDesc.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    colorAttachmentDesc.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
                    colorAttachmentDesc.loadOp = _Vd2VkLoadOp(ctAct.loadAction),
                    colorAttachmentDesc.storeOp = _Vd2VkStoreOp(ctAct.storeAction),
                    colorAttachmentDesc.clearValue = {.color = {
                            .float32 = {
                                ctAct.clearColor.r,
                                ctAct.clearColor.g,
                                ctAct.clearColor.b,
                                ctAct.clearColor.a
                            }
                        }};
                }

                VkRenderingAttachmentInfoKHR depthAttachment{
                    VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR
                };

                VkRenderingAttachmentInfoKHR stencilAttachment{
                    VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR
                };

                bool hasDepth = false, hasStencil = false;

                if (actions.depthTargetAction.has_value())
                {
                    hasDepth = true;

                    //auto vkRT = common::PtrCast<VulkanRenderTarget>(actions.depthTargetAction->target.get());
                    auto vkTexView = common::PtrCast<VulkanTextureView>(&actions.depthTargetAction->target->GetTexture());
                    auto vkDepthTex = common::PtrCast<VulkanTexture>(vkTexView->GetTextureObject().get());
                    auto& texDesc = vkDepthTex->GetDesc();
                    width = std::max(width, texDesc.width);
                    height = std::max(height, texDesc.height);

                    depthAttachment.imageView = vkTexView->GetHandle();
                    depthAttachment.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
                    depthAttachment.resolveImageView = nullptr;
                    depthAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    depthAttachment.loadOp = _Vd2VkLoadOp(actions.depthTargetAction->loadAction);
                    depthAttachment.storeOp = _Vd2VkStoreOp(actions.depthTargetAction->storeAction);
                    depthAttachment.clearValue = {
                        .depthStencil = {
                            .depth = actions.depthTargetAction->clearDepth
                        }
                    };
                }

                if(actions.stencilTargetAction.has_value())
                {
                    hasStencil = true;

                    //auto vkRT = common::PtrCast<VulkanRenderTarget>(actions.stencilTargetAction->target.get());
                    auto vkTexView = common::PtrCast<VulkanTextureView>(&actions.stencilTargetAction->target->GetTexture());
                    auto vkStencilTex = common::PtrCast<VulkanTexture>(vkTexView->GetTextureObject().get());
                    auto& texDesc = vkStencilTex->GetDesc();
                    width = std::max(width, texDesc.width);
                    height = std::max(height, texDesc.height);

                    stencilAttachment.imageView = vkTexView->GetHandle();
                    stencilAttachment.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    stencilAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
                    stencilAttachment.resolveImageView = nullptr;
                    stencilAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    stencilAttachment.loadOp = _Vd2VkLoadOp(actions.stencilTargetAction->loadAction);
                    stencilAttachment.storeOp = _Vd2VkStoreOp(actions.stencilTargetAction->storeAction);
                    stencilAttachment.clearValue = {
                        .depthStencil = {
                            .stencil = actions.stencilTargetAction->clearStencil
                        }
                    };
                }

                const VkRenderingInfoKHR render_info {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
                    .renderArea = VkRect2D{ {0, 0}, {width, height} },
                    .layerCount = 1,
                    .colorAttachmentCount = (uint32_t)colorAttachmentRefs.size(),
                    .pColorAttachments = colorAttachmentRefs.data(),
                    .pDepthAttachment = hasDepth ? &depthAttachment : nullptr,
                    .pStencilAttachment = hasStencil ? &stencilAttachment : nullptr,   
                };

                VK_DEV_CALL(dev, vkCmdBeginRenderingKHR(cmdList, &render_info));

            }
        );
    }

    IRenderCommandEncoder& VulkanCommandList::BeginRenderPass(const RenderPassAction& actions) {
        CHK_RENDERPASS_ENDED();
        
        //auto vkfb = common::SPCast<VulkanFrameBufferBase>(fb);
        
        auto* pNewEnc = new VkRenderCmdEnc(_dev.get(), _cmdBuf, actions);
        //Record render pass
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;

        //vkfb->InsertCmdBeginDynamicRendering(_cmdBuf, actions);

        //assert(actions.colorTargetActions.size() == 1);

        return *pNewEnc;
    }
    
    IComputeCommandEncoder& VulkanCommandList::BeginComputePass() {
        CHK_RENDERPASS_ENDED();
        
        auto* pNewEnc = new VkComputeCmdEnc(_dev.get(), _cmdBuf);
        //Record render pass
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;


        return *pNewEnc;
    }
    
    ITransferCommandEncoder& VulkanCommandList::BeginTransferPass() {
        CHK_RENDERPASS_ENDED();
        
        auto* pNewEnc = new VkTransferCmdEnc(_dev.get(), _cmdBuf);
        //Record render pass
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;


        return *pNewEnc;
    }
        //virtual IBaseCommandEncoder* BeginWithBasicEncoder() = 0;

    void VulkanCommandList::EndPass() {
        CHK_RENDERPASS_BEGUN();
        
        auto& currPassReqStates = _currentPass->firstState;
        //_requestedStates.buffers.insert(
        //    currPassReqStates.buffers.begin(),
        //    currPassReqStates.buffers.end());
        
        //_requestedStates.textures.insert(
        //    currPassReqStates.textures.begin(),
        //    currPassReqStates.textures.end());

        std::vector<VkBufferMemoryBarrier2KHR> bufferBarriers {};

        for(auto& [buffer, stateReq] : currPassReqStates.buffers) {
            //BufferState state {};
            //Search for current recorded state
            auto it = _finalStates.buffers.find(buffer);
            if(it == _finalStates.buffers.end()) {
                //All mem accesses are guaranteed finished after 
                //semaphore signaled in submission
                //No need to insert barriers if this is the first access
                _finalStates.buffers.insert({buffer, stateReq});
                continue;
            }

            auto& state = it->second;

            if(_HasAccessHarzard(state.access, stateReq.access)) {
                auto& action = bufferBarriers.emplace_back(
                    VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR
                );
                action.srcStageMask = state.stage;
                action.srcAccessMask = state.access;
                action.dstStageMask = stateReq.stage;
                action.dstAccessMask = stateReq.access;
                action.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                action.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                action.buffer = buffer->GetHandle();
                action.offset = 0;
                action.size = buffer->GetDesc().sizeInBytes;
            }

            _finalStates.buffers[buffer] = stateReq;
        }

        std::vector<VkImageMemoryBarrier2KHR> texBarriers {};

        for(auto& [texture, stateReq] : currPassReqStates.textures) {
            //TextureState state {};
            //Search for current recorded state
            auto it = _finalStates.textures.find(texture);
            if(it == _finalStates.textures.end()) {
                //All mem accesses are guaranteed finished after 
                //semaphore signaled in submission
                //No need to insert barriers if this is the first access
                _finalStates.textures.insert({texture, stateReq});
                //Request for expected texture layout
                _requestedStates.textures.insert({texture, stateReq});
                continue;
            }

            auto& state = it->second;

            if( _HasAccessHarzard(state.access, stateReq.access) ||
                state.layout != stateReq.layout) {

                auto& desc = texture->GetDesc();
                auto& action = texBarriers.emplace_back(
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR
                );
                action.srcStageMask = state.stage;
                action.srcAccessMask = state.access;
                action.dstStageMask = stateReq.stage;
                action.dstAccessMask = stateReq.access;
                action.oldLayout = state.layout;
                action.newLayout = stateReq.layout;
                action.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                action.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                action.image = texture->GetHandle();
                action.subresourceRange.baseMipLevel = 0;
                action.subresourceRange.levelCount = desc.mipLevels;
                action.subresourceRange.baseArrayLayer = 0;
                action.subresourceRange.layerCount = desc.arrayLayers;
                
                auto& aspectMask = action.subresourceRange.aspectMask;
                if (desc.usage.depthStencil) {
                    aspectMask = FormatHelpers::IsStencilFormat(desc.format)
                        ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
                        : VK_IMAGE_ASPECT_DEPTH_BIT;
                }
                else {
                    aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                }
            }
        }

        VkDependencyInfoKHR depInfo {};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
        //TODO: figure out which scope to use
        depInfo.dependencyFlags = 0;
        
        //uint32_t                         memoryBarrierCount;
        //const VkMemoryBarrier2*          pMemoryBarriers;
        depInfo.bufferMemoryBarrierCount = bufferBarriers.size();
        depInfo.pBufferMemoryBarriers = bufferBarriers.data();
        depInfo.imageMemoryBarrierCount = texBarriers.size();
        depInfo.pImageMemoryBarriers = texBarriers.data();


        _dev->GetFnTable().vkCmdPipelineBarrier2KHR(_cmdBuf, &depInfo);
        //InsertBarrier(_dev.get(), _cmdBuf, barrierActions);

        auto& currPassStates = _currentPass->lastState;
        _finalStates.SyncTo(currPassStates);

        _currentPass->EndPass();

        _currentPass = nullptr;
    }


}
