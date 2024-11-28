#include "DXCResourceFactory.hpp"

#include <cassert>

#include "DXCDevice.hpp"
#include "DXCPipeline.hpp"
//#include "VulkanCommandList.hpp"
#include "DXCTexture.hpp"
#include "DXCShader.hpp"
#include "DXCBindableResource.hpp"
//#include "VulkanSwapChain.hpp"
//#include "VulkanFramebuffer.hpp"

namespace Veldrid
{

    
#define DXC_IMPL_RF_CREATE_WITH_DESC(ResType) \
    sp<ResType> DXCResourceFactory::Create##ResType ( \
        const ResType ::Description& description \
    ){ \
        return DXC##ResType ::Make(_CreateNewDevHandle(), description); \
        /*return nullptr;*/ \
    }

#define DXC_RF_FOR_EACH_RES(V) \
    /*V(Framebuffer)*/\
    V(Texture)\
    V(Buffer)\
    /*V(Sampler)*/\
    /*V(Shader)*/\
    V(ResourceSet)\
    V(ResourceLayout)\
    /*V(SwapChain)*/

    sp<DXCDevice> DXCResourceFactory::_CreateNewDevHandle(){
        //assert(_dev != nullptr);
        auto dev = GetBase();
        dev->ref();
        return sp<DXCDevice>(dev);
    }

    sp<Sampler> DXCResourceFactory::CreateSampler (
        const Sampler ::Description& description
    ){
        return sp(new Sampler(description));
    }

    sp<Framebuffer> DXCResourceFactory::CreateFramebuffer (
        const Framebuffer ::Description& description
    ){
        return nullptr;
    }

    sp<SwapChain> DXCResourceFactory::CreateSwapChain (
        const SwapChain ::Description& description
    ){
        return nullptr;
    }

    //DXC_IMPL_RF_CREATE_WITH_DESC(Texture)

    DXC_RF_FOR_EACH_RES(DXC_IMPL_RF_CREATE_WITH_DESC)

    sp<Shader> DXCResourceFactory::CreateShader(
        const Shader::Description& desc,
        const std::span<std::uint8_t>& il
    ){
        //return VulkanShader::Make(_CreateNewDevHandle(), desc, spv);
        return DXCShader::Make(_CreateNewDevHandle(), desc, il);
    }

    sp<Pipeline> DXCResourceFactory::CreateGraphicsPipeline(
        const GraphicsPipelineDescription& description
    ) {
        //return nullptr;
        return DXCGraphicsPipeline::Make(_CreateNewDevHandle(), description);
    }
        
    sp<Pipeline> DXCResourceFactory::CreateComputePipeline(
        const ComputePipelineDescription& description
    ) {
        //return nullptr;
        return DXCComputePipeline::Make(_CreateNewDevHandle(), description);
    }

    sp<Texture> DXCResourceFactory::WrapNativeTexture(
        void* nativeHandle,
        const Texture::Description& description
    ){
        return nullptr;
        //return VulkanTexture::WrapNative(_CreateNewDevHandle(), description,
        //   VK_IMAGE_LAYOUT_GENERAL, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        //    nativeHandle);
    }

    sp<TextureView> DXCResourceFactory::CreateTextureView(
        const sp<Texture>& texture,
        const TextureView::Description& description
    ){
        return nullptr;
       // auto vkTex = PtrCast<VulkanTexture>(texture.get());
        //return VulkanTextureView::Make(_CreateNewDevHandle(), RefRawPtr(vkTex), description);
    }

    
    sp<CommandList> DXCResourceFactory::CreateCommandList(){
        return nullptr;
        //return VulkanCommandList::Make(_CreateNewDevHandle());
    }

    sp<Fence> DXCResourceFactory::CreateFence(bool initialSignaled) {
       return DXCVLDFence::Make(_CreateNewDevHandle(), initialSignaled);
    }

    sp<Semaphore> DXCResourceFactory::CreateDeviceSemaphore() {
        return DXCVLDSemaphore::Make(_CreateNewDevHandle());
    }


} // namespace Veldrid

