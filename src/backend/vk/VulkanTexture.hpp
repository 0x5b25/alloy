#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include "veldrid/Texture.hpp"
#include "veldrid/Sampler.hpp"

#include <vector>

namespace Veldrid
{
    class VulkanDevice;
    
    class VulkanTexture : public Texture{

        VkImage _img;
        VmaAllocation _allocation;

        //std::vector<VkImageLayout> _imageLayouts;

        //Layout tracking
        VkImageLayout _layout;
        VkAccessFlags _accessFlag;
        VkPipelineStageFlags _pipelineFlag;


        VulkanTexture(
            const sp<GraphicsDevice>& dev,
            const Texture::Description& desc
        );


    public:

        ~VulkanTexture();

        const VkImage& GetHandle() const { return _img; }
        bool IsOwnTexture() const {return _allocation != VK_NULL_HANDLE; }
        VulkanDevice* GetDevice() const;

        static sp<Texture> Make(
            const sp<VulkanDevice>& dev,
            const Texture::Description& desc
        );

        static sp<Texture> WrapNative(
            const sp<VulkanDevice>& dev,
            const Texture::Description& desc,
            VkImageLayout layout,
            VkAccessFlags accessFlag,
            VkPipelineStageFlags pipelineFlag,
            void* nativeHandle
        );

        virtual void WriteSubresource(
            uint32_t mipLevel,
            uint32_t arrayLayer,
            uint32_t dstX, uint32_t dstY, uint32_t dstZ,
            std::uint32_t width, std::uint32_t height, std::uint32_t depth,
            const void* src,
            uint32_t srcRowPitch,
            uint32_t srcDepthPitch
        ) override;

        virtual void ReadSubresource(
            void* dst,
            uint32_t dstRowPitch,
            uint32_t dstDepthPitch,
            uint32_t mipLevel,
            uint32_t arrayLayer,
            uint32_t srcX, uint32_t srcY, uint32_t srcZ,
            std::uint32_t width, std::uint32_t height, std::uint32_t depth
        ) override;

        virtual SubresourceLayout GetSubresourceLayout(
            uint32_t mipLevel,
            uint32_t arrayLayer,
            SubresourceAspect aspect) override;

    public:
        void TransitionImageLayout(
            VkCommandBuffer cb,
            //std::uint32_t baseMipLevel,
            //std::uint32_t levelCount,
            //std::uint32_t baseArrayLayer,
            //std::uint32_t layerCount,
            VkImageLayout layout,
            VkAccessFlags accessFlag,
            VkPipelineStageFlags pipelineFlag
        );

        const VkImageLayout& GetLayout() const { return _layout; }
        void SetLayout(VkImageLayout newLayout) { _layout = newLayout; }

    };


    VkImageUsageFlags VdToVkTextureUsage(const Texture::Description::Usage& vdUsage);
    
    VkImageType VdToVkTextureType(const Texture::Description::Type& type);

    

    class VulkanTextureView : public TextureView {

        VkImageView _view;

        VulkanTextureView(
            const sp<VulkanTexture>& target,
            const TextureView::Description& desc
        ) :
            TextureView(target,desc)
        {}

    public:

        ~VulkanTextureView();

        const VkImageView& GetHandle() const { return _view; }

        static sp<TextureView> Make(
            const sp<VulkanTexture>& target,
            const TextureView::Description& desc
        );

    };

    class VulkanSampler : public Sampler{

        VkSampler _sampler;

        sp<VulkanDevice> _dev;

        VulkanSampler(
            const sp<VulkanDevice>& dev,
            const Description& desc
        ) : Sampler(desc)
          , _dev(dev)
        {}

    public:

        ~VulkanSampler();

        const VkSampler& GetHandle() const { return _sampler; }

        static sp<VulkanSampler> Make(
            const sp<VulkanDevice>& dev,
            const Sampler::Description& desc
        );

    };

} // namespace Veldrid

