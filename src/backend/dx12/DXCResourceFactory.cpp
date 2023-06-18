#include "DXCResourceFactory.hpp"

#include <cassert>

#include "DXCDevice.hpp"
//#include "VulkanPipeline.hpp"
//#include "VulkanCommandList.hpp"
//#include "VulkanTexture.hpp"
//#include "VulkanShader.hpp"
//#include "VulkanBindableResource.hpp"
//#include "VulkanSwapChain.hpp"
//#include "VulkanFramebuffer.hpp"

namespace Veldrid
{

    
#define DXC_IMPL_RF_CREATE_WITH_DESC(ResType) \
    sp<ResType> DXCResourceFactory::Create##ResType ( \
        const ResType ::Description& description \
    ){ \
        /*return DXC##ResType ::Make(_CreateNewDevHandle(), description);*/ \
        return nullptr; \
    }

    sp<DXCDevice> DXCResourceFactory::_CreateNewDevHandle(){
        assert(_dev != nullptr);
        _dev->ref();
        return sp(_dev);
    }

    VLD_RF_FOR_EACH_RES(DXC_IMPL_RF_CREATE_WITH_DESC)

    sp<Shader> DXCResourceFactory::CreateShader(
        const Shader::Description& desc,
        const std::vector<std::uint32_t>& spv
    ){
        //return VulkanShader::Make(_CreateNewDevHandle(), desc, spv);
        return nullptr;
    }

    sp<Pipeline> DXCResourceFactory::CreateGraphicsPipeline(
        const GraphicsPipelineDescription& description
    ) {
        return nullptr;
        //return VulkanGraphicsPipeline::Make(_CreateNewDevHandle(), description);
    }
        
    sp<Pipeline> DXCResourceFactory::CreateComputePipeline(
        const ComputePipelineDescription& description
    ) {
        return nullptr;
        //return VulkanComputePipeline::Make(_CreateNewDevHandle(), description);
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
        return nullptr;
       // return VulkanFence::Make(_CreateNewDevHandle(), initialSignaled);
    }

    sp<Semaphore> DXCResourceFactory::CreateDeviceSemaphore() {
        return nullptr;
        //return VulkanSemaphore::Make(_CreateNewDevHandle());
    }


} // namespace Veldrid

