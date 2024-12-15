#pragma once

#include "veldrid/common/Macros.h"
#include "veldrid/common/RefCnt.hpp"
#include "veldrid/ResourceFactory.hpp"

namespace Veldrid{

    class VulkanDevice;

    #define VK_DECL_RF_CREATE_WITH_DESC(ResType) \
        virtual sp<ResType> Create##ResType ( \
            const ResType ::Description& description);

    class VulkanResourceFactory : public ResourceFactory{

        DISABLE_COPY_AND_ASSIGN(VulkanResourceFactory);

        VulkanDevice* _vkDev;

        sp<VulkanDevice> _CreateNewDevHandle();

    public:
        VulkanResourceFactory(VulkanDevice* dev) : _vkDev(dev){}
        ~VulkanResourceFactory() = default;

        
        virtual void* GetHandle() const override {return nullptr;}

        
        VLD_RF_FOR_EACH_RES(VK_DECL_RF_CREATE_WITH_DESC)

        sp<Pipeline> CreateGraphicsPipeline(
            const GraphicsPipelineDescription& description) override;
        
        sp<Pipeline> CreateComputePipeline(
            const ComputePipelineDescription& description) override;

        sp<Shader> CreateShader(
            const Shader::Description& desc,
            const std::span<std::uint8_t>& il
        ) override;

        sp<Texture> WrapNativeTexture(
            void* nativeHandle,
            const Texture::Description& description) override;

        virtual sp<TextureView> CreateTextureView(
            const sp<Texture>& texture,
            const TextureView::Description& description) override;

       
        virtual sp<CommandList> CreateCommandList() override;

        virtual sp<Fence> CreateFence() override;
        virtual sp<Semaphore> CreateDeviceSemaphore() override;
    };

}
