#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include "alloy/Texture.hpp"
#include "alloy/Sampler.hpp"

#include <vector>
#include <unordered_set>

namespace alloy::vk
{
    class VulkanDevice;
    
    class VulkanTexture : public ITexture{

        common::sp<VulkanDevice> _dev;
        VkImage _img;
        VmaAllocation _allocation;

        std::string _debugName;

        Description _desc;

        VulkanTexture(
            const common::sp<VulkanDevice>& dev,
            const ITexture::Description& desc
        ) : _desc(desc)
          , _dev(dev)
        { }

    public:

        ~VulkanTexture();

        VkImage GetHandle() const { return _img; }
        virtual void* GetNativeHandle() const override {return GetHandle();}

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

        virtual const Description& GetDesc() const override {return _desc;}

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

            
        virtual void SetDebugName(const std::string& name) override;
    };


    VkImageUsageFlags VdToVkTextureUsage(const ITexture::Description::Usage& vdUsage);
    
    VkImageType VdToVkTextureType(const ITexture::Description::Type& type);

    class VulkanTextureViewBase : public ITextureView {        

    protected:
        Description desc;

        common::sp<VulkanTexture> target;
        VkImageView view;

    public:
        VulkanTextureViewBase(
            common::sp<VulkanTexture> target,
            VkImageView view,
            const ITextureView::Description& desc
        )
            : desc(desc)
            , target(std::move(target))
            , view(view)
        { }

        virtual ~VulkanTextureViewBase() override { }

        VkImageView GetHandle() const { return view; }

        
        virtual const Description& GetDesc() const override { return desc; }
        virtual common::sp<ITexture> GetTextureObject() const override { return target; }


        static VkImageView MakeVkView(const VulkanDevice& dev, 
                                      VulkanTexture* target,
                                      const ITextureView::Description& desc );

    };

    class VulkanTextureView : public VulkanTextureViewBase {        
        
        VulkanTextureView(
            common::sp<VulkanTexture> target,
            VkImageView view,
            const ITextureView::Description& desc
        )
            : VulkanTextureViewBase(std::move(target), view, desc)
        { }

    public:

        virtual ~VulkanTextureView() override;

        static common::sp<VulkanTextureView> Make(
            const common::sp<VulkanTexture>& target,
            const ITextureView::Description& desc
        );

    };

    class VulkanSampler : public ISampler{

        VkSampler _sampler;

        common::sp<VulkanDevice> _dev;
        std::string _debugName;

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

        
        
        virtual void SetDebugName(const std::string& name) override;

    };

} // namespace alloy

