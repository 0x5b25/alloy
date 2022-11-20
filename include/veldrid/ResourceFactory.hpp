#pragma once

#include "veldrid/common/RefCnt.hpp"
#include "veldrid/Shader.hpp"
#include "veldrid/Texture.hpp"
#include "veldrid/Framebuffer.hpp"
#include "veldrid/Sampler.hpp"
#include "veldrid/Buffer.hpp"
#include "veldrid/Pipeline.hpp"
#include "veldrid/Shader.hpp"
#include "veldrid/CommandList.hpp"
#include "veldrid/BindableResource.hpp"
#include "veldrid/Fence.hpp"
#include "veldrid/SwapChain.hpp"

namespace Veldrid
{

    #define VLD_RF_CREATE_WITH_DESC(ResType) \
        virtual sp<ResType> Create##ResType ( \
            const ResType ::Description& description) = 0;
    
    #define VLD_RF_FOR_EACH_RES(V) \
        V(Framebuffer)\
        V(Texture)\
        V(Buffer)\
        V(Sampler)\
        /*V(Shader)*/\
        V(ResourceSet)\
        V(ResourceLayout)\
        V(SwapChain)


    class ResourceFactory{

    public:

        VLD_RF_FOR_EACH_RES(VLD_RF_CREATE_WITH_DESC)

        virtual sp<Shader> CreateShader(
            const Shader::Description& description,
            const std::vector<std::uint32_t>& spvBinary) = 0;

        virtual sp<Pipeline> CreateGraphicsPipeline(
            const GraphicsPipelineDescription& description) = 0;
        
        virtual sp<Pipeline> CreateComputePipeline(
            const ComputePipelineDescription& description) = 0;

        virtual sp<Texture> WrapNativeTexture(
            void* nativeHandle,
            const Texture::Description& description) = 0;

        sp<TextureView> CreateTextureView(sp<Texture>& texture) {
            TextureView::Description desc{};
            auto& texDesc = texture->GetDesc();
            
            //desc.target = target;
            desc.baseMipLevel = 0;
            desc.mipLevels = texDesc.mipLevels;
            desc.baseArrayLayer = 0;
            desc.arrayLayers = texDesc.arrayLayers;
            desc.format = texDesc.format;

            return CreateTextureView(texture, desc);
        }
        virtual sp<TextureView> CreateTextureView(
            const sp<Texture>& texture,
            const TextureView::Description& description) = 0;

       
        virtual sp<CommandList> CreateCommandList() = 0;

        virtual sp<Fence> CreateFence(bool initialSignaled) = 0;

    };

    //#undef VLD_RF_FOR_EACH_RES
    //#undef VLD_RF_CREATE_WITH_DESC
} // namespace Veldrid



