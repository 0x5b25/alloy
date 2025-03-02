#pragma once

#include <volk.h>

#include "alloy/Types.hpp"
#include "alloy/FixedFunctions.hpp"
#include "alloy/BindableResource.hpp"
#include "alloy/Sampler.hpp"
#include "alloy/Texture.hpp"

namespace alloy::vk {
    
    VkSamplerAddressMode VdToVkSamplerAddressMode(ISampler::Description::AddressMode mode);

    void GetFilterParams(
        ISampler::Description::SamplerFilter filter,
        VkFilter& minFilter,
        VkFilter& magFilter,
        VkSamplerMipmapMode& mipmapMode);

    VkDescriptorType VdToVkDescriptorType(
        IBindableResource::ResourceKind kind, 
        IResourceLayout::ShaderResourceDescription::Options options);

    VkSampleCountFlagBits VdToVkSampleCount(SampleCount sampleCount);

    VkStencilOp VdToVkStencilOp(DepthStencilStateDescription::StencilBehavior::Operation op);

    VkPolygonMode VdToVkPolygonMode(RasterizerStateDescription::PolygonFillMode fillMode);

    VkCullModeFlags VdToVkCullMode(RasterizerStateDescription::FaceCullMode cullMode);

    VkBlendOp VdToVkBlendOp(AttachmentStateDescription::BlendFunction func);

    VkColorComponentFlags VdToVkColorWriteMask(
        const AttachmentStateDescription::ColorWriteMask& mask
    );

    VkPrimitiveTopology VdToVkPrimitiveTopology(PrimitiveTopology topology);

    std::uint32_t GetSpecializationConstantSize(ShaderConstantType type);

    VkFormat VdToVkShaderDataType(ShaderDataType format);

    VkShaderStageFlags VdToVkShaderStages(
        IShader::Stages stages);

    VkShaderStageFlagBits VdToVkShaderStageSingle(
        IShader::Stage stage);
     
    VkBorderColor VdToVkSamplerBorderColor(ISampler::Description::BorderColor borderColor);

    VkIndexType VdToVkIndexFormat(IndexFormat format);

    VkBlendFactor VdToVkBlendFactor(alloy::AttachmentStateDescription::BlendFactor factor);

    VkCompareOp VdToVkCompareOp(ComparisonKind comparisonKind);

    VkFormat VdToVkPixelFormat(const PixelFormat& format, bool toDepthFormat = false);

    PixelFormat VkToVdPixelFormat(VkFormat vkFormat);
}
