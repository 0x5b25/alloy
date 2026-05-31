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

        bool isHostVisible = desc.hostAccess != HostAccess::None;

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

            // #TODO: Ditch mapped texture altogether. Move towards dedicated transfer queue
            // Make all textures host invisible?
            imageCI.tiling = isHostVisible ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;

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

            case HostAccess::SystemMemoryPreferWrite:
            //Assume non-cached write combine
                allocInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                allocInfo.preferredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
                break;
            case HostAccess::SystemMemoryPreferRead:
                //Not necessarily coherent, flush/invalidate might be needed
                allocInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                allocInfo.preferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
                allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
                break;
            case HostAccess::PreferDeviceMemory:
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

        auto tex = new VulkanTexture{dev, desc};
        tex->_img = img;
        tex->_allocation = allocation;

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
        auto elementSize = FormatHelpers::GetSizeInBytes(_desc.format);

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
        auto elementSize = FormatHelpers::GetSizeInBytes(_desc.format);

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

        VK_DEV_CALL(_dev, 
            vkGetImageSubresourceLayout(_dev->LogicalDev(), _img, &subResDesc, &outLayout));

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
    
    VulkanTextureView::~VulkanTextureView() {
        auto& _dev = target->GetDevice();
        _dev.GetFnTable().vkDestroyImageView(_dev.LogicalDev(), view, nullptr);
    }

    
    VkImageView VulkanTextureViewBase::MakeVkView(
        const VulkanDevice& dev, 
        VulkanTexture* target,
        const ITextureView::Description& desc
    ) {
        
        auto& targetDesc = target->GetDesc();
        
        VkImageViewCreateInfo imageViewCI{};
        imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        //VkTexture tex = Util.AssertSubtype<Texture, VkTexture>(description.Target);
        imageViewCI.image = target->GetHandle();
        imageViewCI.format = VdToVkPixelFormat(targetDesc.format, targetDesc.usage.depthStencil);

        auto aspect = desc.aspect;
        if(aspect == ITextureView::Aspect::Auto) {
            aspect = FormatHelpers::GetAspectFromPixelFormat(targetDesc.format);
        }

        VkImageAspectFlags aspectFlags;
        switch (aspect) {

            case Aspect::Depth: {
                assert(targetDesc.usage.depthStencil);
                assert(FormatHelpers::IsDepthStencilFormat(targetDesc.format));
                aspectFlags = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
            }break;

            case Aspect::Stencil: {
                assert(targetDesc.usage.depthStencil);
                assert(FormatHelpers::IsStencilFormat(targetDesc.format));
                aspectFlags = VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT;
            }

            case Aspect::DepthStencil: {
                assert(targetDesc.usage.depthStencil);
                assert(FormatHelpers::IsDepthStencilFormat(targetDesc.format) && 
                       FormatHelpers::IsStencilFormat(targetDesc.format));

                aspectFlags = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT
                            | VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT;
            }break;

            case Aspect::Color: 
            default: {
                aspectFlags = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
            } break;
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
        VK_CHECK(
            dev.GetFnTable().vkCreateImageView(dev.LogicalDev(), &imageViewCI, nullptr, &vkImgView));

        return vkImgView;
    }

	common::sp<VulkanTextureView> VulkanTextureView::Make(
		const common::sp<VulkanTexture>& target,
		const ITextureView::Description& desc
	){
        auto& dev = target->GetDevice();

        VkImageView vkImgView
            = VulkanTextureViewBase::MakeVkView(dev, target.get(), desc);

        auto texView = new VulkanTextureView(target, vkImgView, desc);

        return common::sp{ texView };
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
        VK_DEV_CALL(_dev, vkDestroySampler(_dev->LogicalDev(), _sampler, nullptr));
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
        VK_DEV_CALL(dev, vkCreateSampler(dev->LogicalDev(), &samplerCI, nullptr, &rawSampler));

        auto* sampler = new VulkanSampler(dev, desc);
        sampler->_sampler = rawSampler;
        return common::sp<VulkanSampler>(sampler);
    }

    void VulkanTexture::SetDebugName(const std::string& name) {
        // Check for a valid function pointer

        if (_dev->GetContext().GetCaps().hasDebugUtilExt)
        {
            VkDebugUtilsObjectNameInfoEXT nameInfo = {};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
            nameInfo.objectHandle = (uint64_t)_img;
            nameInfo.pObjectName = name.c_str();
            VK_INST_CALL(_dev, vkSetDebugUtilsObjectNameEXT(_dev->LogicalDev(), &nameInfo));
        }

        _debugName = name;
    }


    
    void VulkanSampler::SetDebugName(const std::string& name) {
        // Check for a valid function pointer

        if (_dev->GetContext().GetCaps().hasDebugUtilExt)
        {
            VkDebugUtilsObjectNameInfoEXT nameInfo = {};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.objectType = VK_OBJECT_TYPE_SAMPLER;
            nameInfo.objectHandle = (uint64_t)_sampler;
            nameInfo.pObjectName = name.c_str();
            VK_INST_CALL(_dev, vkSetDebugUtilsObjectNameEXT(_dev->LogicalDev(), &nameInfo));
        }

        _debugName = name;
    }
}
