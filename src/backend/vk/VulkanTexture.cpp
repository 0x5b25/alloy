#include "VulkanTexture.hpp"

#include "veldrid/common/Macros.h"
#include "veldrid/Helpers.hpp"

#include "VkCommon.hpp"
#include "VulkanDevice.hpp"
#include "VkTypeCvt.hpp"

namespace Veldrid {
    
    VulkanTexture::VulkanTexture(const sp<GraphicsDevice>& dev, const Texture::Description& desc)
        : Texture(dev, desc)
    { }

    Veldrid::VulkanTexture::~VulkanTexture() {
        if(IsOwnTexture()){
            auto _dev = reinterpret_cast<VulkanDevice*>(dev.get());
            vmaDestroyImage(_dev->Allocator(), _img, _allocation);
        }
    }

    
    VulkanDevice* VulkanTexture::GetDevice() const { return static_cast<VulkanDevice*>(dev.get()); }

	sp<Texture> VulkanTexture::Make(const sp<VulkanDevice>& dev, const Texture::Description& desc)
	{
        //_gd = gd;
        //_width = description.Width;
        //_height = description.Height;
        //_depth = description.Depth;
        //MipLevels = description.MipLevels;
        //ArrayLayers = description.ArrayLayers;
        bool isCubemap = desc.usage.cubemap;
        auto actualImageArrayLayers = isCubemap
            ? 6 * desc.arrayLayers
            : desc.arrayLayers;
        //_format = description.Format;
        //Usage = description.Usage;
        //Type = description.Type;
        //SampleCount = description.SampleCount;
        //VkSampleCount = VkFormats.VdToVkSampleCount(SampleCount);

        //bool isStaging = desc.usage.staging;

        //if (!isStaging)
        //{
            VkImageCreateInfo imageCI{};
            imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageCI.mipLevels = desc.mipLevels;
            imageCI.arrayLayers = actualImageArrayLayers;
            imageCI.imageType = VdToVkTextureType(desc.type);
            imageCI.extent.width = desc.width;
            imageCI.extent.height = desc.height;
            imageCI.extent.depth = desc.depth;
            imageCI.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_PREINITIALIZED;
            imageCI.usage = VdToVkTextureUsage(desc.usage);
            imageCI.tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL; //isStaging ? VkImageTiling.Linear : VkImageTiling.Optimal;
            imageCI.format = Veldrid::VK::priv::VdToVkPixelFormat(desc.format, desc.usage.depthStencil);
            imageCI.flags = VkImageCreateFlagBits::VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

            imageCI.samples = Veldrid::VK::priv::VdToVkSampleCount(desc.sampleCount);
            if (isCubemap)
            {
                imageCI.flags |= VkImageCreateFlagBits::VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            }

            auto& allocator = dev->Allocator();
            //allocator
            VmaAllocationCreateInfo allocCI{};
            allocCI.usage = VMA_MEMORY_USAGE_AUTO;

            VkImage img;
            VmaAllocation allocation;
            auto res = vmaCreateImage(allocator, &imageCI, &allocCI, &img, &allocation, nullptr);

            if (res != VK_SUCCESS) return nullptr;

            //auto subresourceCount = desc.mipLevels * actualImageArrayLayers * desc.depth;
            //std::vector<VkImageLayout> imgLayouts(
            //    subresourceCount,
            //    VkImageLayout::VK_IMAGE_LAYOUT_PREINITIALIZED);
            
            
        //}
        //else // isStaging
        //{
        //    uint depthPitch = FormatHelpers.GetDepthPitch(
        //        FormatHelpers.GetRowPitch(Width, Format),
        //        Height,
        //        Format);
        //    uint stagingSize = depthPitch * Depth;
        //    for (uint level = 1; level < MipLevels; level++)
        //    {
        //        Util.GetMipDimensions(this, level, out uint mipWidth, out uint mipHeight, out uint mipDepth);
        //
        //        depthPitch = FormatHelpers.GetDepthPitch(
        //            FormatHelpers.GetRowPitch(mipWidth, Format),
        //            mipHeight,
        //            Format);
        //
        //        stagingSize += depthPitch * mipDepth;
        //    }
        //    stagingSize *= ArrayLayers;
        //
        //    VkBufferCreateInfo bufferCI = VkBufferCreateInfo.New();
        //    bufferCI.usage = VkBufferUsageFlags.TransferSrc | VkBufferUsageFlags.TransferDst;
        //    bufferCI.size = stagingSize;
        //    VkResult result = vkCreateBuffer(_gd.Device, ref bufferCI, null, out _stagingBuffer);
        //    CheckResult(result);
        //
        //    VkMemoryRequirements bufferMemReqs;
        //    bool prefersDedicatedAllocation;
        //    if (_gd.GetBufferMemoryRequirements2 != null)
        //    {
        //        VkBufferMemoryRequirementsInfo2KHR memReqInfo2 = VkBufferMemoryRequirementsInfo2KHR.New();
        //        memReqInfo2.buffer = _stagingBuffer;
        //        VkMemoryRequirements2KHR memReqs2 = VkMemoryRequirements2KHR.New();
        //        VkMemoryDedicatedRequirementsKHR dedicatedReqs = VkMemoryDedicatedRequirementsKHR.New();
        //        memReqs2.pNext = &dedicatedReqs;
        //        _gd.GetBufferMemoryRequirements2(_gd.Device, &memReqInfo2, &memReqs2);
        //        bufferMemReqs = memReqs2.memoryRequirements;
        //        prefersDedicatedAllocation = dedicatedReqs.prefersDedicatedAllocation || dedicatedReqs.requiresDedicatedAllocation;
        //    }
        //    else
        //    {
        //        vkGetBufferMemoryRequirements(gd.Device, _stagingBuffer, out bufferMemReqs);
        //        prefersDedicatedAllocation = false;
        //    }
        //
        //    // Use "host cached" memory when available, for better performance of GPU -> CPU transfers
        //    var propertyFlags = VkMemoryPropertyFlags.HostVisible | VkMemoryPropertyFlags.HostCoherent | VkMemoryPropertyFlags.HostCached;
        //    if (!TryFindMemoryType(_gd.PhysicalDeviceMemProperties, bufferMemReqs.memoryTypeBits, propertyFlags, out _))
        //    {
        //        propertyFlags ^= VkMemoryPropertyFlags.HostCached;
        //    }
        //    _memoryBlock = _gd.MemoryManager.Allocate(
        //        _gd.PhysicalDeviceMemProperties,
        //        bufferMemReqs.memoryTypeBits,
        //        propertyFlags,
        //        true,
        //        bufferMemReqs.size,
        //        bufferMemReqs.alignment,
        //        prefersDedicatedAllocation,
        //        VkImage.Null,
        //        _stagingBuffer);
        //
        //    result = vkBindBufferMemory(_gd.Device, _stagingBuffer, _memoryBlock.DeviceMemory, _memoryBlock.Offset);
        //    CheckResult(result);
        //}
        auto tex = new VulkanTexture{dev, desc};
        tex->_img = img;
        tex->_allocation = allocation;
        tex->_layout = VkImageLayout::VK_IMAGE_LAYOUT_PREINITIALIZED;
        tex->_accessFlag = 0;
        tex->_pipelineFlag = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        //ClearIfRenderTarget();
        // If the image is going to be used as a render target, we need to clear the data before its first use.
        //if (desc.usage.renderTarget) {
        //    dev->ClearColorTexture(tex, new VkClearColorValue(0, 0, 0, 0));
        //}
        //else if (desc.usage.depthStencil) {
        //    dev->ClearDepthTexture(tex, new VkClearDepthStencilValue(0, 0));
        //}
        ////TransitionIfSampled();
        //if (desc.usage.sampled)
        //{
        //    dev->TransitionImageLayout(tex, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        //}
        //RefCount = new ResourceRefCount(RefCountedDispose);

		

		return sp<Texture>(tex);
	}


    sp<Texture> VulkanTexture::WrapNative(
        const sp<VulkanDevice>& dev, 
        const Texture::Description& desc,
        VkImageLayout layout,
        VkAccessFlags accessFlag,
        VkPipelineStageFlags pipelineFlag,
        void* nativeHandle
    ){
        auto tex = new VulkanTexture{dev, desc};
        tex->_img = (VkImage)nativeHandle;
        tex->_allocation = VK_NULL_HANDLE;
        tex->_layout = layout;
        tex->_accessFlag = accessFlag;
        tex->_pipelineFlag = pipelineFlag;
        //Debug.Assert(width > 0 && height > 0);
        //    _gd = gd;
        //    MipLevels = mipLevels;
        //    _width = width;
        //    _height = height;
        //    _depth = 1;
        //    VkFormat = vkFormat;
        //    _format = VkFormats.VkToVdPixelFormat(VkFormat);
        //    ArrayLayers = arrayLayers;
        //    Usage = usage;
        //    Type = TextureType.Texture2D;
        //    SampleCount = sampleCount;
        //    VkSampleCount = VkFormats.VdToVkSampleCount(sampleCount);
        //    _optimalImage = existingImage;
        //    _imageLayouts = new[] { VkImageLayout.Undefined };
        //    _isSwapchainTexture = true;
//
        //    ClearIfRenderTarget();
        //    RefCount = new ResourceRefCount(DisposeCore);
        return sp(tex);
    }

    
    static std::uint32_t CalculateSubresource(
        const Veldrid::Texture::Description& desc,
        std::uint32_t mipLevel, std::uint32_t arrayLayer
    ) {
        return arrayLayer * desc.mipLevels+ mipLevel;
    }

    void VulkanTexture::TransitionImageLayout(
        VkCommandBuffer cb,
        //std::uint32_t baseMipLevel,
        //std::uint32_t levelCount,
        //std::uint32_t baseArrayLayer,
        //std::uint32_t layerCount,
        VkImageLayout layout,
        VkAccessFlags accessFlag,
        VkPipelineStageFlags pipelineFlag
    ){
        /*VkImageLayout oldLayout = _imageLayouts[
           CalculateSubresource(description, baseMipLevel, baseArrayLayer)];
#ifdef VLD_DEBUG
        //layout of all subresources should be the same
        for (unsigned level = 0; level < levelCount; level++)
        {
            for (unsigned layer = 0; layer < layerCount; layer++)
            {
                assert(_imageLayouts[CalculateSubresource(
                    description, baseMipLevel + level, baseArrayLayer + layer)] == oldLayout);
            }
        }
#endif*/
        if (_layout       == layout
         && _accessFlag   == accessFlag
         && _pipelineFlag == pipelineFlag) {
            return;
        }

        VkImageMemoryBarrier barrier{};
        barrier.oldLayout = _layout;
        barrier.newLayout = layout;
        barrier.srcAccessMask = _accessFlag;
        barrier.dstAccessMask = accessFlag;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = _img;
        auto& aspectMask = barrier.subresourceRange.aspectMask;
        if (description.usage.depthStencil) {
            aspectMask = Helpers::FormatHelpers::IsStencilFormat(description.format)
                ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
                : VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        else {
            aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = description.mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = description.arrayLayers;

        VkPipelineStageFlags srcStageFlags = _pipelineFlag;
        VkPipelineStageFlags dstStageFlags = pipelineFlag;

        vkCmdPipelineBarrier(
            cb,
            srcStageFlags,
            dstStageFlags,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        _layout = layout;
        _pipelineFlag = pipelineFlag;
        _accessFlag = accessFlag;

    }
    
    VulkanTextureView::~VulkanTextureView() {
        auto vkTex = static_cast<VulkanTexture*>(target.get());
        auto _dev = vkTex->GetDevice();
        vkDestroyImageView(_dev->LogicalDev(), _view, nullptr);
    }

	sp<TextureView> VulkanTextureView::Make(
		const sp<VulkanTexture>& target,
		const TextureView::Description& desc
	){
        VulkanDevice* dev = target->GetDevice();
        auto format = desc.format == PixelFormat::Unknown ? target->GetDesc().format : desc.format;
        auto& targetDesc = target->GetDesc();

        VkImageViewCreateInfo imageViewCI{};
        imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        //VkTexture tex = Util.AssertSubtype<Texture, VkTexture>(description.Target);
        imageViewCI.image = target->GetHandle();
        imageViewCI.format = Veldrid::VK::priv::VdToVkPixelFormat(format, targetDesc.usage.depthStencil);

        VkImageAspectFlags aspectFlags;
        if (targetDesc.usage.depthStencil)
        {
            aspectFlags = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        else
        {
            aspectFlags = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
        }

        //imageViewCI.subresourceRange = new VkImageSubresourceRange(
        imageViewCI.subresourceRange.aspectMask = aspectFlags;
        imageViewCI.subresourceRange.baseMipLevel = desc.baseMipLevel;
        imageViewCI.subresourceRange.levelCount = desc.mipLevels;
        imageViewCI.subresourceRange.baseArrayLayer = desc.baseArrayLayer;
        imageViewCI.subresourceRange.layerCount = desc.arrayLayers;

        if (targetDesc.usage.cubemap)
        {
            imageViewCI.viewType = desc.arrayLayers == 1 
                ? VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE 
                : VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            imageViewCI.subresourceRange.layerCount *= 6;
        }
        else
        {
            switch (targetDesc.type)
            {
            case Texture::Description::Type::Texture1D:
                imageViewCI.viewType = desc.arrayLayers == 1
                    ? VkImageViewType::VK_IMAGE_VIEW_TYPE_1D
                    : VkImageViewType::VK_IMAGE_VIEW_TYPE_1D_ARRAY;
                break;
            case Texture::Description::Type::Texture2D:
                imageViewCI.viewType = desc.arrayLayers == 1
                    ? VkImageViewType::VK_IMAGE_VIEW_TYPE_2D
                    : VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                break;
            case Texture::Description::Type::Texture3D:
                imageViewCI.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_3D;
                break;
            }
        }

        VkImageView vkImgView;
        vkCreateImageView(dev->LogicalDev(), &imageViewCI, nullptr, &vkImgView);

        auto imgView = new VulkanTextureView(target, desc);
        imgView->_view = vkImgView;

        return sp(imgView);
	}

    VkImageUsageFlags VdToVkTextureUsage(const Texture::Description::Usage& vdUsage)
    {
        VkImageUsageFlags vkUsage{
              VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT
        };

        if (vdUsage.sampled) {
            vkUsage |= VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        if (vdUsage.depthStencil) {
            vkUsage |= VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        if (vdUsage.renderTarget)
        {
            vkUsage |= VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        if (vdUsage.storage)
        {
            vkUsage |= VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT;
        }

        return vkUsage;
    }

    VkImageType VdToVkTextureType(const Texture::Description::Type& type)
    {
        switch (type)
        {
        case Texture::Description::Type::Texture1D:
            return VkImageType::VK_IMAGE_TYPE_1D;
        case Texture::Description::Type::Texture2D:
            return VkImageType::VK_IMAGE_TYPE_2D;
        case Texture::Description::Type::Texture3D:
            return VkImageType::VK_IMAGE_TYPE_3D;
        default:
            return VK_IMAGE_TYPE_MAX_ENUM;
        }
    }


    VulkanSampler::~VulkanSampler(){
        vkDestroySampler(_dev->LogicalDev(), _sampler, nullptr);
    }


    sp<VulkanSampler> VulkanSampler::Make(
        const sp<VulkanDevice>& dev,
        const Sampler::Description& desc
    ){
        VkFilter minFilter, magFilter;
        VkSamplerMipmapMode mipmapMode;
        Veldrid::VK::priv::GetFilterParams(desc.filter, minFilter, magFilter, mipmapMode);

        VkSamplerCreateInfo samplerCI{};

        bool compareEnable = desc.comparisonKind != nullptr;
        
        samplerCI.sType = VkStructureType::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCI.addressModeU =  Veldrid::VK::priv::VdToVkSamplerAddressMode(desc.addressModeU);
        samplerCI.addressModeV =  Veldrid::VK::priv::VdToVkSamplerAddressMode(desc.addressModeV);
        samplerCI.addressModeW =  Veldrid::VK::priv::VdToVkSamplerAddressMode(desc.addressModeW);
        samplerCI.minFilter = minFilter;
        samplerCI.magFilter = magFilter;
        samplerCI.mipmapMode = mipmapMode;
        samplerCI.compareEnable = compareEnable,
        samplerCI.compareOp = compareEnable
            ? Veldrid::VK::priv::VdToVkCompareOp(*desc.comparisonKind)
            : VkCompareOp::VK_COMPARE_OP_NEVER,
        samplerCI.anisotropyEnable = desc.filter == Sampler::Description::SamplerFilter::Anisotropic;
        samplerCI.maxAnisotropy = desc.maximumAnisotropy;
        samplerCI.minLod = desc.minimumLod;
        samplerCI.maxLod = desc.maximumLod;
        samplerCI.mipLodBias = desc.lodBias;
        samplerCI.borderColor = Veldrid::VK::priv::VdToVkSamplerBorderColor(desc.borderColor);

        VkSampler rawSampler;
        vkCreateSampler(dev->LogicalDev(), &samplerCI, nullptr, &rawSampler);

        auto* sampler = new VulkanSampler(dev, desc);
        sampler->_sampler = rawSampler;
        return sp<VulkanSampler>(sampler);
    }

}
