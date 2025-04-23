#include "MetalResourceFactory.h"

#include <cassert>

#include "MetalPipeline.h"
#include "MetalCommandList.h"
#include "MetalTexture.h"
#include "MetalShader.h"
#include "MetalBindableResource.h"
#include "MetalSwapChain.h"
//#include "MetalFramebuffer.h"
#include "MetalDevice.h"

namespace alloy::mtl {

    
#define MTL_IMPL_RF_CREATE_WITH_DESC(ResType) \
    common::sp<I##ResType> MetalResourceFactory::Create##ResType ( \
        const I##ResType ::Description& description \
    ){ \
        return Metal##ResType ::Make(_CreateNewDevHandle(), description); \
        /*return nullptr;*/ \
    }

#define MTL_RF_FOR_EACH_RES(V) \
    /*V(Framebuffer)*/\
    V(Texture)\
    V(Buffer)\
    V(Sampler)\
    /*V(Shader)*/\
    V(ResourceSet)\
    V(ResourceLayout)\
    V(SwapChain)

    common::sp<MetalDevice> MetalResourceFactory::_CreateNewDevHandle(){
        //assert(_dev != nullptr);
        auto dev = GetBase();
        dev->ref();
        return common::sp<MetalDevice>(dev);
    }

    
    //void* MetalResourceFactory::GetHandle() const {
    //    //auto dev = GetBase();
    //    return GetBase()->GetNativeHandle();
    //}

    common::sp<IFrameBuffer> MetalResourceFactory::CreateFrameBuffer (
        const IFrameBuffer ::Description& description
    ){
        return nullptr;
    }

    //DXC_IMPL_RF_CREATE_WITH_DESC(Texture)

    MTL_RF_FOR_EACH_RES(MTL_IMPL_RF_CREATE_WITH_DESC)

    common::sp<IShader> MetalResourceFactory::CreateShader(
        const IShader::Description& desc,
        const std::span<std::uint8_t>& il
    ){
        //return VulkanShader::Make(_CreateNewDevHandle(), desc, spv);
        return MetalShader::MakeFromDXIL(_CreateNewDevHandle(), desc, il);
    }

    common::sp<IGfxPipeline> MetalResourceFactory::CreateGraphicsPipeline(
        const GraphicsPipelineDescription& description
    ) {
        //return nullptr;
        return MetalGfxPipeline::Make(_CreateNewDevHandle(), description);
    }
        
    common::sp<IComputePipeline> MetalResourceFactory::CreateComputePipeline(
        const ComputePipelineDescription& description
    ) {
        //return nullptr;
        return MetalComputePipeline::Make(_CreateNewDevHandle(), description);
    }

    common::sp<ITexture> MetalResourceFactory::WrapNativeTexture(
        void* nativeHandle,
        const ITexture::Description& description
    ){
        //return nullptr;
        return MetalTexture::WrapNative(_CreateNewDevHandle(), description, (id<MTLTexture>)nativeHandle);
    }

    common::sp<ITextureView> MetalResourceFactory::CreateTextureView(
        const common::sp<ITexture>& texture,
        const ITextureView::Description& description
    ){
        return common::sp(new MetalTextureView(RefRawPtr(texture.get()), description));
        // auto vkTex = PtrCast<VulkanTexture>(texture.get());
        //return VulkanTextureView::Make(_CreateNewDevHandle(), RefRawPtr(vkTex), description);
    }


    common::sp<IRenderTarget> MetalResourceFactory::CreateRenderTarget(
        const common::sp<ITextureView>& texView
    ) {
        return MetalRenderTarget::Make(common::SPCast<MetalTextureView>(texView));
    }

    common::sp<IEvent> MetalResourceFactory::CreateSyncEvent() {
        return MetalEvent::Make(_CreateNewDevHandle());
    }

} // namespace alloy


