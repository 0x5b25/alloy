#pragma once

#include "veldrid/common/Macros.h"
#include "veldrid/common/RefCnt.hpp"
#include "veldrid/ResourceFactory.hpp"

namespace Veldrid{

    class DXCDevice;

    #define DXC_DECL_RF_CREATE_WITH_DESC(ResType) \
        virtual sp<ResType> Create##ResType ( \
            const ResType ::Description& description);

    template<class Base>
    class DXCResourceFactoryThunk : public ResourceFactory{

    protected:
        Base* GetBase() { return static_cast<Base*>(this); }

    };


    class DXCResourceFactory : public DXCResourceFactoryThunk<DXCDevice>{

        DISABLE_COPY_AND_ASSIGN(DXCResourceFactory);

        //DXCDevice* _dev;

        sp<DXCDevice> _CreateNewDevHandle();

    public:
        //DXCResourceFactory(DXCDevice* dev) : _dev(dev){}
        DXCResourceFactory() = default;
        ~DXCResourceFactory() = default;

        
        VLD_RF_FOR_EACH_RES(DXC_DECL_RF_CREATE_WITH_DESC)

        sp<Pipeline> CreateGraphicsPipeline(
            const GraphicsPipelineDescription& description) override;
        
        sp<Pipeline> CreateComputePipeline(
            const ComputePipelineDescription& description) override;

        sp<Shader> CreateShader(
            const Shader::Description& desc,
            const std::span<std::uint8_t>& spv
        ) override;

        sp<Texture> WrapNativeTexture(
            void* nativeHandle,
            const Texture::Description& description) override;

        virtual sp<TextureView> CreateTextureView(
            const sp<Texture>& texture,
            const TextureView::Description& description) override;

       
        virtual sp<CommandList> CreateCommandList() override;

        virtual sp<Fence> CreateFence(bool initialSignaled) override;
        virtual sp<Semaphore> CreateDeviceSemaphore() override;
    };

}
