#include "TrackingResourceFactory.hpp"

#include "TrackingDevice.hpp"
#include "TrackedResource.hpp"

namespace alloy::layers::AutoResourceUsageTracking {

    //Forward decl to instantiate function impl
    class TrackingDevice;
    template class TrackingResourceFactory<TrackingDevice>;

    using TFactory = TrackingResourceFactory<TrackingDevice>;
    
#define T_IMPL_RF_CREATE_WITH_DESC(ResType) \
    template<> common::sp<I##ResType> TFactory::Create##ResType ( \
        const I##ResType ::Description& description \
    ){ \
        return GetBase()->GetInner()->GetResourceFactory().Create##ResType(description); \
    }

#define T_RF_FOR_EACH_RES(V) \
    /*V(Framebuffer)*/\
    V(Texture)\
    V(Buffer)\
    V(Sampler)\
    /*V(Shader)*/\
    V(ResourceSet)\
    V(MutableResourceSet)\
    V(ResourceLayout)


    //DXC_IMPL_RF_CREATE_WITH_DESC(Texture)

    T_RF_FOR_EACH_RES(T_IMPL_RF_CREATE_WITH_DESC)

    //common::sp<IMutableResourceSet> TFactory::CreateMutableResourceSet(
    //    const IMutableResourceSet::Description& description
    //) {
    //    return DXCMutableResourceSet::Make(_CreateNewDevHandle(), description);
    //}

    template<>
    common::sp<IShader> TFactory::CreateShader(
        const IShader::Description& desc,
        const std::span<const std::uint8_t>& il
    ){
        return GetBase()->GetInner()->GetResourceFactory().CreateShader(desc, il);
    }

    template<>
    common::sp<IGfxPipeline> TFactory::CreateGraphicsPipeline(
        const GraphicsPipelineDescription& description
    ) {
        return GetBase()->GetInner()->GetResourceFactory().CreateGraphicsPipeline(description);
    }
        
    template<>
    common::sp<IComputePipeline> TFactory::CreateComputePipeline(
        const ComputePipelineDescription& description
    ) {
        return GetBase()->GetInner()->GetResourceFactory().CreateComputePipeline(description);
    }

    template<>
    common::sp<IMeshShaderPipeline> TFactory::CreateMeshShaderPipeline(
        const MeshShaderPipelineDescription& description
    ) {
        return GetBase()->GetInner()->GetResourceFactory().CreateMeshShaderPipeline(description);
    }

    template<>
    common::sp<ITexture> TFactory::WrapNativeTexture(
        void* nativeHandle,
        const ITexture::Description& description
    ){
        return nullptr;
        //return VulkanTexture::WrapNative(_CreateNewDevHandle(), description,
        //   VK_IMAGE_LAYOUT_GENERAL, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        //    nativeHandle);
    }

    template<>
    common::sp<ITextureView> TFactory::CreateTextureView(
        const common::sp<ITexture>& texture,
        const ITextureView::Description& description
    ){
        auto pProxyTex = common::SPCast<TrackedTexture>(texture);
        auto texView = GetBase()->GetInner()->GetResourceFactory().CreateTextureView(
            pProxyTex->GetInnerTexture(), description
        );

        return common::sp(new TrackedTexView(std::move(pProxyTex), std::move(texView)));
    }

 
   
    template<>
    common::sp<ISwapChain> TFactory::CreateSwapChain(const ISwapChain::Description& desc) {
       auto sc = GetBase()->GetInner()->GetResourceFactory().CreateSwapChain(desc);

       return common::sp{ new TrackedSwapChain( common::ref_sp(GetBase()), sc ) };
    }

    template<>
    common::sp<IEvent> TFactory::CreateSyncEvent() {
       return GetBase()->GetInner()->GetResourceFactory().CreateSyncEvent();
    }
}
