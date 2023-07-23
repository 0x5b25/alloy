#pragma once
//3rd-party headers

//veldrid public headers
#include "veldrid/Types.hpp"
#include "veldrid/FixedFunctions.hpp"
#include "veldrid/BindableResource.hpp"
#include "veldrid/Sampler.hpp"
#include "veldrid/Texture.hpp"

//standard library headers

//backend specific headers

//platform specific headers
#include <d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers


namespace Veldrid {
    
    //VkSamplerAddressMode VdToVkSamplerAddressMode(Sampler::Description::AddressMode mode);

    //void GetFilterParams(
    //    Sampler::Description::SamplerFilter filter,
    //    VkFilter& minFilter,
    //    VkFilter& magFilter,
    //    VkSamplerMipmapMode& mipmapMode);

    //VkDescriptorType VdToVkDescriptorType(
    //    ResourceLayout::Description::ElementDescription::ResourceKind kind, 
    //    ResourceLayout::Description::ElementDescription::Options options);

    //VkSampleCountFlagBits VdToVkSampleCount(Texture::Description::SampleCount sampleCount);

    D3D12_STENCIL_OP VdToD3DStencilOp(DepthStencilStateDescription::StencilBehavior::Operation op);

    D3D12_FILL_MODE VdToD3DPolygonMode(RasterizerStateDescription::PolygonFillMode fillMode);

    D3D12_CULL_MODE VdToD3DCullMode(RasterizerStateDescription::FaceCullMode cullMode);

    D3D12_BLEND_OP VdToD3DBlendOp(BlendStateDescription::BlendFunction func);

    std::uint8_t VdToD3DColorWriteMask(
        const BlendStateDescription::ColorWriteMask& mask
    );

    //VkPrimitiveTopology VdToVkPrimitiveTopology(PrimitiveTopology topology);

    //std::uint32_t GetSpecializationConstantSize(ShaderConstantType type);

    DXGI_FORMAT VdToD3DShaderDataType(ShaderDataType format);

    //VkShaderStageFlags VdToVkShaderStages(
    //    Shader::Description::Stage stage);

    //VkShaderStageFlagBits VdToVkShaderStageSingle(
    //    Shader::Description::Stage stage);
     
    //VkBorderColor VdToVkSamplerBorderColor(Sampler::Description::BorderColor borderColor);

    //VkIndexType VdToVkIndexFormat(IndexFormat format);

    D3D12_BLEND VdToD3DBlendFactor(Veldrid::BlendStateDescription::BlendFactor factor);

    D3D12_COMPARISON_FUNC VdToD3DCompareOp(ComparisonKind comparisonKind);

    DXGI_FORMAT VdToD3DPixelFormat(const PixelFormat& format, bool toDepthFormat = false);
    

    PixelFormat D3DToVdPixelFormat(DXGI_FORMAT d3dFormat);
}
