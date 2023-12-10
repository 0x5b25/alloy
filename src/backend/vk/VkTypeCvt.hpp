#pragma once

#include <volk.h>

#include "veldrid/Types.hpp"
#include "veldrid/FixedFunctions.hpp"
#include "veldrid/BindableResource.hpp"
#include "veldrid/Sampler.hpp"
#include "veldrid/Texture.hpp"

namespace Veldrid::VK::priv {
    
    VkSamplerAddressMode VdToVkSamplerAddressMode(Sampler::Description::AddressMode mode);

    void GetFilterParams(
        Sampler::Description::SamplerFilter filter,
        VkFilter& minFilter,
        VkFilter& magFilter,
        VkSamplerMipmapMode& mipmapMode);

    VkDescriptorType VdToVkDescriptorType(
        IBindableResource::ResourceKind kind, 
        ResourceLayout::Description::ElementDescription::Options options);

    VkSampleCountFlagBits VdToVkSampleCount(Texture::Description::SampleCount sampleCount);

    VkStencilOp VdToVkStencilOp(DepthStencilStateDescription::StencilBehavior::Operation op);

    VkPolygonMode VdToVkPolygonMode(RasterizerStateDescription::PolygonFillMode fillMode);

    VkCullModeFlags VdToVkCullMode(RasterizerStateDescription::FaceCullMode cullMode);

    VkBlendOp VdToVkBlendOp(BlendStateDescription::BlendFunction func);

    VkColorComponentFlags VdToVkColorWriteMask(
        const BlendStateDescription::ColorWriteMask& mask
    );

    VkPrimitiveTopology VdToVkPrimitiveTopology(PrimitiveTopology topology);

    std::uint32_t GetSpecializationConstantSize(ShaderConstantType type);

    VkFormat VdToVkShaderDataType(ShaderDataType format);

    VkShaderStageFlags VdToVkShaderStages(
        Shader::Description::Stage stage);

    VkShaderStageFlagBits VdToVkShaderStageSingle(
        Shader::Description::Stage stage);
     
    VkBorderColor VdToVkSamplerBorderColor(Sampler::Description::BorderColor borderColor);

    VkIndexType VdToVkIndexFormat(IndexFormat format);

    VkBlendFactor VdToVkBlendFactor(Veldrid::BlendStateDescription::BlendFactor factor);

    VkCompareOp VdToVkCompareOp(ComparisonKind comparisonKind);

    VkFormat VdToVkPixelFormat(const PixelFormat& format, bool toDepthFormat = false);

    PixelFormat VkToVdPixelFormat(VkFormat vkFormat);
}
