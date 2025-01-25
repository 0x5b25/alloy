#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include "alloy/Texture.hpp"
#include "alloy/Sampler.hpp"

#include <vector>

namespace alloy::vk
{
    class VulkanDevice;
    
    class VulkanTexture : public ITexture{

        common::sp<VulkanDevice> _dev;
        VkImage _img;
        VmaAllocation _allocation;

        //std::vector<VkImageLayout> _imageLayouts;

        //Layout tracking
        VkImageLayout _layout;
        VkAccessFlags _accessFlag;
        VkPipelineStageFlags _pipelineFlag;


        VulkanTexture(
            const common::sp<VulkanDevice>& dev,
            const ITexture::Description& desc
        ) : ITexture(desc)
          , _dev(dev)
        { }

    public:

        ~VulkanTexture();

        const VkImage& GetHandle() const { return _img; }
        const VulkanDevice& GetDevice() const { return *_dev; }
        bool IsOwnTexture() const {return _allocation != VK_NULL_HANDLE; }

        static common::sp<ITexture> Make(
            const common::sp<VulkanDevice>& dev,
            const ITexture::Description& desc
        );

        static common::sp<ITexture> WrapNative(
            const common::sp<VulkanDevice>& dev,
            const ITexture::Description& desc,
            VkImageLayout layout,
            VkAccessFlags accessFlag,
            VkPipelineStageFlags pipelineFlag,
            void* nativeHandle
        );

        virtual void WriteSubresource(
            uint32_t mipLevel,
            uint32_t arrayLayer,
            Point3D dstOrigin,
            Size3D writeSize,
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
            Point3D srcOrigin,
            Size3D readSize
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


    VkImageUsageFlags VdToVkTextureUsage(const ITexture::Description::Usage& vdUsage);
    
    VkImageType VdToVkTextureType(const ITexture::Description::Type& type);

    

    class VulkanTextureView : public ITextureView {

        VkImageView _view;

        VulkanTextureView(
            const common::sp<VulkanTexture>& target,
            const ITextureView::Description& desc
        ) :
            ITextureView(target,desc)
        {}

    public:

        ~VulkanTextureView();

        const VkImageView& GetHandle() const { return _view; }

        static common::sp<VulkanTextureView> Make(
            const common::sp<VulkanTexture>& target,
            const ITextureView::Description& desc
        );

    };

    class VulkanSampler : public ISampler{

        VkSampler _sampler;

        common::sp<VulkanDevice> _dev;

        VulkanSampler(
            const common::sp<VulkanDevice>& dev,
            const Description& desc
        ) : ISampler(desc)
          , _dev(dev)
        {}

    public:

        ~VulkanSampler();

        const VkSampler& GetHandle() const { return _sampler; }

        static common::sp<VulkanSampler> Make(
            const common::sp<VulkanDevice>& dev,
            const ISampler::Description& desc
        );

    };

} // namespace alloy

