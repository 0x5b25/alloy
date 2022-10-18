#include "VulkanResourceFactory.hpp"

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

    VLD_RF_FOR_EACH_RES(VK_IMPL_RF_CREATE_WITH_DESC)

    sp<Shader> VulkanResourceFactory::CreateShader(
        const Shader::Description& desc,
        const std::vector<std::uint32_t>& spv
    ){
        return VulkanShader::Make(_CreateNewDevHandle(), desc, spv);
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

    sp<Shader> VulkanResourceFactory::CreateShader(
        const Shader::Description& desc,
        const std::vector<std::uint32_t>& spv
    ) {
        return VulkanShader::Make(_CreateNewDevHandle(), desc, spv);
    }

    sp<Texture> VulkanResourceFactory::WrapNativeTexture(
        void* nativeHandle,
        const Texture::Description& description
    ){
        return VulkanTexture::WrapNative(_CreateNewDevHandle(), description, nativeHandle);
    }

    sp<TextureView> VulkanResourceFactory::CreateTextureView(
        sp<Texture>& texture,
        const TextureView::Description& description
    ){
        return VulkanTextureView::Make(_CreateNewDevHandle(), texture, description);
    }

    
    sp<CommandList> VulkanResourceFactory::CreateCommandList(){
        return VulkanCommandList::Make(_CreateNewDevHandle());
    }

    sp<Fence> VulkanResourceFactory::CreateFence(bool initialSignaled) {
        return VulkanFence::Make(_CreateNewDevHandle(), initialSignaled);
    }


} // namespace Veldrid

