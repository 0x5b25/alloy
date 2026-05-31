#include "TrackingCommandList.hpp"

#include "TrackingDevice.hpp"

namespace alloy::layers::AutoResourceUsageTracking {
    

    
    TrackingCommandList::TrackingCommandList(
        common::sp<TrackingDevice> dev,
        TrackingCommandQueue* cmdQ,
        common::sp<ICommandList> inner
    )
        : _dev(std::move(dev))
        , _cmdQ(cmdQ)
        , _inner(std::move(inner))
    { }

    namespace {
        std::vector<PassResourceAccess> CopyPassResources(
            const PassResourceUsage& usage
        ) {
            return { usage.resources.begin(), usage.resources.end() };
        }

        std::vector<BarrierOp> CopyPassBarriers(
            const PassResourceUsage& usage
        ) {
            return { usage.barriers.begin(), usage.barriers.end() };
        }

        PassResourceUsage MakePassResourceUsage(
            const std::vector<PassResourceAccess>& resources,
            const std::vector<BarrierOp>& barriers
        ) {
            return {
                std::span<const PassResourceAccess>(resources.data(), resources.size()),
                std::span<const BarrierOp>(barriers.data(), barriers.size())
            };
        }
    }
    
    using common::operator|;
    static const ResourceAccessMask WRITE_MASK =
        ResourceAccess::RenderTarget                 |
        ResourceAccess::UnorderedAccess              |
        ResourceAccess::DepthStencilWrite            |
        ResourceAccess::CopyDest                     |
        ResourceAccess::AccelerationStructureWrite;


    bool _IsReadAccess(const ResourceAccessMask& access) {
        return bool(WRITE_MASK & access) == false;
    }

    bool _HasAccessHarzard(
        const ResourceAccessMask& src,
        const ResourceAccessMask& dst
    ) {
        return !_IsReadAccess(src | dst);
        //find anyone with a write access
    }

    void TrackingCmdEncBase::RegisterBufferUsage(
        IBuffer* buffer,
        const TrackingCommandList::BufferState& state
    ) {

        //Find usages
        auto res = firstState.buffers.find(buffer);
        if (res == firstState.buffers.end()) {
            //This is a first time use
            firstState.buffers.insert({ buffer, state });
        }

        lastState.buffers[buffer] = state;
    }

    void TrackingCmdEncBase::RegisterTexUsage(
        TrackedTexView* tex,
        const TrackingCommandList::TextureState& state
    ) {

        //Find usages
        auto res = firstState.textures.find(tex);
        if (res == firstState.textures.end()) {
            //This is a first time use
            firstState.textures.insert({ tex, state });
        }

        lastState.textures[tex] = state;
    }


    void TrackingCmdEncBase::RegisterResourceSet(IResourceSet* rs) {
        const auto& layoutDesc = rs->GetLayout().GetDesc();

        const auto& layoutSlots = layoutDesc.shaderResources;

        for(uint32_t i = 0; i < layoutSlots.size(); ++i) {
            const auto& slot = layoutSlots[i];
            
            for(uint32_t arrIdx = 0; arrIdx < slot.bindingCount; ++arrIdx) {
                auto pBoundRes = rs->GetBoundResource(i, arrIdx);
                if(!pBoundRes) continue;

                
                using _ResKind = IBindableResource::ResourceKind;

                switch (slot.kind) {
                    case _ResKind::Texture: {
                        auto* vkTexView = PtrCast<TrackedTexView>(pBoundRes);
                        TrackingCommandList::TextureState state{};
                        state.access = ResourceAccess::ShaderResourceRead;
                        if(slot.options.writable)
                            state.access |= ResourceAccess::UnorderedAccess;
                        state.stage = PipelineStage::AllGraphics; // Adjust as needed
                        state.layout = slot.options.writable ?
                            TextureLayout::General :
                            TextureLayout::ShaderReadOnly;
                        RegisterTexUsage(vkTexView, state);
                        break;
                    }
                    case _ResKind::UniformBuffer: {
                        auto* range = PtrCast<BufferRange>(pBoundRes);
                        auto buffer = PtrCast<TrackedBuffer>(range->GetBufferObject().get());
                        TrackingCommandList::BufferState state{};
                        state.access = ResourceAccess::ConstantBufferRead;
                        state.stage = PipelineStage::AllGraphics;
                        RegisterBufferUsage(buffer, state);
                        break;
                    }
                    case _ResKind::StorageBuffer: {
                        auto* range = PtrCast<BufferRange>(pBoundRes);
                        auto buffer = PtrCast<TrackedBuffer>(range->GetBufferObject().get());
                        TrackingCommandList::BufferState state{};
                        state.access = ResourceAccess::ShaderResourceRead;
                        if(slot.options.writable)
                            state.access |= ResourceAccess::UnorderedAccess;
                        state.stage = PipelineStage::AllGraphics;
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

    void TrackingCmdEncBase::PushDebugGroup(const std::string& name, const Color4f& color) {

        recordedCmds.emplace_back([this, name, color](ICommandList* cmdList){
            cmdList->PushDebugGroup(name, color);
        });
    }

    void TrackingCmdEncBase::PopDebugGroup() {

        recordedCmds.emplace_back([this](ICommandList* cmdList){
            cmdList->PopDebugGroup();
        });

    }

    void TrackingCmdEncBase::InsertDebugMarker(const std::string& name, const Color4f& color) {
        
        recordedCmds.emplace_back([this, name, color](ICommandList* cmdList){
            cmdList->InsertDebugMarker(name, color);
        });
    }

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


    TrackingCommandList::~TrackingCommandList() {
        for(auto* p : _passes) {
            delete p;
        }
    }

    void TrackingCommandList::Begin(){

        for(auto* p : _passes) {
            delete p;
        }

        _passes.clear();
        _currentPass = nullptr;

        _inner->Begin();
    }
    void TrackingCommandList::End(){
        //if(_currentPass != nullptr) {
        //    assert(false);
        //}
        _EndCurrentActivePass();

        _inner->End();
    }

    void TrackingCommandList::_EndCurrentActivePass() {
        if(_currentPass) {
            EndPass();
        }
    }

    void TrackingCommandList::_BeginDummyPassIfNoActivePass() {
        if(!_currentPass) {
            //Begin a dummy pass for misc command recording
            auto dummyPass = new TrackingCmdEncBase(this);
            //auto* pNewEnc = new _DXCDummyPass(_dev.get(), this);
            _passes.push_back(dummyPass);
            _currentPass = dummyPass;
        }
    }

    void TrackingRndCmdEnc::SetPipeline(const common::sp<IGfxPipeline>& pipeline){

        recordedCmds.emplace_back([this, pipeline](ICommandList* cmdList){
            inner->SetPipeline(pipeline);
        });
    }

    
    void TrackingRndCmdEnc::SetPipeline(const common::sp<IMeshShaderPipeline>& pipeline){

        recordedCmds.emplace_back([this, pipeline](ICommandList* cmdList){
            inner->SetPipeline(pipeline);
        });
    }


    void TrackingRndCmdEnc::DispatchMesh( uint32_t groupCountX,
                                          uint32_t groupCountY,
                                          uint32_t groupCountZ )
    {
        //CHK_MESH_PIPELINE_SET();
        recordedCmds.emplace_back([ =, this ](ICommandList* cmdList){
            inner->DispatchMesh(groupCountX, groupCountY, groupCountZ);
        });
    }

    void TrackingCompCmdEnc::SetPipeline(const common::sp<IComputePipeline>& pipeline){
        
        recordedCmds.emplace_back([this, pipeline](ICommandList* cmdList){
            inner->SetPipeline(pipeline);
        });
    }


    void TrackingRndCmdEnc::SetVertexBuffer(
        std::uint32_t index, const common::sp<BufferRange>& buffer
    ){
        TrackingCommandList::BufferState state {};
        state.access = ResourceAccess::VertexBufferRead;
        state.stage = PipelineStage::VertexInput;
        RegisterBufferUsage(
            PtrCast<TrackedBuffer>(buffer->GetBufferObject().get()),
            state
        );

        recordedCmds.emplace_back([this, index, buffer](ICommandList* cmdList){
            inner->SetVertexBuffer(index, buffer);
        });
    }

    void TrackingRndCmdEnc::SetIndexBuffer(
        const common::sp<BufferRange>& buffer, IndexFormat format
    ){
        TrackingCommandList::BufferState state {};
        state.access = ResourceAccess::IndexBufferRead;
        state.stage = PipelineStage::VertexInput;
        RegisterBufferUsage(
            PtrCast<TrackedBuffer>(buffer->GetBufferObject().get()),
            state
        );

        
        recordedCmds.emplace_back([this, buffer, format](ICommandList* cmdList){
            inner->SetIndexBuffer(buffer, format);
        });
    }


    void TrackingRndCmdEnc::SetGraphicsResourceSet(
        const common::sp<IResourceSet>& rs
    ){

        RegisterResourceSet(rs.get());

        recordedCmds.emplace_back([this, rs](
            ICommandList* cmdList
        ){
            inner->SetGraphicsResourceSet(rs);
        });
    }


    void TrackingRndCmdEnc::SetPushConstants( std::uint32_t pushConstantIndex,
                                              std::span<const uint32_t> data,
                                              std::uint32_t destOffsetIn32BitValues
    ) {
        std::vector<uint32_t> savedData {data.begin(), data.end()};
        recordedCmds.emplace_back(
        [ =, this, savedData = std::move(savedData) ](ICommandList* cmdList){
            inner->SetPushConstants(pushConstantIndex, savedData, destOffsetIn32BitValues);
        });
    }

    void TrackingCompCmdEnc::SetComputeResourceSet(
        const common::sp<IResourceSet>& rs
    ){
        RegisterResourceSet(rs.get());

        recordedCmds.emplace_back([this, rs](
            ICommandList* cmdList
        ){
            inner->SetComputeResourceSet(rs);
        });
    }



    void TrackingCompCmdEnc::SetPushConstants( std::uint32_t pushConstantIndex,
                                           std::span<const uint32_t> data,
                                           std::uint32_t destOffsetIn32BitValues
    ) {
        std::vector<uint32_t> savedData {data.begin(), data.end()};
        recordedCmds.emplace_back(
        [ =, this, savedData = std::move(savedData) ](ICommandList* cmdList){
            inner->SetPushConstants(pushConstantIndex, savedData, destOffsetIn32BitValues);
        });
    }


    void TrackingRndCmdEnc::SetViewports(std::span<const Viewport> viewport){

        std::vector<Viewport> savedData {viewport.begin(), viewport.end()};

        recordedCmds.emplace_back([this, savedData = std::move(savedData)](auto _){
            inner->SetViewports(savedData);
        });
    }
    void TrackingRndCmdEnc::SetFullViewport() {

        recordedCmds.emplace_back([this](auto _){
            inner->SetFullViewport();
        });

    }

    void TrackingRndCmdEnc::SetScissorRects(std::span<const Rect> rects)
    {
        std::vector<Rect> savedData {rects.begin(), rects.end()};
        recordedCmds.emplace_back([this, savedData = std::move(savedData)](auto _){
            inner->SetScissorRects(savedData);
        });
    }

    void TrackingRndCmdEnc::SetFullScissorRect() {
        
        recordedCmds.emplace_back([this](auto _){
            inner->SetFullScissorRect();
        });
    }

    void TrackingRndCmdEnc::Draw(
        std::uint32_t vertexCount, std::uint32_t instanceCount,
        std::uint32_t vertexStart, std::uint32_t instanceStart
    ){
        //PreDrawCommand();
        recordedCmds.emplace_back([=](ICommandList* cmdList){
            inner->Draw(vertexCount, instanceCount, vertexStart, instanceStart);
        });
    }

    void TrackingRndCmdEnc::DrawIndexed(
        std::uint32_t indexCount, std::uint32_t instanceCount,
        std::uint32_t indexStart, std::uint32_t vertexOffset,
        std::uint32_t instanceStart
    ){
        //PreDrawCommand();
        recordedCmds.emplace_back([=](ICommandList* cmdList){
            inner->DrawIndexed(
                    indexCount,
                    instanceCount,
                    indexStart,
                    vertexOffset,
                    instanceStart);
        });
    }


    void TrackingCompCmdEnc::Dispatch(
        std::uint32_t groupCountX, std::uint32_t groupCountY, std::uint32_t groupCountZ
    ){

        recordedCmds.emplace_back([=](ICommandList* cmdList){
            inner->Dispatch(groupCountX, groupCountY, groupCountZ);
        });
    };

    void TrackingXferCmdEnc::CopyBufferToTexture(
        const common::sp<BufferRange>& src,
        std::uint32_t srcBytesPerRow,
        std::uint32_t srcBytesPerImage,
        const common::sp<ITextureView>& dst,
        const Point3D& dstOrigin,
        std::uint32_t dstMipLevel,
        std::uint32_t dstBaseArrayLayer,
        const Size3D& copySize
    ){

        auto* srcBuffer = PtrCast<TrackedBuffer>(src->GetBufferObject().get());
        auto* dstImg = PtrCast<TrackedTexView>(dst.get());

        TrackingCommandList::BufferState srcState{};
        srcState.access = ResourceAccess::CopySource;
        srcState.stage = PipelineStage::Copy;
        RegisterBufferUsage(srcBuffer, srcState);

        TrackingCommandList::TextureState dstState{};
        dstState.access = ResourceAccess::CopyDest;
        dstState.stage = PipelineStage::Copy;
        dstState.layout = TextureLayout::CopyDest;
        RegisterTexUsage(dstImg, dstState);

        recordedCmds.emplace_back([=, this](ICommandList* cmdList){

            inner->CopyBufferToTexture(
                src, srcBytesPerRow, srcBytesPerImage,
                dst, dstOrigin, dstMipLevel, dstBaseArrayLayer,
                copySize
            );
        });
    }


    void TrackingXferCmdEnc::CopyTextureToBuffer(
        const common::sp<ITextureView>& src,
        const Point3D& srcOrigin,
        std::uint32_t srcMipLevel,
        std::uint32_t srcBaseArrayLayer,
        const common::sp<BufferRange>& dst,
        std::uint32_t dstBytesPerRow,
        std::uint32_t dstBytesPerImage,
        const Size3D& copySize
    ) {

        auto srcVkTexture = PtrCast<TrackedTexView>(src.get());
        auto* dstBuffer = PtrCast<TrackedBuffer>(dst->GetBufferObject().get());

        TrackingCommandList::TextureState srcState{};
        srcState.access = ResourceAccess::CopySource;
        srcState.stage = PipelineStage::Copy;
        srcState.layout = TextureLayout::CopySource;
        RegisterTexUsage(srcVkTexture, srcState);

        TrackingCommandList::BufferState dstState{};
        dstState.access = ResourceAccess::CopyDest;
        dstState.stage = PipelineStage::Copy;
        RegisterBufferUsage(dstBuffer, dstState);

        recordedCmds.emplace_back([=, this](ICommandList* cmdList){
            
            inner->CopyTextureToBuffer(
                src, srcOrigin, srcMipLevel, srcBaseArrayLayer,
                dst, dstBytesPerRow, dstBytesPerImage,
                copySize
            );

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

    void TrackingXferCmdEnc::CopyBuffer(
        const common::sp<BufferRange>& source,
        const common::sp<BufferRange>& destination,
        std::uint32_t sizeInBytes
    ){
        auto* srcVkBuffer = PtrCast<TrackedBuffer>(source->GetBufferObject().get());
        auto* dstVkBuffer = PtrCast<TrackedBuffer>(destination->GetBufferObject().get());


        TrackingCommandList::BufferState srcState{};
        srcState.access = ResourceAccess::CopySource;
        srcState.stage = PipelineStage::Copy;
        RegisterBufferUsage(srcVkBuffer, srcState);

        TrackingCommandList::BufferState dstState{};
        dstState.access = ResourceAccess::CopyDest;
        dstState.stage = PipelineStage::Copy;
        RegisterBufferUsage(dstVkBuffer, dstState);

        recordedCmds.emplace_back([=, this](ICommandList* cmdList){
            inner->CopyBuffer(source, destination, sizeInBytes);
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

    void TrackingXferCmdEnc::CopyTexture(
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

        auto srcVkTexture = PtrCast<TrackedTexView>(src.get());
        auto dstVkTexture = PtrCast<TrackedTexView>(dst.get());

        TrackingCommandList::TextureState srcState{};
        srcState.access = ResourceAccess::CopySource;
        srcState.stage = PipelineStage::Copy;
        srcState.layout = TextureLayout::CopySource;
        RegisterTexUsage(srcVkTexture, srcState);


        TrackingCommandList::TextureState dstState{};
        dstState.access = ResourceAccess::CopyDest;
        dstState.stage = PipelineStage::Copy;
        dstState.layout = TextureLayout::CopyDest;
        RegisterTexUsage(dstVkTexture, dstState);

        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);


        recordedCmds.emplace_back([=](ICommandList* cmdList){

            inner->CopyTexture(
                src, srcOrigin, srcMipLevel, srcBaseArrayLayer,
                dst, dstOrigin, dstMipLevel, dstBaseArrayLayer,
                copySize
            );


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

    void TrackingXferCmdEnc::GenerateMipmaps(const common::sp<ITexture>& texture){
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

    void TrackingCommandList::PushDebugGroup(const std::string& name, const Color4f& color) {
        _BeginDummyPassIfNoActivePass();
        _currentPass->PushDebugGroup(name, color);
    };

    void TrackingCommandList::PopDebugGroup() {
        _BeginDummyPassIfNoActivePass();
        _currentPass->PopDebugGroup();
    };

    void TrackingCommandList::InsertDebugMarker(const std::string& name, const Color4f& color) {
        _BeginDummyPassIfNoActivePass();
        _currentPass->InsertDebugMarker(name, color);
    };

    void TrackingCommandList::TransitionTextureToDefaultLayout(
        const std::vector<common::sp<ITextureView>>& textures
    ) {
        //#TODO: revisit this function once we have unified auto state tracking
        // between dx12 & vulkan
        _EndCurrentActivePass();

        auto* dummyPass = new TrackingCmdEncBase(this);
        _passes.push_back(dummyPass);

        for(auto& t : textures) {
            _resRefs.emplace(t);
            auto vkTex = PtrCast<TrackedTexView>(t.get());

            // Effective BOTTOM_OF_PIPE bit
            TextureState state {};
            state.stage = PipelineStage::AllCommands;
            state.access = {};
            state.layout = TextureLayout::General;

            dummyPass->RegisterTexUsage(vkTex, state);
        }
    }

    TrackingRndCmdEnc::TrackingRndCmdEnc(
        TrackingCommandList* cmdList,
        const RenderPassAction& fb,
        const PassResourceUsage& usage
    )
        : TrackingCmdEncBase{ cmdList }
        , inner(nullptr)
    {

        for (auto& ctAct : fb.colorTargetActions)
        {
            auto trackedTexView = PtrCast<TrackedTexView>(ctAct.target.get());
            auto trackedTex = PtrCast<TrackedTexture>(trackedTexView->GetTextureObject().get());

            auto colorTex = trackedTex->GetInnerTexture();

            TrackingCommandList::TextureState state{};
            state.access = ResourceAccess::RenderTarget;
            //if(ctAct.loadAction != alloy::LoadAction::ReadOnly)
            //    state.access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            state.stage = PipelineStage::ColorOutput;
            state.layout = TextureLayout::ColorAttachment;
            RegisterTexUsage(trackedTexView, state);

            if(ctAct.msaaResolveTarget) {
                auto trackedResolveTexView 
                    = PtrCast<TrackedTexView>(ctAct.msaaResolveTarget.get());
                auto trackedResolveTex = PtrCast<TrackedTexture>(trackedResolveTexView->GetTextureObject().get());
                
                auto vkResolveTex = trackedResolveTex->GetInnerTexture();

                TrackingCommandList::TextureState destinationState{};
                destinationState.access  = ResourceAccess::RenderTarget;
                destinationState.stage = PipelineStage::ColorOutput;
                destinationState.layout =  TextureLayout::ColorAttachment;
                RegisterTexUsage(trackedResolveTexView, destinationState);
            }
        };

        if (fb.depthTargetAction.has_value())
        {
            const auto& dtAct = fb.depthTargetAction.value();
            auto trackedTexView = PtrCast<TrackedTexView>(dtAct.target.get());
            auto trackedTex = PtrCast<TrackedTexture>(trackedTexView->GetTextureObject().get());

            auto tex = trackedTex->GetInnerTexture();

            TrackingCommandList::TextureState state{};
            state.stage = PipelineStage::DepthStencil;

            //Always writable
            state.access = ResourceAccess::DepthStencilRead;
            state.layout = TextureLayout::DepthStencilReadOnly;
            if(dtAct.loadAction != alloy::LoadAction::ReadOnly) {
                //Either clear or load means content is readable
                state.access |= ResourceAccess::DepthStencilWrite;
                state.layout = TextureLayout::DepthStencilWrite;
            }

            RegisterTexUsage(trackedTexView, state);

            if(dtAct.msaaResolveTarget) {
                auto tResolveTexView 
                    = PtrCast<TrackedTexView>(dtAct.msaaResolveTarget.get());
                auto tResolveTex = PtrCast<TrackedTexture>(tResolveTexView->GetTextureObject().get());

                TrackingCommandList::TextureState destinationState{};
                destinationState.access =  ResourceAccess::DepthStencilWrite;
                destinationState.stage = state.stage;
                destinationState.layout = TextureLayout::DepthStencilWrite;
                RegisterTexUsage(tResolveTexView, destinationState);
            }
        }

        if(fb.stencilTargetAction.has_value())
        {
            const auto& stAct = fb.stencilTargetAction.value();
            auto trackedTexView = PtrCast<TrackedTexView>(stAct.target.get());
            auto trackedTex = PtrCast<TrackedTexture>(trackedTexView->GetTextureObject().get());

            TrackingCommandList::TextureState state{};
            state.stage = PipelineStage::DepthStencil;

            if(fb.stencilTargetAction->storeAction != alloy::StoreAction::DontCare)
            {
                state.access = ResourceAccess::DepthStencilRead | ResourceAccess::DepthStencilWrite;
                state.layout = TextureLayout::DepthStencilWrite;
            } else {
                state.access = ResourceAccess::DepthStencilRead;
                state.layout = TextureLayout::DepthStencilReadOnly;
            }
            RegisterTexUsage(trackedTexView, state);
        }

        recordedCmds.emplace_back(
            [this,
             actions = fb,
             passResources = CopyPassResources(usage),
             passBarriers = CopyPassBarriers(usage)](ICommandList* cmdList) {
                auto passUsage = MakePassResourceUsage(passResources, passBarriers);
                inner = &cmdList->BeginRenderPass(actions, passUsage);
            }
        );
    }

    TrackingCompCmdEnc::TrackingCompCmdEnc(
        TrackingCommandList* cmdList,
        const PassResourceUsage& usage
    )
        : TrackingCmdEncBase{ cmdList }
        , cmdList(cmdList)
        , inner(nullptr)
    {
        recordedCmds.emplace_back(
            [this,
             passResources = CopyPassResources(usage),
             passBarriers = CopyPassBarriers(usage)](ICommandList* cmdList) {
                auto passUsage = MakePassResourceUsage(passResources, passBarriers);
                inner = &cmdList->BeginComputePass(passUsage);
            }
        );
    }

    IRenderCommandEncoder& TrackingCommandList::BeginRenderPass(
        const RenderPassAction& actions,
        const PassResourceUsage& usage) {
        //CHK_RENDERPASS_ENDED();
        _EndCurrentActivePass();

        //auto vkfb = common::SPCast<VulkanFrameBufferBase>(fb);

        auto* pNewEnc = new TrackingRndCmdEnc(this, actions, usage);
        //Record render pass
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;

        //vkfb->InsertCmdBeginDynamicRendering(_cmdBuf, actions);

        //assert(actions.colorTargetActions.size() == 1);

        return *pNewEnc;
    }

    IComputeCommandEncoder& TrackingCommandList::BeginComputePass(
        const PassResourceUsage& usage) {
        //CHK_RENDERPASS_ENDED();
        _EndCurrentActivePass();

        auto* pNewEnc = new TrackingCompCmdEnc(this, usage);
        //Record render pass
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;


        return *pNewEnc;
    }

    ITransferCommandEncoder& TrackingCommandList::BeginTransferPass() {
        //CHK_RENDERPASS_ENDED();
        _EndCurrentActivePass();

        auto* pNewEnc = new TrackingXferCmdEnc(this);
        //Record render pass
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;


        return *pNewEnc;
    }
        //virtual IBaseCommandEncoder* BeginWithBasicEncoder() = 0;

    void TrackingCommandList::EndPass() {
        CHK_RENDERPASS_BEGUN();

        auto& currPassReqStates = _currentPass->firstState;
        //_requestedStates.buffers.insert(
        //    currPassReqStates.buffers.begin(),
        //    currPassReqStates.buffers.end());

        //_requestedStates.textures.insert(
        //    currPassReqStates.textures.begin(),
        //    currPassReqStates.textures.end());

        std::vector<BarrierOp> barriers {};

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
                barriers.emplace_back(BufferBarrierOp{
                    .buffer = alloy::BufferRange::MakeByteBuffer(common::ref_sp(buffer)),
                    .from = {
                        .stages = state.stage,
                        .access = state.access,
                    },
                    .to = {
                        .stages = stateReq.stage,
                        .access = stateReq.access,
                    }
                });
            }

            _finalStates.buffers[buffer] = stateReq;
        }

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
                barriers.emplace_back(TextureBarrierOp {
                    .texture = common::ref_sp(texture),
                    .from = {
                        .stages = state.stage,
                        .access = state.access,
                        .layout = state.layout,
                    },
                    .to = {
                        .stages = stateReq.stage,
                        .access = stateReq.access,
                        .layout = stateReq.layout,
                    },
                });
            }
        }

        if(!barriers.empty())
            _inner->Barrier(barriers);

        auto& currPassStates = _currentPass->lastState;
        _finalStates.SyncTo(currPassStates);

        _currentPass->EndPass();

        _currentPass = nullptr;
    }


}
