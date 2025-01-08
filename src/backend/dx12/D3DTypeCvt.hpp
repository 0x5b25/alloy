#pragma once
//3rd-party headers

//alloy public headers
#include "alloy/Types.hpp"
#include "alloy/FixedFunctions.hpp"
#include "alloy/BindableResource.hpp"
#include "alloy/Sampler.hpp"
#include "alloy/Texture.hpp"

//standard library headers

//backend specific headers

//platform specific headers
#include <d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers


namespace alloy::dxc {
    
    D3D12_TEXTURE_ADDRESS_MODE VdToD3DSamplerAddressMode(ISampler::Description::AddressMode mode);

    D3D12_FILTER VdToD3DFilter(ISampler::Description::SamplerFilter filter, bool enableComparison);

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

    DXGI_FORMAT VdToD3DIndexFormat(IndexFormat format);

    D3D12_BLEND VdToD3DBlendFactor(alloy::BlendStateDescription::BlendFactor factor);

    D3D12_COMPARISON_FUNC VdToD3DCompareOp(ComparisonKind comparisonKind);

    DXGI_FORMAT VdToD3DPixelFormat(const PixelFormat& format, bool toDepthFormat = false);
    

    PixelFormat D3DToVdPixelFormat(DXGI_FORMAT d3dFormat);
}
