#include "VulkanTexture.hpp"

#include "alloy/common/Macros.h"
#include "alloy/common/Common.hpp"
#include "alloy/Helpers.hpp"

#include "VkCommon.hpp"
#include "VulkanDevice.hpp"
#include "VkTypeCvt.hpp"

namespace alloy::vk {
    
    VulkanTexture::~VulkanTexture() {
        if(IsOwnTexture()){
            vmaDestroyImage(_dev->Allocator(), _img, _allocation);
        }
    }

	common::sp<ITexture> VulkanTexture::Make(const common::sp<VulkanDevice>& dev, 
                                             const ITexture::Description& desc)
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

        bool isStaging = desc.hostAccess != HostAccess::None;

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
            imageCI.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
            imageCI.usage = VdToVkTextureUsage(desc.usage);
            imageCI.tiling = isStaging ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
            imageCI.format = VdToVkPixelFormat(desc.format, desc.usage.depthStencil);
            imageCI.flags = VkImageCreateFlagBits::VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

            imageCI.samples = VdToVkSampleCount(desc.sampleCount);
            if (isCubemap)
            {
                imageCI.flags |= VkImageCreateFlagBits::VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            }

            auto& allocator = dev->Allocator();
            //allocator
            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

            switch (desc.hostAccess)
            {        
            case HostAccess::PreferRead:
                //allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
                allocInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                allocInfo.preferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
                allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
                break;
            case HostAccess::PreferWrite:
                //allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
                allocInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                allocInfo.preferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
                break;
            case HostAccess::None:
                allocInfo.preferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            default:
                break;
            }

            VkImage img;
            VmaAllocation allocation;
            auto res = vmaCreateImage(allocator, &imageCI, &allocInfo, &img, &allocation, nullptr);

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

		

		return common::sp<ITexture>(tex);
	}


    common::sp<ITexture> VulkanTexture::WrapNative(
        const common::sp<VulkanDevice>& dev, 
        const ITexture::Description& desc,
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
        return common::sp(tex);
    }

    
    static std::uint32_t CalculateSubresource(
        const alloy::ITexture::Description& desc,
        std::uint32_t mipLevel, std::uint32_t arrayLayer
    ) {
        return arrayLayer * desc.mipLevels+ mipLevel;
    }
    
    void VulkanTexture::WriteSubresource(
        uint32_t mipLevel,
        uint32_t arrayLayer,
        Point3D dstOrigin,
        Size3D writeSize,
        const void* src,
        uint32_t srcRowPitch,
        uint32_t srcDepthPitch
    ) {
        void* mappedData;
        VK_CHECK(vmaMapMemory(_dev->Allocator(), _allocation, &mappedData));

        auto subResLayout = GetSubresourceLayout(mipLevel, arrayLayer, SubresourceAspect::Color);
        auto elementSize = FormatHelpers::GetSizeInBytes(description.format);

        auto pDst = (char*)mappedData + subResLayout.offset;

        //According to vulkan spec:
        // (x,y,z,layer) are in texel coordinates
        //address(x,y,z,layer) = layer*arrayPitch 
        //                     + z*depthPitch 
        //                     + y*rowPitch 
        //                     + x*elementSize 
        //                     + offset

        auto pDstZ = pDst + dstOrigin.z * subResLayout.depthPitch;
        auto pSrcZ = (const char*)src;

        for(uint32_t z = 0; z < writeSize.depth; z++) {
            auto pSrcY = pSrcZ;
            auto pDstY = pDstZ + dstOrigin.y * subResLayout.rowPitch;
            for(uint32_t y = 0; y < writeSize.height; y++) {
                
                auto pSrcX = pSrcY;
                auto pDstX = pDstY + dstOrigin.x * elementSize;

                memcpy(pDstX, pSrcX, writeSize.width * elementSize);

                pSrcY += srcRowPitch;
                pDstY += subResLayout.rowPitch;
            }

            pSrcZ += srcDepthPitch;
            pDstZ += subResLayout.depthPitch;
        }
                                                      
        vmaUnmapMemory(_dev->Allocator(), _allocation);
    }

    void VulkanTexture::ReadSubresource(
        void* dst,
        uint32_t dstRowPitch,
        uint32_t dstDepthPitch,
        uint32_t mipLevel,
        uint32_t arrayLayer,
        Point3D srcOrigin,
        Size3D readSize
    ) {
        void* mappedData;
        VK_CHECK(vmaMapMemory(_dev->Allocator(), _allocation, &mappedData));

        auto subResLayout = GetSubresourceLayout(mipLevel, arrayLayer, SubresourceAspect::Color);
        auto elementSize = FormatHelpers::GetSizeInBytes(description.format);

        auto pSrc = (char*)mappedData + subResLayout.offset;

        //According to vulkan spec:
        // (x,y,z,layer) are in texel coordinates
        //address(x,y,z,layer) = layer*arrayPitch 
        //                     + z*depthPitch 
        //                     + y*rowPitch 
        //                     + x*elementSize 
        //                     + offset

        auto pSrcZ = pSrc + srcOrigin.z * subResLayout.depthPitch;
        auto pDstZ = (char*)dst;

        for(uint32_t z = 0; z < readSize.depth; z++) {
            auto pSrcY = pSrcZ + srcOrigin.y * subResLayout.rowPitch;
            auto pDstY = pDstZ;
            for(uint32_t y = 0; y < readSize.height; y++) {
                
                auto pSrcX = pSrcY + srcOrigin.x * elementSize;
                auto pDstX = pDstY;

                memcpy(pDstX, pSrcX, readSize.width * elementSize);

                pSrcY += subResLayout.rowPitch;
                pDstY += dstRowPitch;
            }

            pSrcZ += subResLayout.depthPitch;
            pDstZ += dstDepthPitch;
        }
                                                      
        vmaUnmapMemory(_dev->Allocator(), _allocation);
    }


    
    ITexture::SubresourceLayout VulkanTexture::GetSubresourceLayout(
            uint32_t mipLevel,
            uint32_t arrayLayer,
            SubresourceAspect aspect
    ) {
        
        VkImageSubresource subResDesc {
            //VkImageAspectFlags    aspectMask;
            .mipLevel = mipLevel,
            .arrayLayer = arrayLayer
        };

        switch(aspect) {
            case SubresourceAspect::Depth :
                subResDesc.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                break;
            case SubresourceAspect::Stencil : 
                subResDesc.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
                break;
            case SubresourceAspect::Color :
            default:
                subResDesc.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                break;
        }
        
        VkSubresourceLayout outLayout{ };

        vkGetImageSubresourceLayout(_dev->LogicalDev(), _img, &subResDesc, &outLayout);

        ITexture::SubresourceLayout ret {};
        ret.offset = outLayout.offset;
        ret.rowPitch = outLayout.rowPitch;
        ret.depthPitch = outLayout.depthPitch;
        ret.arrayPitch = outLayout.arrayPitch;
        //According to vulkan spec:
        // (x,y,z,layer) are in texel coordinates
        //address(x,y,z,layer) = layer*arrayPitch 
        //                     + z*depthPitch 
        //                     + y*rowPitch 
        //                     + x*elementSize 
        //                     + offset
        return ret;

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
            aspectMask = FormatHelpers::IsStencilFormat(description.format)
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
        auto& _dev = vkTex->GetDevice();
        vkDestroyImageView(_dev.LogicalDev(), _view, nullptr);
    }

	common::sp<VulkanTextureView> VulkanTextureView::Make(
		const common::sp<VulkanTexture>& target,
		const ITextureView::Description& desc
	){
        auto& dev = target->GetDevice();
        auto& targetDesc = target->GetDesc();
        
        VkImageViewCreateInfo imageViewCI{};
        imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        //VkTexture tex = Util.AssertSubtype<Texture, VkTexture>(description.Target);
        imageViewCI.image = target->GetHandle();
        imageViewCI.format = VdToVkPixelFormat(targetDesc.format, targetDesc.usage.depthStencil);

        VkImageAspectFlags aspectFlags;
        if (targetDesc.usage.depthStencil)
        {
            aspectFlags = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;

            if(FormatHelpers::IsStencilFormat(targetDesc.format)){
                aspectFlags |= VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT;
            }
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
            case ITexture::Description::Type::Texture1D:
                imageViewCI.viewType = desc.arrayLayers == 1
                    ? VkImageViewType::VK_IMAGE_VIEW_TYPE_1D
                    : VkImageViewType::VK_IMAGE_VIEW_TYPE_1D_ARRAY;
                break;
            case ITexture::Description::Type::Texture2D:
                imageViewCI.viewType = desc.arrayLayers == 1
                    ? VkImageViewType::VK_IMAGE_VIEW_TYPE_2D
                    : VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                break;
            case ITexture::Description::Type::Texture3D:
                imageViewCI.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_3D;
                break;
            }
        }

        VkImageView vkImgView;
        vkCreateImageView(dev.LogicalDev(), &imageViewCI, nullptr, &vkImgView);

        auto imgView = new VulkanTextureView(target, desc);
        imgView->_view = vkImgView;

        return common::sp(imgView);
	}

    VkImageUsageFlags VdToVkTextureUsage(const ITexture::Description::Usage& vdUsage)
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

    VkImageType VdToVkTextureType(const ITexture::Description::Type& type)
    {
        switch (type)
        {
        case ITexture::Description::Type::Texture1D:
            return VkImageType::VK_IMAGE_TYPE_1D;
        case ITexture::Description::Type::Texture2D:
            return VkImageType::VK_IMAGE_TYPE_2D;
        case ITexture::Description::Type::Texture3D:
            return VkImageType::VK_IMAGE_TYPE_3D;
        default:
            return VK_IMAGE_TYPE_MAX_ENUM;
        }
    }


    VulkanSampler::~VulkanSampler(){
        vkDestroySampler(_dev->LogicalDev(), _sampler, nullptr);
    }


    common::sp<VulkanSampler> VulkanSampler::Make(
        const common::sp<VulkanDevice>& dev,
        const ISampler::Description& desc
    ){
        VkFilter minFilter, magFilter;
        VkSamplerMipmapMode mipmapMode;
        GetFilterParams(desc.filter, minFilter, magFilter, mipmapMode);

        VkSamplerCreateInfo samplerCI{};

        bool compareEnable = desc.comparisonKind != nullptr;
        
        samplerCI.sType = VkStructureType::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCI.addressModeU =  VdToVkSamplerAddressMode(desc.addressModeU);
        samplerCI.addressModeV =  VdToVkSamplerAddressMode(desc.addressModeV);
        samplerCI.addressModeW =  VdToVkSamplerAddressMode(desc.addressModeW);
        samplerCI.minFilter = minFilter;
        samplerCI.magFilter = magFilter;
        samplerCI.mipmapMode = mipmapMode;
        samplerCI.compareEnable = compareEnable,
        samplerCI.compareOp = compareEnable
            ? VdToVkCompareOp(*desc.comparisonKind)
            : VkCompareOp::VK_COMPARE_OP_NEVER,
        samplerCI.anisotropyEnable = desc.filter == ISampler::Description::SamplerFilter::Anisotropic;
        samplerCI.maxAnisotropy = desc.maximumAnisotropy;
        samplerCI.minLod = desc.minimumLod;
        samplerCI.maxLod = desc.maximumLod;
        samplerCI.mipLodBias = desc.lodBias;
        samplerCI.borderColor = VdToVkSamplerBorderColor(desc.borderColor);

        VkSampler rawSampler;
        vkCreateSampler(dev->LogicalDev(), &samplerCI, nullptr, &rawSampler);

        auto* sampler = new VulkanSampler(dev, desc);
        sampler->_sampler = rawSampler;
        return common::sp<VulkanSampler>(sampler);
    }

}
