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
#include <directx/d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers


namespace Veldrid {
    
    D3D12_TEXTURE_ADDRESS_MODE VdToD3DSamplerAddressMode(Sampler::Description::AddressMode mode);

    D3D12_FILTER VdToD3DFilter(Sampler::Description::SamplerFilter filter, bool enableComparison);

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
    //    Shader::Stage stage);

    //VkShaderStageFlagBits VdToVkShaderStageSingle(
    //    Shader::Stage stage);
     
    //VkBorderColor VdToVkSamplerBorderColor(Sampler::Description::BorderColor borderColor);

    //VkIndexType VdToVkIndexFormat(IndexFormat format);

    D3D12_BLEND VdToD3DBlendFactor(Veldrid::BlendStateDescription::BlendFactor factor);

    D3D12_COMPARISON_FUNC VdToD3DCompareOp(ComparisonKind comparisonKind);

    DXGI_FORMAT VdToD3DPixelFormat(const PixelFormat& format, bool toDepthFormat = false);
    

    PixelFormat D3DToVdPixelFormat(DXGI_FORMAT d3dFormat);
}
