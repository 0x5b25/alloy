#pragma once

#include <span>

#include "alloy/common/Common.hpp"
#include "alloy/common/RefCnt.hpp"
#include "alloy/Shader.hpp"
#include "alloy/Texture.hpp"
#include "alloy/FrameBuffer.hpp"
#include "alloy/Sampler.hpp"
#include "alloy/Buffer.hpp"
#include "alloy/Pipeline.hpp"
#include "alloy/Shader.hpp"
#include "alloy/CommandList.hpp"
#include "alloy/BindableResource.hpp"
#include "alloy/SyncObjects.hpp"
#include "alloy/SwapChain.hpp"

namespace alloy
{
    #define VLD_RF_CREATE_WITH_DESC(ResType) \
        virtual common::sp<I##ResType> Create##ResType ( \
            const I##ResType ::Description& description) = 0;
    
    #define VLD_RF_FOR_EACH_RES(V) \
        V(FrameBuffer)\
        V(Texture)\
        V(Buffer)\
        V(Sampler)\
        /*V(Shader)*/\
        V(ResourceSet)\
        V(ResourceLayout)\
        V(SwapChain)


    class ResourceFactory{

    public:
        //virtual void* GetHandle() const = 0;

        VLD_RF_FOR_EACH_RES(VLD_RF_CREATE_WITH_DESC)

        virtual common::sp<IShader> CreateShader(
            const IShader::Description& description,
            const std::span<std::uint8_t>& il) = 0;

        virtual common::sp<IGfxPipeline> CreateGraphicsPipeline(
            const GraphicsPipelineDescription& description) = 0;
        
        virtual common::sp<IComputePipeline> CreateComputePipeline(
            const ComputePipelineDescription& description) = 0;

        virtual common::sp<ITexture> WrapNativeTexture(
            void* nativeHandle,
            const ITexture::Description& description) = 0;

        common::sp<ITextureView> CreateTextureView(const common::sp<ITexture>& texture) {
            ITextureView::Description desc{};
            auto& texDesc = texture->GetDesc();
            
            //desc.target = target;
            desc.baseMipLevel = 0;
            desc.mipLevels = texDesc.mipLevels;
            desc.baseArrayLayer = 0;
            desc.arrayLayers = texDesc.arrayLayers;
            //desc.format = texDesc.format;

            return CreateTextureView(texture, desc);
        }
        virtual common::sp<ITextureView> CreateTextureView(
            const common::sp<ITexture>& texture,
            const ITextureView::Description& description) = 0;

        virtual common::sp<IRenderTarget> CreateRenderTarget(
            const common::sp<ITextureView>& texView) = 0;
        //virtual sp<CommandList> CreateCommandList() = 0;

        virtual common::sp<IEvent> CreateSyncEvent() = 0;
        //Why don't call CreateSemaphore? because there is a WinBase #define 
        // called CreateSemaphore!!!

    };

    //#undef VLD_RF_FOR_EACH_RES
    //#undef VLD_RF_CREATE_WITH_DESC
} // namespace alloy



