#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include "veldrid/Texture.hpp"
#include "veldrid/Sampler.hpp"

namespace Veldrid
{
    class VulkanDevice;
    
    class VulkanTexture : public Texture{

        VkImage _img;
        VmaAllocation _allocation;

        VulkanTexture(
            const sp<VulkanDevice>& dev,
            const Texture::Description& desc
        ) :
            Texture(dev, desc)
        {}


    public:

        ~VulkanTexture();

        VkImage GetHandle() const { return _img; }

        static sp<Texture> Make(
            const sp<VulkanDevice>& dev,
            const Texture::Description& desc
        );

    };


    VkImageUsageFlags VdToVkTextureUsage(const Texture::Description::Usage& vdUsage);
    
    VkImageType VdToVkTextureType(const Texture::Description::Type& type);

    

    class VulkanTextureView : public TextureView {

        VkImageView _view;

        VulkanTextureView(
            const sp<VulkanDevice>& dev,
            const sp<VulkanTexture>& target,
            const TextureView::Description& desc
        ) :
            TextureView(dev,target,desc)
        {}

    public:

        ~VulkanTextureView();

        VkImageView GetHandle() const { return _view; }

        static sp<TextureView> Make(
            const sp<VulkanDevice>& dev,
            const sp<VulkanTexture>& target,
            const TextureView::Description& desc
        );

    };

    class VulkanSampler : public Sampler{

        VkSampler _sampler;

        VulkanSampler(
            const sp<VulkanDevice>& dev
        ) :
            Sampler(dev)
        {}

    public:

        ~VulkanSampler();

        VkSampler GetHandle() const { return _sampler; }

        static sp<VulkanSampler> Make(
            const sp<VulkanDevice>& dev,
            const Sampler::Description& desc
        );

    };

} // namespace Veldrid

