#pragma once

#include "alloy/common/Macros.h"
#include "alloy/common/RefCnt.hpp"
#include "alloy/ResourceFactory.hpp"

namespace alloy::vk{

    class VulkanDevice;

    #define VK_DECL_RF_CREATE_WITH_DESC(ResType) \
        virtual common::sp<I##ResType> Create##ResType ( \
            const I##ResType ::Description& description) override;

    template<class Base>
    class VulkanResourceFactoryThunk : public ResourceFactory{

    protected:
        const Base* GetBase() const { return static_cast<const Base*>(this); }
        Base* GetBase() { return static_cast<Base*>(this); }

    };

    class VulkanResourceFactory : public VulkanResourceFactoryThunk<VulkanDevice>{

        DISABLE_COPY_AND_ASSIGN(VulkanResourceFactory);

        common::sp<VulkanDevice> _CreateNewDevHandle();

    public:
        VulkanResourceFactory() = default;
        ~VulkanResourceFactory() = default;

        
        //virtual void* GetHandle() const override {return nullptr;}

        
        VLD_RF_FOR_EACH_RES(VK_DECL_RF_CREATE_WITH_DESC)

        common::sp<IGfxPipeline> CreateGraphicsPipeline(
            const GraphicsPipelineDescription& description) override;
        
        common::sp<IComputePipeline> CreateComputePipeline(
            const ComputePipelineDescription& description) override;

        common::sp<IShader> CreateShader(
            const IShader::Description& desc,
            const std::span<std::uint8_t>& spv
        ) override;

        common::sp<ITexture> WrapNativeTexture(
            void* nativeHandle,
            const ITexture::Description& description) override;

        virtual common::sp<ITextureView> CreateTextureView(
            const common::sp<ITexture>& texture,
            const ITextureView::Description& description) override;

            
        virtual common::sp<IRenderTarget> CreateRenderTarget(
            const common::sp<ITextureView>& texView) override;

        //virtual sp<Fence> CreateFence() override;
        virtual common::sp<IEvent> CreateSyncEvent() override;
    };

}
