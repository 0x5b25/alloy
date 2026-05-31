#include "VulkanCommandList.hpp"

#include "alloy/common/Common.hpp"
#include "alloy/Helpers.hpp"

#include "VkCommon.hpp"
#include "VkTypeCvt.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTexture.hpp"
#include "VulkanContext.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanResourceBarrier.hpp"
#include "VulkanDescriptorHeap.hpp"

#include <ranges>

namespace alloy::vk{


    void VkCmdEncBase::PushDebugGroup(const std::string& name, const Color4f& color) {
        if(!dev->GetContext().GetCaps().hasDebugUtilExt) return;

        VkDebugUtilsLabelEXT markerInfo = {};
        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        markerInfo.pLabelName = name.c_str();
        markerInfo.color[0] = color.r;
        markerInfo.color[1] = color.g;
        markerInfo.color[2] = color.b;
        markerInfo.color[3] = color.a;

        VK_INST_CALL(dev, vkCmdBeginDebugUtilsLabelEXT(
            cmdList,
            &markerInfo));
    }

    void VkCmdEncBase::PopDebugGroup() {
        if(!dev->GetContext().GetCaps().hasDebugUtilExt) return;

        VK_INST_CALL(dev, vkCmdEndDebugUtilsLabelEXT(cmdList));

    }

    void VkCmdEncBase::InsertDebugMarker(const std::string& name, const Color4f& color) {
        if(!dev->GetContext().GetCaps().hasDebugUtilExt) return;

        VkDebugUtilsLabelEXT markerInfo = {};
        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        markerInfo.pLabelName = name.c_str();
        markerInfo.color[0] = color.r;
        markerInfo.color[1] = color.g;
        markerInfo.color[2] = color.b;
        markerInfo.color[3] = color.a;

        VK_INST_CALL(dev, vkCmdInsertDebugUtilsLabelEXT(
            cmdList,
            &markerInfo));
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

    #define CHK_GFX_PIPELINE_SET()                          \
        assert(                                             \
            std::holds_alternative<VulkanGraphicsPipeline*>(currentPipeline) \
        )

    #define CHK_COMPUTE_PIPELINE_SET()                          \
        assert(                                             \
            std::holds_alternative<VulkanComputePipeline*>(currentPipeline) \
        )

    #define CHK_MESH_PIPELINE_SET()                          \
        assert(                                             \
            std::holds_alternative<VulkanMeshShaderPipeline*>(currentPipeline) \
        )

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
        //if(_currentPass != nullptr) {
        //    assert(false);
        //}
        _EndCurrentActivePass();

        VK_DEV_CALL(_dev, vkEndCommandBuffer(_cmdBuf));
    }

    void VulkanCommandList::_EndCurrentActivePass() {
        if(_currentPass) {
            EndPass();
        }
    }

    void VulkanCommandList::_BeginDummyPassIfNoActivePass() {
        if(!_currentPass) {
            //Begin a dummy pass for misc command recording
            auto dummyPass = new VkCmdEncBase(_dev.get(), _cmdBuf);
            //auto* pNewEnc = new _DXCDummyPass(_dev.get(), this);
            _passes.push_back(dummyPass);
            _currentPass = dummyPass;
        }
    }

    void VkRenderCmdEnc::SetPipeline(const common::sp<IGfxPipeline>& pipeline){
        auto vkPipeline = PtrCast<VulkanGraphicsPipeline>(pipeline.get());

        assert((VulkanPipelineBase*)vkPipeline != currentPipeline);

        //DEBUGCODE(
        //    if(!std::holds_alternative<std::monostate>(currentPipeline)) {
        //        auto ppCurrGfxPipe = std::get_if<VulkanGraphicsPipeline*>(&currentPipeline);
        //        assert(ppCurrGfxPipe != nullptr);    //Empty pipeline should be in std::monostate
        //        assert(vkPipeline != *ppCurrGfxPipe);//Shouldn't bind same pipeline multiple times
        //    }
        //);

        resources.insert(pipeline);
        VK_DEV_CALL(dev,
            vkCmdBindPipeline(cmdList,
                              VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS,
                              vkPipeline->GetHandle()));

        //Mark current pipeline
        currentPipeline = vkPipeline;
    }

    void VkRenderCmdEnc::SetPipeline(const common::sp<IMeshShaderPipeline>& pipeline) {
        auto vkPipeline = PtrCast<VulkanMeshShaderPipeline>(pipeline.get());

        assert((VulkanPipelineBase*)vkPipeline != currentPipeline);

        //DEBUGCODE(
        //    if(!std::holds_alternative<std::monostate>(currentPipeline)) {
        //        auto ppCurrPipe = std::get_if<VulkanMeshShaderPipeline*>(&currentPipeline);
        //        assert(ppCurrPipe != nullptr);    //Empty pipeline should be in std::monostate
        //        assert(vkPipeline != *ppCurrPipe);//Shouldn't bind same pipeline multiple times
        //    }
        //);

        resources.insert(pipeline);
        VK_DEV_CALL(dev, vkCmdBindPipeline(
            cmdList,
            VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS,
            vkPipeline->GetHandle()));


        //Mark current pipeline
        currentPipeline = vkPipeline;
    }

    void VkRenderCmdEnc::DispatchMesh( uint32_t groupCountX,
                                       uint32_t groupCountY,
                                       uint32_t groupCountZ )
    {
        assert(dev->GetDevCaps().SupportMeshShader());

        //CHK_MESH_PIPELINE_SET();
        VK_DEV_CALL(dev,
            vkCmdDrawMeshTasksEXT(cmdList, groupCountX, groupCountY, groupCountZ)
        );
    }

    void VkComputeCmdEnc::SetPipeline(const common::sp<IComputePipeline>& pipeline){
        auto vkPipeline = PtrCast<VulkanComputePipeline>(pipeline.get());

        assert((VulkanPipelineBase*)vkPipeline != currentPipeline);

        //DEBUGCODE(
        //    if(!std::holds_alternative<std::monostate>(currentPipeline)) {
        //        auto ppCurrCompPipe = std::get_if<VulkanComputePipeline*>(&currentPipeline);
        //        assert(ppCurrCompPipe != nullptr);    //Empty pipeline should be in std::monostate
        //        assert(vkPipeline != *ppCurrCompPipe);//Shouldn't bind same pipeline multiple times
        //    }
        //);

        resources.insert(pipeline);
        VK_DEV_CALL(dev, vkCmdBindPipeline(
            cmdList,
            VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE,
            vkPipeline->GetHandle()));


        //Mark current pipeline
        currentPipeline = vkPipeline;
    }



    void VkRenderCmdEnc::EndPass() {
        VK_DEV_CALL(dev, vkCmdEndRenderingKHR(cmdList));

        super::EndPass();
    }


    void VkRenderCmdEnc::SetVertexBuffer(
        std::uint32_t index, const common::sp<BufferRange>& buffer
    ){
        resources.insert(buffer);
        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(buffer->GetBufferObject().get());
        std::uint64_t offset64 = buffer->GetShape().GetOffsetInBytes();

        VK_DEV_CALL(dev,
            vkCmdBindVertexBuffers(cmdList, index, 1, &(vkBuffer->GetHandle()), &offset64));
    }

    void VkRenderCmdEnc::SetIndexBuffer(
        const common::sp<BufferRange>& buffer, IndexFormat format
    ){
        resources.insert(buffer);
        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(buffer->GetBufferObject().get());
        auto offset = buffer->GetShape().GetOffsetInBytes();
        VK_DEV_CALL(dev,
            vkCmdBindIndexBuffer(cmdList,
                                 vkBuffer->GetHandle(),
                                 offset,
                                 alloy::vk::VdToVkIndexFormat(format)));
    }


    void VkRenderCmdEnc::SetGraphicsResourceSet(
        const common::sp<IResourceSet>& rs
    ){
        assert(currentPipeline != nullptr);

        VkPipelineLayout pipelineLayout = currentPipeline->GetLayout();
        uint32_t resourceSetCount = currentPipeline->GetResourceSetCount();
        //if (auto* v = std::get_if<VulkanGraphicsPipeline*>(&currentPipeline)) {
        //    pipelineLayout = (*v)->GetLayout();
        //    resourceSetCount = (*v)->GetResourceSetCount();
        //} else if (auto* v = std::get_if<VulkanMeshShaderPipeline*>(&currentPipeline)) {
        //    pipelineLayout = (*v)->GetLayout();
        //    resourceSetCount = (*v)->GetResourceSetCount();
        //} else { // std::monostate
        //    //Need to bind a pipeline
        //    assert(false);
        //}

        resources.insert(rs);
        auto vkrs = PtrCast<VulkanResourceSet>(rs.get());

        auto& dss = vkrs->GetHandle();

        assert(resourceSetCount == dss.size());

        std::vector<VkDescriptorSet> descriptorSets;
        descriptorSets.reserve(resourceSetCount);
        for(auto& ds : dss)
            descriptorSets.push_back(ds.GetHandle());

        if (!descriptorSets.empty())
        {
            VK_DEV_CALL(dev,
            vkCmdBindDescriptorSets(
                cmdList,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                0,
                descriptorSets.size(),
                descriptorSets.data(),
                0,
                nullptr));
        }
        //auto& entry = _resourceSets[slot];
        //entry.isNewlyChanged = true;
        //entry.resSet = RefRawPtr(vkrs);
        //entry.offsets = dynamicOffsets;
    }

    void VkRenderCmdEnc::SetGraphicsMutableResourceSet(
        const common::sp<IMutableResourceSet>& rs
    ){
        assert(currentPipeline != nullptr);

        VkPipelineLayout pipelineLayout = currentPipeline->GetLayout();
        uint32_t resourceSetCount = currentPipeline->GetResourceSetCount();

        resources.insert(rs);
        auto vkrs = PtrCast<VulkanMutableResourceSet>(rs.get());

        auto& dss = vkrs->GetHandle();

        assert(resourceSetCount == dss.size());

        std::vector<VkDescriptorSet> descriptorSets;
        descriptorSets.reserve(resourceSetCount);
        for(auto& ds : dss)
            descriptorSets.push_back(ds.GetHandle());

        if (!descriptorSets.empty())
        {
            VK_DEV_CALL(dev,
            vkCmdBindDescriptorSets(
                cmdList,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                0,
                descriptorSets.size(),
                descriptorSets.data(),
                0,
                nullptr));
        }
    }

    void VkRenderCmdEnc::SetDescriptorHeaps(
        const common::sp<IResourceDescriptorHeap>& resourceHeap,
        const common::sp<ISamplerDescriptorHeap>& samplerHeap
    ) {
        assert(currentPipeline != nullptr);

        const auto* pRsrcHeap = PtrCast<VulkanResourceDescriptorHeap>(resourceHeap.get());
        const auto* pSampHeap = PtrCast<VulkanSamplerDescriptorHeap>(samplerHeap.get());

        VkPipelineLayout pipelineLayout = currentPipeline->GetLayout();

        std::vector<VkDescriptorSet> descriptorSets(2, VK_NULL_HANDLE);

        if(pRsrcHeap) {
            resources.insert(resourceHeap);
            auto heapSet = pRsrcHeap->GetHeapSet();
            descriptorSets[VulkanResourceLayout::T2Set_ResourceHeap] = heapSet;
        }

        if(pSampHeap) {
            resources.insert(samplerHeap);
            auto heapSet = pSampHeap->GetHeapSet();
            descriptorSets[VulkanResourceLayout::T2Set_SamplerHeap] = heapSet;
        }

        VK_DEV_CALL(dev,
            vkCmdBindDescriptorSets(
                cmdList,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                VulkanResourceLayout::T2Set_ResourceHeap,
                descriptorSets.size(),
                descriptorSets.data(),
                0,
                nullptr));
    }

    void VkRenderCmdEnc::SetPushConstants( std::uint32_t pushConstantIndex,
                                           std::span<const uint32_t> data,
                                           std::uint32_t destOffsetIn32BitValues
    ) {
        //VulkanPipelineBase* pipeline = nullptr;
        //if (auto* v = std::get_if<VulkanGraphicsPipeline*>(&currentPipeline)) {
        //    pipeline = (*v);
        //} else if (auto* v = std::get_if<VulkanMeshShaderPipeline*>(&currentPipeline)) {
        //    pipeline = (*v);
        //} else { // std::monostate
        //    //Need to bind a pipeline
        //    assert(false);
        //}
        assert(currentPipeline != nullptr);

        std::vector<uint32_t> savedData {data.begin(), data.end()};
        auto& pcs = currentPipeline->GetPushConstants();

        assert(pcs.size() > pushConstantIndex);
        auto& pc = pcs[pushConstantIndex];
        assert(pc.sizeInDwords >= destOffsetIn32BitValues + savedData.size());

        auto offset = pc.offsetInDwords;

        VK_DEV_CALL(dev,
        vkCmdPushConstants(
            cmdList,
            currentPipeline->GetLayout(),
            VK_SHADER_STAGE_ALL_GRAPHICS,
            (offset + destOffsetIn32BitValues) * 4,
            savedData.size() * 4,
            savedData.data()));
    }

    void VkComputeCmdEnc::SetComputeResourceSet(
        const common::sp<IResourceSet>& rs
    ){
        assert(currentPipeline != nullptr);
        //assert(std::holds_alternative<VulkanComputePipeline*>(currentPipeline));

        //VulkanPipelineBase* pipeline = std::get<VulkanComputePipeline*>(currentPipeline);

        resources.insert(rs);
        auto vkrs = PtrCast<VulkanResourceSet>(rs.get());

        auto& dss = vkrs->GetHandle();

        auto resourceSetCount = currentPipeline->GetResourceSetCount();
        assert(resourceSetCount == dss.size());

        std::vector<VkDescriptorSet> descriptorSets;
        descriptorSets.reserve(resourceSetCount);
        for(auto& ds : dss)
            descriptorSets.push_back(ds.GetHandle());

        if (!descriptorSets.empty())
        {
            VK_DEV_CALL(dev,
                vkCmdBindDescriptorSets(
                    cmdList,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    currentPipeline->GetLayout(),
                    0,
                    descriptorSets.size(),
                    descriptorSets.data(),
                    0,
                    nullptr));
        }


        //auto& entry = _resourceSets[slot];
        //entry.isNewlyChanged = true;
        //entry.resSet = RefRawPtr(vkrs);
        //entry.offsets = dynamicOffsets;
    }

    void VkComputeCmdEnc::SetComputeMutableResourceSet(
        const common::sp<IMutableResourceSet>& rs
    ){
        assert(currentPipeline != nullptr);

        resources.insert(rs);
        auto vkrs = PtrCast<VulkanMutableResourceSet>(rs.get());

        auto& dss = vkrs->GetHandle();

        auto resourceSetCount = currentPipeline->GetResourceSetCount();
        assert(resourceSetCount == dss.size());

        std::vector<VkDescriptorSet> descriptorSets;
        descriptorSets.reserve(resourceSetCount);
        for(auto& ds : dss)
            descriptorSets.push_back(ds.GetHandle());

        if (!descriptorSets.empty())
        {
            VK_DEV_CALL(dev,
                vkCmdBindDescriptorSets(
                    cmdList,
                    VK_PIPELINE_BIND_POINT_COMPUTE,
                    currentPipeline->GetLayout(),
                    0,
                    descriptorSets.size(),
                    descriptorSets.data(),
                    0,
                    nullptr));
        }
    }


    void VkComputeCmdEnc::SetDescriptorHeaps(
        const common::sp<IResourceDescriptorHeap>& resourceHeap,
        const common::sp<ISamplerDescriptorHeap>& samplerHeap
    ) {
        assert(currentPipeline != nullptr);

        const auto* pRsrcHeap = PtrCast<VulkanResourceDescriptorHeap>(resourceHeap.get());
        const auto* pSampHeap = PtrCast<VulkanSamplerDescriptorHeap>(samplerHeap.get());

        VkPipelineLayout pipelineLayout = currentPipeline->GetLayout();

        std::vector<VkDescriptorSet> descriptorSets(2, VK_NULL_HANDLE);

        if(pRsrcHeap) {
            resources.insert(resourceHeap);
            auto heapSet = pRsrcHeap->GetHeapSet();
            descriptorSets[VulkanResourceLayout::T2Set_ResourceHeap] = heapSet;
        }

        if(pSampHeap) {
            resources.insert(samplerHeap);
            auto heapSet = pSampHeap->GetHeapSet();
            descriptorSets[VulkanResourceLayout::T2Set_SamplerHeap] = heapSet;
        }

        VK_DEV_CALL(dev,
            vkCmdBindDescriptorSets(
                cmdList,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout,
                VulkanResourceLayout::T2Set_ResourceHeap,
                descriptorSets.size(),
                descriptorSets.data(),
                0,
                nullptr));
    }

    void VkComputeCmdEnc::SetPushConstants( std::uint32_t pushConstantIndex,
                                            std::span<const uint32_t> data,
                                           std::uint32_t destOffsetIn32BitValues
    ) {
        assert(currentPipeline != nullptr);
        //assert(std::holds_alternative<VulkanComputePipeline*>(currentPipeline));

        //VulkanPipelineBase* pipeline = std::get<VulkanComputePipeline*>(currentPipeline);

        std::vector<uint32_t> savedData {data.begin(), data.end()};
        auto& pcs = currentPipeline->GetPushConstants();
        assert(pcs.size() > pushConstantIndex);

        auto& pc = pcs[pushConstantIndex];
        assert(pc.sizeInDwords >= destOffsetIn32BitValues + savedData.size());

        auto offset = pc.offsetInDwords;

        VK_DEV_CALL(dev,
        vkCmdPushConstants(
            cmdList,
            currentPipeline->GetLayout(),
            VK_SHADER_STAGE_COMPUTE_BIT,
            (offset + destOffsetIn32BitValues) * 4,
            savedData.size() * 4,
            savedData.data()));
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

    void VkRenderCmdEnc::SetViewports(std::span<const Viewport> viewport){

        std::vector<Viewport> savedData {viewport.begin(), viewport.end()};

        {
            const auto& viewport = savedData;
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
        }
    }
    void VkRenderCmdEnc::SetFullViewport() {

        Viewport vp[1];
        bool vpIsSet = false;

        auto _SetVP = [&](const ITexture::Description& texDesc){
            vp[0] = {
                .x = 0,
                .y = 0,
                .width    = (float)texDesc.width,
                .height   = (float)texDesc.height,
                .minDepth = 0,
                .maxDepth = 1,
            };

            vpIsSet = true;
        };

        for(auto& rt : _fb.colorTargetActions) {
            auto& texDesc = rt.target->GetTextureObject()->GetDesc();
            _SetVP(texDesc);
            break;
        }

        if(!vpIsSet) {
            if(_fb.depthTargetAction) {
                auto& dt = _fb.depthTargetAction.value();
                auto& texDesc = dt.target->GetTextureObject()->GetDesc();
                _SetVP(texDesc);
                vpIsSet = true;
            }
        }

        if(!vpIsSet) {
            if(_fb.stencilTargetAction) {
                auto& st = _fb.stencilTargetAction.value();
                auto& texDesc = st.target->GetTextureObject()->GetDesc();
                _SetVP(texDesc);
                vpIsSet = true;
            }
        }
        
        assert(vpIsSet && "Pass with no render / depth / stencil targets!");

        SetViewports(vp);
    }

    void VkRenderCmdEnc::SetScissorRects(std::span<const Rect> rects)
    {

        std::vector<Rect> savedData {rects.begin(), rects.end()};

        {
            const auto& rects = savedData;
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
        }
    }

    void VkRenderCmdEnc::SetFullScissorRect() {
        Rect sr[1];
        bool srIsSet = false;

        auto _SetSR = [&](const ITexture::Description& texDesc){
            sr[0] = {
                .x = 0,
                .y = 0,
                .width  = texDesc.width,
                .height = texDesc.height,
            };

            srIsSet = true;
        };

        for(auto& rt : _fb.colorTargetActions) {
            auto& texDesc = rt.target->GetTextureObject()->GetDesc();
            _SetSR(texDesc);
            break;
        }

        if(!srIsSet) {
            if(_fb.depthTargetAction) {
                auto& dt = _fb.depthTargetAction.value();
                auto& texDesc = dt.target->GetTextureObject()->GetDesc();
                _SetSR(texDesc);
            }
        }

        if(!srIsSet) {
            if(_fb.stencilTargetAction) {
                auto& st = _fb.stencilTargetAction.value();
                auto& texDesc = st.target->GetTextureObject()->GetDesc();
                _SetSR(texDesc);
            }
        }

        assert(srIsSet && "Pass with no render / depth / stencil targets!");

        SetScissorRects(sr);
    }

    void VkRenderCmdEnc::Draw(
        std::uint32_t vertexCount, std::uint32_t instanceCount,
        std::uint32_t vertexStart, std::uint32_t instanceStart
    ){
        //PreDrawCommand();
        VK_DEV_CALL(dev,
            vkCmdDraw(cmdList, vertexCount, instanceCount, vertexStart, instanceStart));
    }

    void VkRenderCmdEnc::DrawIndexed(
        std::uint32_t indexCount, std::uint32_t instanceCount,
        std::uint32_t indexStart, std::uint32_t vertexOffset,
        std::uint32_t instanceStart
    ){
        //PreDrawCommand();
        VK_DEV_CALL(dev,
            vkCmdDrawIndexed(
                cmdList,
                indexCount,
                instanceCount,
                indexStart,
                vertexOffset,
                instanceStart));
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

        VK_DEV_CALL(dev, vkCmdDispatch(cmdList, groupCountX, groupCountY, groupCountZ));
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

#if 0
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



        {
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
        }
    }
#endif

    void VkTransferCmdEnc::CopyBufferToTexture(
        const common::sp<BufferRange>& src,
        std::uint32_t srcBytesPerRow,
        std::uint32_t srcBytesPerImage,
        const common::sp<ITextureView>& dst,
        const Point3D& dstOrigin,
        std::uint32_t dstMipLevel,
        std::uint32_t dstBaseArrayLayer,
        const Size3D& copySize
    ){

        auto* srcVkBuffer = PtrCast<VulkanBuffer>(src->GetBufferObject().get());
        
        auto dstVkView = PtrCast<VulkanTextureView>(dst.get());
        auto dstVkTexture = PtrCast<VulkanTexture>(dst->GetTextureObject().get());
        const auto& dstViewDesc = dstVkView->GetDesc();

        resources.insert(src);
        resources.insert(dst);

        auto& dstDesc = dstVkTexture->GetDesc();

        //Vulkan region bufferRowLength is in texel count
        auto sizePerPixel = FormatHelpers::GetSizeInBytes(dstDesc.format);
        auto srcTexelsPerRow = srcBytesPerRow / sizePerPixel;

        assert(srcBytesPerRow%sizePerPixel == 0);
        assert(srcBytesPerImage%srcBytesPerRow == 0);

        {

            VkBufferImageCopy regions{};
            regions.bufferOffset = src->GetShape().GetOffsetInBytes();
            regions.bufferRowLength = srcTexelsPerRow;
            regions.bufferImageHeight = srcBytesPerImage / srcBytesPerRow;
            regions.imageExtent = {copySize.width, copySize.height, copySize.depth};
            regions.imageOffset = {(int)dstOrigin.x, (int)dstOrigin.y, (int)dstOrigin.z};

            VkImageSubresourceLayers &dstSubresource = regions.imageSubresource;

            dstSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
            dstSubresource.layerCount = 1;
            dstSubresource.mipLevel = dstMipLevel + dstViewDesc.baseMipLevel;
            dstSubresource.baseArrayLayer = dstBaseArrayLayer + dstViewDesc.baseArrayLayer;

            VK_DEV_CALL(dev,vkCmdCopyBufferToImage(
                cmdList, srcVkBuffer->GetHandle(),
                dstVkTexture->GetHandle(), VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regions));

        }
    }


    void VkTransferCmdEnc::CopyTextureToBuffer(
        const common::sp<ITextureView>& src,
        const Point3D& srcOrigin,
        std::uint32_t srcMipLevel,
        std::uint32_t srcBaseArrayLayer,
        const common::sp<BufferRange>& dst,
        std::uint32_t dstBytesPerRow,
        std::uint32_t dstBytesPerImage,
        const Size3D& copySize
    ) {
        auto srcVkView = PtrCast<VulkanTextureView>(src.get());
        auto srcVkTexture = PtrCast<VulkanTexture>(src->GetTextureObject().get());
        const auto& srcViewDesc = srcVkView->GetDesc();

        auto dstBuffer = PtrCast<VulkanBuffer>(dst->GetBufferObject().get());

        resources.insert(src);
        resources.insert(dst);

        const auto& srcDesc = srcVkTexture->GetDesc();

        //Vulkan region bufferRowLength is in texel count
        auto sizePerPixel = FormatHelpers::GetSizeInBytes(srcVkTexture->GetDesc().format);
        auto dstTexelsPerRow = dstBytesPerRow / sizePerPixel;

        assert(dstTexelsPerRow%sizePerPixel == 0);
        assert(dstBytesPerImage%dstTexelsPerRow == 0);

        {


            VkImageAspectFlags aspect = (srcVkTexture->GetDesc().usage.depthStencil)
                ? VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT
                : VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;

            VkBufferImageCopy region {};

            region.bufferRowLength = sizePerPixel;
            region.bufferImageHeight = dstBytesPerImage / dstBytesPerRow;
            region.bufferOffset =  dst->GetShape().GetSizeInBytes();
            region.imageExtent = { copySize.width, copySize.height, copySize.depth };
            region.imageOffset = { (int)srcOrigin.x, (int)srcOrigin.y, (int)srcOrigin.z };

            VkImageSubresourceLayers& srcSubresource = region.imageSubresource;
            srcSubresource.aspectMask = aspect;
            srcSubresource.layerCount = 1;
            srcSubresource.mipLevel = srcMipLevel + srcViewDesc.baseMipLevel;
            srcSubresource.baseArrayLayer = srcBaseArrayLayer + srcViewDesc.baseArrayLayer;


            VK_DEV_CALL(dev,vkCmdCopyImageToBuffer(
                cmdList,
                srcVkTexture->GetHandle(), VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                dstBuffer->GetHandle(), 1, &region));

        }

    }

    void VkTransferCmdEnc::CopyBuffer(
        const common::sp<BufferRange>& source,
        const common::sp<BufferRange>& destination,
        std::uint32_t sizeInBytes
    ){
        resources.insert(source);
        resources.insert(destination);

        auto* srcVkBuffer = PtrCast<VulkanBuffer>(source->GetBufferObject().get());
        auto* dstVkBuffer = PtrCast<VulkanBuffer>(destination->GetBufferObject().get());

        {

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

        }

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
        const common::sp<ITextureView>& src,
        const Point3D& srcOrigin,
        std::uint32_t srcMipLevel,
        std::uint32_t srcBaseArrayLayer,
        const common::sp<ITextureView>& dst,
        const Point3D& dstOrigin,
        std::uint32_t dstMipLevel,
        std::uint32_t dstBaseArrayLayer,
        const Size3D& copySize
    ){

        auto srcVkView = PtrCast<VulkanTextureView>(src.get());
        auto srcVkTexture = PtrCast<VulkanTexture>(src->GetTextureObject().get());
        const auto& srcViewDesc = srcVkView->GetDesc();
        
        auto dstVkView = PtrCast<VulkanTextureView>(dst.get());
        auto dstVkTexture = PtrCast<VulkanTexture>(dst->GetTextureObject().get());
        const auto& dstViewDesc = dstVkView->GetDesc();

        resources.insert(src);
        resources.insert(dst);

        {


            VkImageSubresourceLayers srcSubresource{};
            srcSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
            srcSubresource.layerCount = 1;
            srcSubresource.mipLevel = srcMipLevel + srcViewDesc.baseMipLevel;
            srcSubresource.baseArrayLayer = srcBaseArrayLayer + srcViewDesc.baseArrayLayer;

            VkImageSubresourceLayers dstSubresource{};
            dstSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
            dstSubresource.layerCount = 1;
            dstSubresource.mipLevel = dstMipLevel + dstViewDesc.baseMipLevel;
            dstSubresource.baseArrayLayer = dstBaseArrayLayer + dstViewDesc.baseArrayLayer;

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

        }

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

    void VulkanCommandList::PushDebugGroup(const std::string& name, const Color4f& color) {
        _BeginDummyPassIfNoActivePass();
        _currentPass->PushDebugGroup(name, color);
    };

    void VulkanCommandList::PopDebugGroup() {
        _BeginDummyPassIfNoActivePass();
        _currentPass->PopDebugGroup();
    };

    void VulkanCommandList::InsertDebugMarker(const std::string& name, const Color4f& color) {
        _BeginDummyPassIfNoActivePass();
        _currentPass->InsertDebugMarker(name, color);
    };


    void VulkanCommandList::Barrier(std::span<const alloy::BarrierOp> barriers) {
        BindBarrier(this, barriers);
    }

    VkRenderCmdEnc::VkRenderCmdEnc(VulkanDevice* dev,
                    VkCommandBuffer cmdList,
                    const RenderPassAction& fb )
        : VkCmdEncBase{ dev, cmdList }
        , _fb(fb)
    {

        {
            const auto& actions = fb;
            auto _Vd2VkLoadOp = [](alloy::LoadAction load) {
                switch(load) {
                    case alloy::LoadAction::Load : return VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
                    case alloy::LoadAction::Clear : return VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
                    default: return VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                }
            };

            auto _Vd2VkStoreOp = [](alloy::StoreAction store) {
                switch(store) {
                    case alloy::StoreAction::Store : return  VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
                    default: return VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE;
                }
            };


            uint32_t width = 0, height = 0;


            unsigned colorAttachmentCount = actions.colorTargetActions.size();

            std::vector<VkRenderingAttachmentInfoKHR> colorAttachmentRefs{};
            for (auto& ctAct : actions.colorTargetActions)
            {
                //auto vkRT = common::PtrCast<VulkanRenderTarget>(ctAct.target.get());
                auto vkTexView = common::PtrCast<VulkanTextureView>(ctAct.target.get());
                auto vkColorTex = common::PtrCast<VulkanTexture>(vkTexView->GetTextureObject().get());

                auto& texDesc = vkColorTex->GetDesc();

                width = std::max(width, texDesc.width);
                height = std::max(height, texDesc.height);

                auto& colorAttachmentDesc
                    = colorAttachmentRefs.emplace_back(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR);

                colorAttachmentDesc.imageView = vkTexView->GetHandle();
                colorAttachmentDesc.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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

                if(ctAct.msaaResolveTarget) {
                    auto vkResolveTexView = common::PtrCast<VulkanTextureViewBase>(ctAct.msaaResolveTarget.get());

                    colorAttachmentDesc.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
                    colorAttachmentDesc.resolveImageView = vkResolveTexView->GetHandle();
                    colorAttachmentDesc.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                }
                else {
                    colorAttachmentDesc.resolveMode = VK_RESOLVE_MODE_NONE;
                    colorAttachmentDesc.resolveImageView = nullptr;
                    colorAttachmentDesc.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                }
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
                    const auto& dtAct = actions.depthTargetAction.value();
                    hasDepth = true;

                    //auto vkRT = common::PtrCast<VulkanRenderTarget>(actions.depthTargetAction->target.get());
                    auto vkTexView = common::PtrCast<VulkanTextureViewBase>(dtAct.target.get());
                    auto vkDepthTex = common::PtrCast<VulkanTexture>(vkTexView->GetTextureObject().get());
                    const auto& texDesc = vkDepthTex->GetDesc();
                    width = std::max(width, texDesc.width);
                    height = std::max(height, texDesc.height);

                    depthAttachment.imageView = vkTexView->GetHandle();
                    depthAttachment.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    depthAttachment.loadOp = _Vd2VkLoadOp(dtAct.loadAction);
                    depthAttachment.storeOp = _Vd2VkStoreOp(dtAct.storeAction);
                    depthAttachment.clearValue = {
                        .depthStencil = {
                            .depth = dtAct.clearDepth
                        }
                    };

                    if(dtAct.msaaResolveTarget) {
                        auto vkResolveTexView = common::PtrCast<VulkanTextureViewBase>(
                            dtAct.msaaResolveTarget.get());

                        VkResolveModeFlagBits resolveMode;
                        switch(dtAct.msaaResolveMode) {
                            case MSAADepthResolveMode::Min : resolveMode = VK_RESOLVE_MODE_MIN_BIT; break;
                            case MSAADepthResolveMode::Max : resolveMode = VK_RESOLVE_MODE_MAX_BIT; break;
                            default: resolveMode = VK_RESOLVE_MODE_NONE;
                        }

                        depthAttachment.resolveMode = resolveMode;
                        depthAttachment.resolveImageView = vkResolveTexView->GetHandle();
                        depthAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    }
                    else {
                        depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
                        depthAttachment.resolveImageView = nullptr;
                        depthAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    }
                }

                if(actions.stencilTargetAction.has_value())
                {
                    const auto& stAct = actions.stencilTargetAction.value();
                    hasStencil = true;

                    //auto vkRT = common::PtrCast<VulkanRenderTarget>(stAct.target.get());
                    auto vkTexView = common::PtrCast<VulkanTextureViewBase>(stAct.target.get());
                    auto vkStencilTex = common::PtrCast<VulkanTexture>(vkTexView->GetTextureObject().get());
                    auto& texDesc = vkStencilTex->GetDesc();
                    width = std::max(width, texDesc.width);
                    height = std::max(height, texDesc.height);

                    stencilAttachment.imageView = vkTexView->GetHandle();
                    stencilAttachment.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    stencilAttachment.loadOp = _Vd2VkLoadOp(stAct.loadAction);
                    stencilAttachment.storeOp = _Vd2VkStoreOp(stAct.storeAction);
                    stencilAttachment.clearValue = {
                        .depthStencil = {
                            .stencil = stAct.clearStencil
                        }
                    };

                    //if(stAct.msaaResolveTarget) {
                    //    auto vkResolveTexView = common::PtrCast<VulkanTextureView>(
                    //        &stAct.msaaResolveTarget->GetTexture());
                    //
                    //    stencilAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
                    //    stencilAttachment.resolveImageView = vkResolveTexView->GetHandle();
                    //    stencilAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    //}
                    //else
                    {
                        stencilAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
                        stencilAttachment.resolveImageView = nullptr;
                        stencilAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    }
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
    }

    IRenderCommandEncoder& VulkanCommandList::BeginRenderPass(
        const RenderPassAction& actions,
        const PassResourceUsage&
    ) {
        //CHK_RENDERPASS_ENDED();
        _EndCurrentActivePass();

        //auto vkfb = common::SPCast<VulkanFrameBufferBase>(fb);

        auto* pNewEnc = new VkRenderCmdEnc(_dev.get(), _cmdBuf, actions);
        //Record render pass
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;

        //vkfb->InsertCmdBeginDynamicRendering(_cmdBuf, actions);

        //assert(actions.colorTargetActions.size() == 1);

        return *pNewEnc;
    }

    IComputeCommandEncoder& VulkanCommandList::BeginComputePass(
        const PassResourceUsage&
    ) {
        //CHK_RENDERPASS_ENDED();
        _EndCurrentActivePass();

        auto* pNewEnc = new VkComputeCmdEnc(_dev.get(), _cmdBuf);
        //Record render pass
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;


        return *pNewEnc;
    }

    ITransferCommandEncoder& VulkanCommandList::BeginTransferPass() {
        //CHK_RENDERPASS_ENDED();
        _EndCurrentActivePass();

        auto* pNewEnc = new VkTransferCmdEnc(_dev.get(), _cmdBuf);
        //Record render pass
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;


        return *pNewEnc;
    }
        //virtual IBaseCommandEncoder* BeginWithBasicEncoder() = 0;

    void VulkanCommandList::EndPass() {
        CHK_RENDERPASS_BEGUN();

        _currentPass->EndPass();
        _currentPass = nullptr;
    }

    
    void VulkanCommandList::SetDebugName(const std::string& name) {
        // Check for a valid function pointer

        if (_dev->GetContext().GetCaps().hasDebugUtilExt)
        {
            VkDebugUtilsObjectNameInfoEXT nameInfo = {};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
            nameInfo.objectHandle = (uint64_t)_cmdBuf;
            nameInfo.pObjectName = name.c_str();
            VK_INST_CALL(_dev, vkSetDebugUtilsObjectNameEXT(_dev->LogicalDev(), &nameInfo));
        }

        _debugName = name;
    }


}
