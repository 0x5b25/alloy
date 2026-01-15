#include "DXCResourceFactory.hpp"

#include <cassert>

#include "DXCPipeline.hpp"
#include "DXCCommandList.hpp"
#include "DXCTexture.hpp"
#include "DXCShader.hpp"
#include "DXCBindableResource.hpp"
#include "DXCSwapChain.hpp"
#include "DXCFrameBuffer.hpp"
#include "DXCDevice.hpp"
#include "DXCExecuteIndirect.hpp"

namespace alloy::dxc
{

    
#define DXC_IMPL_RF_CREATE_WITH_DESC(ResType) \
    common::sp<I##ResType> DXCResourceFactory::Create##ResType ( \
        const I##ResType ::Description& description \
    ){ \
        return DXC##ResType ::Make(_CreateNewDevHandle(), description); \
        /*return nullptr;*/ \
    }

#define DXC_RF_FOR_EACH_RES(V) \
    /*V(Framebuffer)*/\
    V(Texture)\
    V(Buffer)\
    V(Sampler)\
    /*V(Shader)*/\
    V(ResourceSet)\
    V(ResourceLayout)\
    V(SwapChain)\
    V(IndirectCommandLayout)

    common::sp<DXCDevice> DXCResourceFactory::_CreateNewDevHandle(){
        //assert(_dev != nullptr);
        auto dev = GetBase();
        dev->ref();
        return common::sp<DXCDevice>(dev);
    }

    common::sp<IFrameBuffer> DXCResourceFactory::CreateFrameBuffer (
        const IFrameBuffer ::Description& description
    ){
        return nullptr;
    }

    //DXC_IMPL_RF_CREATE_WITH_DESC(Texture)

    DXC_RF_FOR_EACH_RES(DXC_IMPL_RF_CREATE_WITH_DESC)

    common::sp<IShader> DXCResourceFactory::CreateShader(
        const IShader::Description& desc,
        const std::span<std::uint8_t>& il
    ){
        //return VulkanShader::Make(_CreateNewDevHandle(), desc, spv);
        return DXCShader::Make(_CreateNewDevHandle(), desc, il);
    }

    common::sp<IGfxPipeline> DXCResourceFactory::CreateGraphicsPipeline(
        const GraphicsPipelineDescription& description
    ) {
        //return nullptr;
        return DXCGraphicsPipeline::Make(_CreateNewDevHandle(), description);
    }
        
    common::sp<IComputePipeline> DXCResourceFactory::CreateComputePipeline(
        const ComputePipelineDescription& description
    ) {
        //return nullptr;
        return DXCComputePipeline::Make(_CreateNewDevHandle(), description);
    }

    common::sp<ITexture> DXCResourceFactory::WrapNativeTexture(
        void* nativeHandle,
        const ITexture::Description& description
    ){
        return nullptr;
        //return VulkanTexture::WrapNative(_CreateNewDevHandle(), description,
        //   VK_IMAGE_LAYOUT_GENERAL, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        //    nativeHandle);
    }

    common::sp<ITextureView> DXCResourceFactory::CreateTextureView(
        const common::sp<ITexture>& texture,
        const ITextureView::Description& description
    ){
        return common::sp(new DXCTextureView(RefRawPtr(texture.get()), description));
        // auto vkTex = PtrCast<VulkanTexture>(texture.get());
        //return VulkanTextureView::Make(_CreateNewDevHandle(), RefRawPtr(vkTex), description);
    }

    
    common::sp<IRenderTarget> DXCResourceFactory::CreateRenderTarget(
        const common::sp<ITextureView>& texView
    ) {
        return DXCRenderTarget::Make(common::SPCast<DXCTextureView>(texView));
    } 

    common::sp<IEvent> DXCResourceFactory::CreateSyncEvent() {
       return common::sp(new DXCFence(GetBase()));
    }

} // namespace alloy

