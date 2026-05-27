#pragma once

#include "alloy/ResourceFactory.hpp"

namespace alloy::layers::AutoResourceUsageTracking {

    class TrackingDevice;

    #define DXC_DECL_RF_CREATE_WITH_DESC(ResType) \
    virtual common::sp<I##ResType> Create##ResType ( \
        const I##ResType ::Description& description) override;

    // Mixin style
    template<typename Base>
    class TrackingResourceFactory : public ResourceFactory {
        const Base* GetBase() const { return static_cast<const Base*>(this); }
        Base* GetBase() { return static_cast<Base*>(this); }

    public:

        //virtual void* GetHandle() const override;

        VLD_RF_FOR_EACH_RES(DXC_DECL_RF_CREATE_WITH_DESC)

        common::sp<IGfxPipeline> CreateGraphicsPipeline(
            const GraphicsPipelineDescription& description) override;
        
        common::sp<IComputePipeline> CreateComputePipeline(
            const ComputePipelineDescription& description) override;

        virtual common::sp<IMeshShaderPipeline> CreateMeshShaderPipeline(
            const MeshShaderPipelineDescription& description) override;

        common::sp<IShader> CreateShader(
            const IShader::Description& desc,
            const std::span<const std::uint8_t>& spv
        ) override;

        common::sp<ITexture> WrapNativeTexture(
            void* nativeHandle,
            const ITexture::Description& description) override;

        virtual common::sp<ITextureView> CreateTextureView(
            const common::sp<ITexture>& texture,
            const ITextureView::Description& description) override;

       
        virtual common::sp<IEvent> CreateSyncEvent() override;

    };

}
