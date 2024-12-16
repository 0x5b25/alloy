#include "VulkanResourceFactory.hpp"

#include <volk.h>

#include <cassert>

#include "VulkanDevice.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanCommandList.hpp"
#include "VulkanTexture.hpp"
#include "VulkanShader.hpp"
#include "VulkanBindableResource.hpp"
#include "VulkanSwapChain.hpp"
#include "VulkanFramebuffer.hpp"

namespace Veldrid
{

    
#define VK_IMPL_RF_CREATE_WITH_DESC(ResType) \
    sp<ResType> VulkanResourceFactory::Create##ResType ( \
        const ResType ::Description& description \
    ){ \
        return Vulkan##ResType ::Make(_CreateNewDevHandle(), description); \
    }

    sp<VulkanDevice> VulkanResourceFactory::_CreateNewDevHandle(){
        assert(_vkDev != nullptr);
        _vkDev->ref();
        return sp(_vkDev);
    }

    sp<Framebuffer> VulkanResourceFactory::CreateFramebuffer (
        const Framebuffer ::Description& description 
    ){ 
        return VulkanFramebuffer ::Make(_CreateNewDevHandle(), description); 
    }
    
    sp<Texture> VulkanResourceFactory::CreateTexture ( 
        const Texture ::Description& description 
    ){ 
        return VulkanTexture ::Make(_CreateNewDevHandle(), description); 
    } 
    sp<Buffer> VulkanResourceFactory::CreateBuffer ( 
        const Buffer ::Description& description 
    ){ 
        return VulkanBuffer ::Make(_CreateNewDevHandle(), description); 
    } 
    
    sp<Sampler> VulkanResourceFactory::CreateSampler ( 
        const Sampler ::Description& description 
    ){ 
        return VulkanSampler ::Make(_CreateNewDevHandle(), description); 
    } 
    
    sp<ResourceSet> VulkanResourceFactory::CreateResourceSet ( 
        const ResourceSet ::Description& description 
    ){ 
        return VulkanResourceSet ::Make(_CreateNewDevHandle(), description); 
    } 
    
    sp<ResourceLayout> VulkanResourceFactory::CreateResourceLayout ( 
        const ResourceLayout ::Description& description 
    ){ 
        return VulkanResourceLayout ::Make(_CreateNewDevHandle(), description); 
    } 
    
    sp<SwapChain> VulkanResourceFactory::CreateSwapChain (
        const SwapChain ::Description& description 
    ){ 
        return VulkanSwapChain ::Make(_CreateNewDevHandle(), description); 
    }

    sp<Shader> VulkanResourceFactory::CreateShader(
        const Shader::Description& desc,
        const std::span<std::uint8_t>& il
    ){
        return VulkanShader::Make(_CreateNewDevHandle(), desc, il);
    }

    sp<Pipeline> VulkanResourceFactory::CreateGraphicsPipeline(
        const GraphicsPipelineDescription& description
    ) {
        return VulkanGraphicsPipeline::Make(_CreateNewDevHandle(), description);
    }
        
    sp<Pipeline> VulkanResourceFactory::CreateComputePipeline(
        const ComputePipelineDescription& description
    ) {
        return VulkanComputePipeline::Make(_CreateNewDevHandle(), description);
    }

    sp<Texture> VulkanResourceFactory::WrapNativeTexture(
        void* nativeHandle,
        const Texture::Description& description
    ){
        return VulkanTexture::WrapNative(_CreateNewDevHandle(), description,
            VK_IMAGE_LAYOUT_GENERAL, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            nativeHandle);
    }

    sp<TextureView> VulkanResourceFactory::CreateTextureView(
        const sp<Texture>& texture,
        const TextureView::Description& description
    ){
        auto vkTex = PtrCast<VulkanTexture>(texture.get());
        return VulkanTextureView::Make(RefRawPtr(vkTex), description);
    }

    
    sp<CommandList> VulkanResourceFactory::CreateCommandList(){
        return VulkanCommandList::Make(_CreateNewDevHandle());
    }

    sp<Fence> VulkanResourceFactory::CreateFence() {
        return VulkanFence::Make(_CreateNewDevHandle());
    }

    sp<Semaphore> VulkanResourceFactory::CreateDeviceSemaphore() {
        return VulkanSemaphore::Make(_CreateNewDevHandle());
    }


} // namespace Veldrid

