#include "VulkanResourceFactory.hpp"

#include <volk.h>

#include <cassert>

#include "VulkanDevice.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanCommandList.hpp"
#include "VulkanDescriptorHeap.hpp"
#include "VulkanTexture.hpp"
#include "VulkanShader.hpp"
#include "VulkanBindableResource.hpp"
#include "VulkanSwapChain.hpp"

namespace alloy::vk
{


#define VK_IMPL_RF_CREATE_WITH_DESC(ResType) \
    common::sp<I##ResType> VulkanResourceFactory::Create##ResType ( \
        const I##ResType ::Description& description \
    ){ \
        return Vulkan##ResType ::Make(_CreateNewDevHandle(), description); \
    }

#define VK_RF_FOR_EACH_RES(V) \
    V(Texture)\
    V(Buffer)\
    V(Sampler)\
    /*V(Shader)*/\
    V(MutableResourceSet)\
    V(ResourceSet)\
    V(ResourceLayout)\
    V(ResourceDescriptorHeap)\
    V(SamplerDescriptorHeap)\
    V(SwapChain)

    VK_RF_FOR_EACH_RES(VK_IMPL_RF_CREATE_WITH_DESC)

    common::sp<VulkanDevice> VulkanResourceFactory::_CreateNewDevHandle(){
        auto dev = GetBase();
        dev->ref();
        return common::sp(dev);
    }

    common::sp<IShader> VulkanResourceFactory::CreateShader(
        const IShader::Description& desc,
        const std::span<const std::uint8_t>& il
    ){
        return VulkanShader::Make(_CreateNewDevHandle(), desc, il);
    }

    common::sp<IGfxPipeline> VulkanResourceFactory::CreateGraphicsPipeline(
        const GraphicsPipelineDescription& description
    ) {
        return VulkanGraphicsPipeline::Make(_CreateNewDevHandle(), description);
    }

    common::sp<IComputePipeline> VulkanResourceFactory::CreateComputePipeline(
        const ComputePipelineDescription& description
    ) {
        return VulkanComputePipeline::Make(_CreateNewDevHandle(), description);
    }

    common::sp<IMeshShaderPipeline> VulkanResourceFactory::CreateMeshShaderPipeline(
        const MeshShaderPipelineDescription& description
    ) {
        return VulkanMeshShaderPipeline::Make(_CreateNewDevHandle(), description);
    }

    common::sp<ITexture> VulkanResourceFactory::WrapNativeTexture(
        void* nativeHandle,
        const ITexture::Description& description
    ){
        return VulkanTexture::WrapNative(_CreateNewDevHandle(), description,
            VK_IMAGE_LAYOUT_GENERAL, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            nativeHandle);
    }

    common::sp<ITextureView> VulkanResourceFactory::CreateTextureView(
        const common::sp<ITexture>& texture,
        const ITextureView::Description& description
    ){
        auto vkTex = PtrCast<VulkanTexture>(texture.get());
        return VulkanTextureView::Make(RefRawPtr(vkTex), description);
    }

    common::sp<IEvent> VulkanResourceFactory::CreateSyncEvent() {
       return VulkanFence::Make(_CreateNewDevHandle());
    }
} // namespace alloy
