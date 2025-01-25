#include "D3DTypeCvt.hpp"

namespace alloy::dxc {

    D3D12_TEXTURE_ADDRESS_MODE VdToD3DSamplerAddressMode(ISampler::Description::AddressMode mode)
    {
        using SamplerAddressMode = typename ISampler::Description::AddressMode;
        switch (mode)
        {
        case SamplerAddressMode::Wrap:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case SamplerAddressMode::Mirror:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case SamplerAddressMode::Clamp:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case SamplerAddressMode::Border:
            return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        default:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        }
    }

    
    D3D12_FILTER VdToD3DFilter(ISampler::Description::SamplerFilter filter, bool enableComparison)
    {
        using SamplerFilter = typename ISampler::Description::SamplerFilter;
        switch (filter)
        {
        case SamplerFilter::Anisotropic: return D3D12_FILTER_ANISOTROPIC;
        case SamplerFilter::MinPoint_MagPoint_MipPoint: 
            return enableComparison ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT
                                    : D3D12_FILTER_MIN_MAG_MIP_POINT;
        case SamplerFilter::MinPoint_MagPoint_MipLinear: 
            return enableComparison ? D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR
                                    : D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
        case SamplerFilter::MinPoint_MagLinear_MipPoint: 
            return enableComparison ? D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT
                                    : D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        case SamplerFilter::MinPoint_MagLinear_MipLinear: 
            return enableComparison ? D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR
                                    : D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        case SamplerFilter::MinLinear_MagPoint_MipPoint: 
            return enableComparison ? D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT
                                    : D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        case SamplerFilter::MinLinear_MagPoint_MipLinear: 
            return enableComparison ? D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR
                                    : D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        case SamplerFilter::MinLinear_MagLinear_MipPoint: 
            return enableComparison ? D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT
                                    : D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        case SamplerFilter::MinLinear_MagLinear_MipLinear: 
            return enableComparison ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR
                                    : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        default : return D3D12_FILTER_MIN_MAG_MIP_POINT;
        }
    }

     
/*
    VkDescriptorType VdToVkDescriptorType(
        ResourceLayout::Description::ElementDescription::ResourceKind kind, 
        ResourceLayout::Description::ElementDescription::Options options)
    {
        using ResourceKind = typename ResourceLayout::Description::ElementDescription::ResourceKind;
        bool dynamicBinding = (options.dynamicBinding) != 0;
        switch (kind)
        {
        case ResourceKind::UniformBuffer:
            return dynamicBinding 
                ? VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC 
                : VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case ResourceKind::StructuredBufferReadWrite:
        case ResourceKind::StructuredBufferReadOnly:
            return dynamicBinding 
                ? VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
                : VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case ResourceKind::TextureReadOnly:
            return VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case ResourceKind::TextureReadWrite:
            return VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case ResourceKind::Sampler:
            return VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER;
        default:
            return VkDescriptorType::VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    }

     VkSampleCountFlagBits VdToVkSampleCount(Texture::Description::SampleCount sampleCount)
    {
        using SampleCount = typename Texture::Description::SampleCount;
        switch (sampleCount)
        {
        case SampleCount::x1:
            return VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
        case SampleCount::x2:
            return VkSampleCountFlagBits::VK_SAMPLE_COUNT_2_BIT;
        case SampleCount::x4:
            return VkSampleCountFlagBits::VK_SAMPLE_COUNT_4_BIT;
        case SampleCount::x8:
            return VkSampleCountFlagBits::VK_SAMPLE_COUNT_8_BIT;
        case SampleCount::x16:
            return VkSampleCountFlagBits::VK_SAMPLE_COUNT_16_BIT;
        case SampleCount::x32:
            return VkSampleCountFlagBits::VK_SAMPLE_COUNT_32_BIT;
        default:
            return VkSampleCountFlagBits::VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
        }
    }
*/
    D3D12_STENCIL_OP VdToD3DStencilOp(DepthStencilStateDescription::StencilBehavior::Operation op)
    {
        using StencilOperation = typename DepthStencilStateDescription::StencilBehavior::Operation;
        switch (op)
        {
        case StencilOperation::Keep:
            return D3D12_STENCIL_OP::D3D12_STENCIL_OP_KEEP;
        case StencilOperation::Zero:
            return D3D12_STENCIL_OP::D3D12_STENCIL_OP_ZERO;
        case StencilOperation::Replace:
            return D3D12_STENCIL_OP::D3D12_STENCIL_OP_REPLACE;
        case StencilOperation::IncrementAndClamp:
            return D3D12_STENCIL_OP::D3D12_STENCIL_OP_INCR_SAT;
        case StencilOperation::DecrementAndClamp:
            return D3D12_STENCIL_OP::D3D12_STENCIL_OP_DECR_SAT;
        case StencilOperation::Invert:
            return D3D12_STENCIL_OP::D3D12_STENCIL_OP_INVERT;
        case StencilOperation::IncrementAndWrap:
            return D3D12_STENCIL_OP::D3D12_STENCIL_OP_INCR;
        case StencilOperation::DecrementAndWrap:
            return D3D12_STENCIL_OP::D3D12_STENCIL_OP_DECR;
        default:
            return (D3D12_STENCIL_OP)0;
        }
    }

    D3D12_FILL_MODE VdToD3DPolygonMode(RasterizerStateDescription::PolygonFillMode fillMode)
    {
        using PolygonFillMode = typename RasterizerStateDescription::PolygonFillMode;
        switch (fillMode)
        {
        case PolygonFillMode::Solid:
            return D3D12_FILL_MODE::D3D12_FILL_MODE_SOLID;
        case PolygonFillMode::Wireframe:
            return D3D12_FILL_MODE::D3D12_FILL_MODE_WIREFRAME;
        default:
            return (D3D12_FILL_MODE)0;
        }
    }

    D3D12_CULL_MODE VdToD3DCullMode(RasterizerStateDescription::FaceCullMode cullMode)
    {
        switch (cullMode)
        {
        case RasterizerStateDescription::FaceCullMode::Back:
            return D3D12_CULL_MODE::D3D12_CULL_MODE_BACK;
        case RasterizerStateDescription::FaceCullMode::Front:
            return D3D12_CULL_MODE::D3D12_CULL_MODE_FRONT;
        case RasterizerStateDescription::FaceCullMode::None:
            return D3D12_CULL_MODE::D3D12_CULL_MODE_NONE;
        default:
            return (D3D12_CULL_MODE)0;
        }
    }

    D3D12_BLEND_OP VdToD3DBlendOp(AttachmentStateDescription::BlendFunction func)
    {
        switch (func)
        {
        case AttachmentStateDescription::BlendFunction::Add:
            return D3D12_BLEND_OP::D3D12_BLEND_OP_ADD;
        case AttachmentStateDescription::BlendFunction::Subtract:
            return D3D12_BLEND_OP::D3D12_BLEND_OP_SUBTRACT;
        case AttachmentStateDescription::BlendFunction::ReverseSubtract:
            return D3D12_BLEND_OP::D3D12_BLEND_OP_REV_SUBTRACT;
        case AttachmentStateDescription::BlendFunction::Minimum:
            return D3D12_BLEND_OP::D3D12_BLEND_OP_MIN;
        case AttachmentStateDescription::BlendFunction::Maximum:
            return D3D12_BLEND_OP::D3D12_BLEND_OP_MAX;
        default:
            return (D3D12_BLEND_OP)0;
        }
    }

    std::uint8_t VdToD3DColorWriteMask(
        const AttachmentStateDescription::ColorWriteMask& mask
    ) {
        std::uint8_t flags = 0;

        if (mask.r) flags |= D3D12_COLOR_WRITE_ENABLE_RED;
        if (mask.g) flags |= D3D12_COLOR_WRITE_ENABLE_GREEN;
        if (mask.b) flags |= D3D12_COLOR_WRITE_ENABLE_BLUE;
        if (mask.a) flags |= D3D12_COLOR_WRITE_ENABLE_ALPHA;

        return flags;
    }
/*
     VkPrimitiveTopology VdToVkPrimitiveTopology(PrimitiveTopology topology)
    {
        switch (topology)
        {
        case PrimitiveTopology::TriangleList:
            return VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::TriangleStrip:
            return VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveTopology::LineList:
            return VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::LineStrip:
            return VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PrimitiveTopology::PointList:
            return VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        default:
            return VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
        }
    }

    std::uint32_t GetSpecializationConstantSize(ShaderConstantType type)
    {
        switch (type)
        {
        case ShaderConstantType::Bool:
            return 4;
        case ShaderConstantType::UInt16:
            return 2;
        case ShaderConstantType::Int16:
            return 2;
        case ShaderConstantType::UInt32:
            return 4;
        case ShaderConstantType::Int32:
            return 4;
        case ShaderConstantType::UInt64:
            return 8;
        case ShaderConstantType::Int64:
            return 8;
        case ShaderConstantType::Float:
            return 4;
        case ShaderConstantType::Double:
            return 8;
        default:
            assert(false);
            return 0;
        }
    }

    
*/
/*typedef enum DXGI_FORMAT
{
    DXGI_FORMAT_UNKNOWN	                                = 0,
    DXGI_FORMAT_R32G32B32A32_TYPELESS                   = 1,
    DXGI_FORMAT_R32G32B32A32_FLOAT                      = 2,
    DXGI_FORMAT_R32G32B32A32_UINT                       = 3,
    DXGI_FORMAT_R32G32B32A32_SINT                       = 4,
    DXGI_FORMAT_R32G32B32_TYPELESS                      = 5,
    DXGI_FORMAT_R32G32B32_FLOAT                         = 6,
    DXGI_FORMAT_R32G32B32_UINT                          = 7,
    DXGI_FORMAT_R32G32B32_SINT                          = 8,
    DXGI_FORMAT_R16G16B16A16_TYPELESS                   = 9,
    DXGI_FORMAT_R16G16B16A16_FLOAT                      = 10,
    DXGI_FORMAT_R16G16B16A16_UNORM                      = 11,
    DXGI_FORMAT_R16G16B16A16_UINT                       = 12,
    DXGI_FORMAT_R16G16B16A16_SNORM                      = 13,
    DXGI_FORMAT_R16G16B16A16_SINT                       = 14,
    DXGI_FORMAT_R32G32_TYPELESS                         = 15,
    DXGI_FORMAT_R32G32_FLOAT                            = 16,
    DXGI_FORMAT_R32G32_UINT                             = 17,
    DXGI_FORMAT_R32G32_SINT                             = 18,
    DXGI_FORMAT_R32G8X24_TYPELESS                       = 19,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT                    = 20,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS                = 21,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT                 = 22,
    DXGI_FORMAT_R10G10B10A2_TYPELESS                    = 23,
    DXGI_FORMAT_R10G10B10A2_UNORM                       = 24,
    DXGI_FORMAT_R10G10B10A2_UINT                        = 25,
    DXGI_FORMAT_R11G11B10_FLOAT                         = 26,
    DXGI_FORMAT_R8G8B8A8_TYPELESS                       = 27,
    DXGI_FORMAT_R8G8B8A8_UNORM                          = 28,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB                     = 29,
    DXGI_FORMAT_R8G8B8A8_UINT                           = 30,
    DXGI_FORMAT_R8G8B8A8_SNORM                          = 31,
    DXGI_FORMAT_R8G8B8A8_SINT                           = 32,
    DXGI_FORMAT_R16G16_TYPELESS                         = 33,
    DXGI_FORMAT_R16G16_FLOAT                            = 34,
    DXGI_FORMAT_R16G16_UNORM                            = 35,
    DXGI_FORMAT_R16G16_UINT                             = 36,
    DXGI_FORMAT_R16G16_SNORM                            = 37,
    DXGI_FORMAT_R16G16_SINT                             = 38,
    DXGI_FORMAT_R32_TYPELESS                            = 39,
    DXGI_FORMAT_D32_FLOAT                               = 40,
    DXGI_FORMAT_R32_FLOAT                               = 41,
    DXGI_FORMAT_R32_UINT                                = 42,
    DXGI_FORMAT_R32_SINT                                = 43,
    DXGI_FORMAT_R24G8_TYPELESS                          = 44,
    DXGI_FORMAT_D24_UNORM_S8_UINT                       = 45,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS                   = 46,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT                    = 47,
    DXGI_FORMAT_R8G8_TYPELESS                           = 48,
    DXGI_FORMAT_R8G8_UNORM                              = 49,
    DXGI_FORMAT_R8G8_UINT                               = 50,
    DXGI_FORMAT_R8G8_SNORM                              = 51,
    DXGI_FORMAT_R8G8_SINT                               = 52,
    DXGI_FORMAT_R16_TYPELESS                            = 53,
    DXGI_FORMAT_R16_FLOAT                               = 54,
    DXGI_FORMAT_D16_UNORM                               = 55,
    DXGI_FORMAT_R16_UNORM                               = 56,
    DXGI_FORMAT_R16_UINT                                = 57,
    DXGI_FORMAT_R16_SNORM                               = 58,
    DXGI_FORMAT_R16_SINT                                = 59,
    DXGI_FORMAT_R8_TYPELESS                             = 60,
    DXGI_FORMAT_R8_UNORM                                = 61,
    DXGI_FORMAT_R8_UINT                                 = 62,
    DXGI_FORMAT_R8_SNORM                                = 63,
    DXGI_FORMAT_R8_SINT                                 = 64,
    DXGI_FORMAT_A8_UNORM                                = 65,
    DXGI_FORMAT_R1_UNORM                                = 66,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP                      = 67,
    DXGI_FORMAT_R8G8_B8G8_UNORM                         = 68,
    DXGI_FORMAT_G8R8_G8B8_UNORM                         = 69,
    DXGI_FORMAT_BC1_TYPELESS                            = 70,
    DXGI_FORMAT_BC1_UNORM                               = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB                          = 72,
    DXGI_FORMAT_BC2_TYPELESS                            = 73,
    DXGI_FORMAT_BC2_UNORM                               = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB                          = 75,
    DXGI_FORMAT_BC3_TYPELESS                            = 76,
    DXGI_FORMAT_BC3_UNORM                               = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB                          = 78,
    DXGI_FORMAT_BC4_TYPELESS                            = 79,
    DXGI_FORMAT_BC4_UNORM                               = 80,
    DXGI_FORMAT_BC4_SNORM                               = 81,
    DXGI_FORMAT_BC5_TYPELESS                            = 82,
    DXGI_FORMAT_BC5_UNORM                               = 83,
    DXGI_FORMAT_BC5_SNORM                               = 84,
    DXGI_FORMAT_B5G6R5_UNORM                            = 85,
    DXGI_FORMAT_B5G5R5A1_UNORM                          = 86,
    DXGI_FORMAT_B8G8R8A8_UNORM                          = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM                          = 88,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM              = 89,
    DXGI_FORMAT_B8G8R8A8_TYPELESS                       = 90,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB                     = 91,
    DXGI_FORMAT_B8G8R8X8_TYPELESS                       = 92,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB                     = 93,
    DXGI_FORMAT_BC6H_TYPELESS                           = 94,
    DXGI_FORMAT_BC6H_UF16                               = 95,
    DXGI_FORMAT_BC6H_SF16                               = 96,
    DXGI_FORMAT_BC7_TYPELESS                            = 97,
    DXGI_FORMAT_BC7_UNORM                               = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB                          = 99,
    DXGI_FORMAT_AYUV                                    = 100,
    DXGI_FORMAT_Y410                                    = 101,
    DXGI_FORMAT_Y416                                    = 102,
    DXGI_FORMAT_NV12                                    = 103,
    DXGI_FORMAT_P010                                    = 104,
    DXGI_FORMAT_P016                                    = 105,
    DXGI_FORMAT_420_OPAQUE                              = 106,
    DXGI_FORMAT_YUY2                                    = 107,
    DXGI_FORMAT_Y210                                    = 108,
    DXGI_FORMAT_Y216                                    = 109,
    DXGI_FORMAT_NV11                                    = 110,
    DXGI_FORMAT_AI44                                    = 111,
    DXGI_FORMAT_IA44                                    = 112,
    DXGI_FORMAT_P8                                      = 113,
    DXGI_FORMAT_A8P8                                    = 114,
    DXGI_FORMAT_B4G4R4A4_UNORM                          = 115,

    DXGI_FORMAT_P208                                    = 130,
    DXGI_FORMAT_V208                                    = 131,
    DXGI_FORMAT_V408                                    = 132,


    DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE         = 189,
    DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE = 190,

*/
    DXGI_FORMAT VdToD3DShaderDataType(ShaderDataType format)
    {
        switch (format)
        {
        case ShaderDataType::Float1:
            return DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;// ::VK_FORMAT_R32_SFLOAT;
        case ShaderDataType::Float2:
            return DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT;//DXGI_FORMAT::DXGI_FORMAT_R32G32_SFLOAT;
        case ShaderDataType::Float3:
            return DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT;//DXGI_FORMAT::DXGI_FORMAT_R32G32B32_SFLOAT;
        case ShaderDataType::Float4:
            return DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;//DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_SFLOAT;
        case ShaderDataType::Byte2_Norm:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8_UNORM;//DXGI_FORMAT::DXGI_FORMAT_R8G8_UNORM;
        case ShaderDataType::Byte2:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8_UINT;//DXGI_FORMAT::DXGI_FORMAT_R8G8_UINT;
        case ShaderDataType::Byte4_Norm:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;//DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
        case ShaderDataType::Byte4:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UINT;
        case ShaderDataType::SByte2_Norm:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8_SNORM;
        case ShaderDataType::SByte2:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8_SINT;
        case ShaderDataType::SByte4_Norm:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_SNORM;
        case ShaderDataType::SByte4:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_SINT;
        case ShaderDataType::UShort2_Norm:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16_UNORM;
        case ShaderDataType::UShort2:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16_UINT;
        case ShaderDataType::UShort4_Norm:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_UNORM;
        case ShaderDataType::UShort4:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_UINT;
        case ShaderDataType::Short2_Norm:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16_SNORM;
        case ShaderDataType::Short2:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16_SINT;
        case ShaderDataType::Short4_Norm:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_SNORM;
        case ShaderDataType::Short4:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_SINT;
        case ShaderDataType::UInt1:
            return DXGI_FORMAT::DXGI_FORMAT_R32_UINT;
        case ShaderDataType::UInt2:
            return DXGI_FORMAT::DXGI_FORMAT_R32G32_UINT;
        case ShaderDataType::UInt3:
            return DXGI_FORMAT::DXGI_FORMAT_R32G32B32_UINT;
        case ShaderDataType::UInt4:
            return DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_UINT;
        case ShaderDataType::Int1:
            return DXGI_FORMAT::DXGI_FORMAT_R32_SINT;
        case ShaderDataType::Int2:
            return DXGI_FORMAT::DXGI_FORMAT_R32G32_SINT;
        case ShaderDataType::Int3:
            return DXGI_FORMAT::DXGI_FORMAT_R32G32B32_SINT;
        case ShaderDataType::Int4:
            return DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_SINT;
        case ShaderDataType::Half1:
            return DXGI_FORMAT::DXGI_FORMAT_R16_FLOAT;
        case ShaderDataType::Half2:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16_FLOAT;
        case ShaderDataType::Half4:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT;
        default:
            return (DXGI_FORMAT)0;
        }
    }/*

    VkShaderStageFlags VdToVkShaderStages(Shader::Stage stage){
        VkShaderStageFlags flag = 0;
        if (stage.vertex)                 flag |= VK_SHADER_STAGE_VERTEX_BIT;
        if (stage.geometry)               flag |= VK_SHADER_STAGE_GEOMETRY_BIT;
        if (stage.tessellationControl)    flag |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        if (stage.tessellationEvaluation) flag |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        if (stage.fragment)               flag |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (stage.compute)                flag |= VK_SHADER_STAGE_COMPUTE_BIT;
        return flag;

    }

    VkShaderStageFlagBits VdToVkShaderStageSingle(Shader::Stage stage) {
        if (stage.vertex)                 return VK_SHADER_STAGE_VERTEX_BIT;
        if (stage.geometry)               return VK_SHADER_STAGE_GEOMETRY_BIT;
        if (stage.tessellationControl)    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        if (stage.tessellationEvaluation) return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        if (stage.fragment)               return VK_SHADER_STAGE_FRAGMENT_BIT;
        if (stage.compute)                return VK_SHADER_STAGE_COMPUTE_BIT;
        return VkShaderStageFlagBits::VK_SHADER_STAGE_ALL;
    }

     
    VkBorderColor VdToVkSamplerBorderColor(Sampler::Description::BorderColor borderColor)
    {
        using SamplerBorderColor = typename Sampler::Description::BorderColor;
        switch (borderColor)
        {
        case SamplerBorderColor::TransparentBlack:
            return VkBorderColor::VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        case SamplerBorderColor::OpaqueBlack:
            return VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        case SamplerBorderColor::OpaqueWhite:
            return VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        default:
            return VkBorderColor::VK_BORDER_COLOR_MAX_ENUM;
        }
    }


*/
    DXGI_FORMAT VdToD3DIndexFormat(IndexFormat format)
    {
        switch (format)
        {
        case IndexFormat::UInt16:
            return DXGI_FORMAT::DXGI_FORMAT_R16_UINT;
        case IndexFormat::UInt32:
            return DXGI_FORMAT::DXGI_FORMAT_R32_UINT;
        default:
            return DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
        }
    }

    D3D12_BLEND VdToD3DBlendFactor(alloy::AttachmentStateDescription::BlendFactor factor)
    {
        using BlendFactor = typename alloy::AttachmentStateDescription::BlendFactor;
        switch (factor)
        {
        case BlendFactor::Zero:
            return D3D12_BLEND::D3D12_BLEND_ZERO;
        case BlendFactor::One:
            return D3D12_BLEND::D3D12_BLEND_ONE;
        case BlendFactor::SourceAlpha:
            return D3D12_BLEND::D3D12_BLEND_SRC_ALPHA;
        case BlendFactor::InverseSourceAlpha:
            return D3D12_BLEND::D3D12_BLEND_INV_SRC_ALPHA;
        case BlendFactor::DestinationAlpha:
            return D3D12_BLEND::D3D12_BLEND_DEST_ALPHA;
        case BlendFactor::InverseDestinationAlpha:
            return D3D12_BLEND::D3D12_BLEND_INV_DEST_ALPHA;
        case BlendFactor::SourceColor:
            return D3D12_BLEND::D3D12_BLEND_SRC_COLOR;
        case BlendFactor::InverseSourceColor:
            return D3D12_BLEND::D3D12_BLEND_INV_SRC_COLOR;
        case BlendFactor::DestinationColor:
            return D3D12_BLEND::D3D12_BLEND_DEST_COLOR;
        case BlendFactor::InverseDestinationColor:
            return D3D12_BLEND::D3D12_BLEND_INV_DEST_COLOR;
        case BlendFactor::BlendFactor:
            return D3D12_BLEND::D3D12_BLEND_BLEND_FACTOR;
        case BlendFactor::InverseBlendFactor:
            return D3D12_BLEND::D3D12_BLEND_INV_BLEND_FACTOR;
        default:
            return (D3D12_BLEND)0;
        }
    }


    D3D12_COMPARISON_FUNC VdToD3DCompareOp(ComparisonKind comparisonKind)
    {
        switch (comparisonKind)
        {
        case ComparisonKind::Never:
            return D3D12_COMPARISON_FUNC::D3D12_COMPARISON_FUNC_NEVER;
        case ComparisonKind::Less:
            return D3D12_COMPARISON_FUNC::D3D12_COMPARISON_FUNC_LESS;
        case ComparisonKind::Equal:
            return D3D12_COMPARISON_FUNC::D3D12_COMPARISON_FUNC_EQUAL;
        case ComparisonKind::LessEqual:
            return D3D12_COMPARISON_FUNC::D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case ComparisonKind::Greater:
            return D3D12_COMPARISON_FUNC::D3D12_COMPARISON_FUNC_GREATER;
        case ComparisonKind::NotEqual:
            return D3D12_COMPARISON_FUNC::D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case ComparisonKind::GreaterEqual:
            return D3D12_COMPARISON_FUNC::D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case ComparisonKind::Always:
            return D3D12_COMPARISON_FUNC::D3D12_COMPARISON_FUNC_ALWAYS;
        default:
            return (D3D12_COMPARISON_FUNC)0;
        }
    }

    DXGI_FORMAT VdToD3DPixelFormat(const PixelFormat& format, bool toDepthFormat)
    {
        switch (format)
        {
        case PixelFormat::R8_UNorm:
            return DXGI_FORMAT::DXGI_FORMAT_R8_UNORM;
        case PixelFormat::R8_SNorm:
            return DXGI_FORMAT::DXGI_FORMAT_R8_SNORM;
        case PixelFormat::R8_UInt:
            return DXGI_FORMAT::DXGI_FORMAT_R8_UINT;
        case PixelFormat::R8_SInt:
            return DXGI_FORMAT::DXGI_FORMAT_R8_SINT;

        case PixelFormat::R16_UNorm:
            return toDepthFormat
                ? DXGI_FORMAT::DXGI_FORMAT_D16_UNORM
                : DXGI_FORMAT::DXGI_FORMAT_R16_UNORM;
        case PixelFormat::R16_SNorm:
            return DXGI_FORMAT::DXGI_FORMAT_R16_SNORM;
        case PixelFormat::R16_UInt:
            return DXGI_FORMAT::DXGI_FORMAT_R16_UINT;
        case PixelFormat::R16_SInt:
            return DXGI_FORMAT::DXGI_FORMAT_R16_SINT;
        case PixelFormat::R16_Float:
            return DXGI_FORMAT::DXGI_FORMAT_R16_FLOAT;

        case PixelFormat::R32_UInt:
            return DXGI_FORMAT::DXGI_FORMAT_R32_UINT;
        case PixelFormat::R32_SInt:
            return DXGI_FORMAT::DXGI_FORMAT_R32_SINT;
        case PixelFormat::R32_Float:
            return toDepthFormat
                ? DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT
                : DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;

        case PixelFormat::R8_G8_UNorm:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8_UNORM;
        case PixelFormat::R8_G8_SNorm:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8_SNORM;
        case PixelFormat::R8_G8_UInt:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8_UINT;
        case PixelFormat::R8_G8_SInt:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8_SINT;

        case PixelFormat::R16_G16_UNorm:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16_UNORM;
        case PixelFormat::R16_G16_SNorm:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16_SNORM;
        case PixelFormat::R16_G16_UInt:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16_UINT;
        case PixelFormat::R16_G16_SInt:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16_SINT;
        case PixelFormat::R16_G16_Float:
            //return VkFormat.R16g16b16a16Sfloat; ??
            return DXGI_FORMAT::DXGI_FORMAT_R16G16_FLOAT;

        case PixelFormat::R32_G32_UInt:
            return DXGI_FORMAT::DXGI_FORMAT_R32G32_UINT;
        case PixelFormat::R32_G32_SInt:
            return DXGI_FORMAT::DXGI_FORMAT_R32G32_SINT;
        case PixelFormat::R32_G32_Float:
            return DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT;

        case PixelFormat::R8_G8_B8_A8_UNorm:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
        case PixelFormat::R8_G8_B8_A8_UNorm_SRgb:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case PixelFormat::B8_G8_R8_A8_UNorm:
            return DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;
        case PixelFormat::B8_G8_R8_A8_UNorm_SRgb:
            return DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case PixelFormat::R8_G8_B8_A8_SNorm:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_SNORM;
        case PixelFormat::R8_G8_B8_A8_UInt:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UINT;
        case PixelFormat::R8_G8_B8_A8_SInt:
            return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_SINT;

        case PixelFormat::R16_G16_B16_A16_UNorm:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_UNORM;
        case PixelFormat::R16_G16_B16_A16_SNorm:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_SNORM;
        case PixelFormat::R16_G16_B16_A16_UInt:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_UINT;
        case PixelFormat::R16_G16_B16_A16_SInt:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_SINT;
        case PixelFormat::R16_G16_B16_A16_Float:
            return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT;

        case PixelFormat::R32_G32_B32_A32_UInt:
            return DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_UINT;
        case PixelFormat::R32_G32_B32_A32_SInt:
            return DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_SINT;
        case PixelFormat::R32_G32_B32_A32_Float:
            return DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;

        case PixelFormat::BC1_Rgb_UNorm:
        case PixelFormat::BC1_Rgba_UNorm:
            return DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM;
        case PixelFormat::BC1_Rgb_UNorm_SRgb:
        case PixelFormat::BC1_Rgba_UNorm_SRgb:
            return DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM_SRGB;
        case PixelFormat::BC2_UNorm:
            return DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM;
        case PixelFormat::BC2_UNorm_SRgb:
            return DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM_SRGB;
        case PixelFormat::BC3_UNorm:
            return DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM;
        case PixelFormat::BC3_UNorm_SRgb:
            return DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM_SRGB;
        case PixelFormat::BC4_UNorm:
            return DXGI_FORMAT::DXGI_FORMAT_BC4_UNORM;
        case PixelFormat::BC4_SNorm:
            return DXGI_FORMAT::DXGI_FORMAT_BC4_SNORM;
        case PixelFormat::BC5_UNorm:
            return DXGI_FORMAT::DXGI_FORMAT_BC5_UNORM;
        case PixelFormat::BC5_SNorm:
            return DXGI_FORMAT::DXGI_FORMAT_BC5_SNORM;
        case PixelFormat::BC7_UNorm:
            return DXGI_FORMAT::DXGI_FORMAT_BC7_UNORM;
        case PixelFormat::BC7_UNorm_SRgb:
            return DXGI_FORMAT::DXGI_FORMAT_BC7_UNORM_SRGB;        

        case PixelFormat::D32_Float_S8_UInt:
            return DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        case PixelFormat::D24_UNorm_S8_UInt:
            return DXGI_FORMAT::DXGI_FORMAT_D24_UNORM_S8_UINT;

        case PixelFormat::R10_G10_B10_A2_UNorm:
            return DXGI_FORMAT::DXGI_FORMAT_R10G10B10A2_UNORM;
        case PixelFormat::R10_G10_B10_A2_UInt:
            return DXGI_FORMAT::DXGI_FORMAT_R10G10B10A2_UINT;
        case PixelFormat::R11_G11_B10_Float:
            return DXGI_FORMAT::DXGI_FORMAT_R11G11B10_FLOAT;

        case PixelFormat::ETC2_R8_G8_B8_UNorm:
        case PixelFormat::ETC2_R8_G8_B8_A1_UNorm:
        case PixelFormat::ETC2_R8_G8_B8_A8_UNorm:
            //ETC2 formats are not supported on D3D
            assert(false);
        default:
            //throw new alloyException($"Invalid {nameof(PixelFormat)}: {format}");
            return DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
        }
    }

    PixelFormat D3DToVdPixelFormat(DXGI_FORMAT d3dFormat)
    {
        switch (d3dFormat) {
        case DXGI_FORMAT::DXGI_FORMAT_R8_UNORM:
            return PixelFormat::R8_UNorm;
        case DXGI_FORMAT::DXGI_FORMAT_R8_SNORM:
            return PixelFormat::R8_SNorm;
        case DXGI_FORMAT::DXGI_FORMAT_R8_UINT:
            return PixelFormat::R8_UInt;
        case DXGI_FORMAT::DXGI_FORMAT_R8_SINT:
            return PixelFormat::R8_SInt;

        case DXGI_FORMAT::DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT::DXGI_FORMAT_R16_UNORM:
            return PixelFormat::R16_UNorm;
        case DXGI_FORMAT::DXGI_FORMAT_R16_SNORM:
            return PixelFormat::R16_SNorm;
        case DXGI_FORMAT::DXGI_FORMAT_R16_UINT:
            return PixelFormat::R16_UInt;
        case DXGI_FORMAT::DXGI_FORMAT_R16_SINT:
            return PixelFormat::R16_SInt;
        case DXGI_FORMAT::DXGI_FORMAT_R16_FLOAT:
            return PixelFormat::R16_Float;

        case DXGI_FORMAT::DXGI_FORMAT_R32_UINT:
            return PixelFormat::R32_UInt;
        case DXGI_FORMAT::DXGI_FORMAT_R32_SINT:
            return PixelFormat::R32_SInt;
        case DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT:
            return PixelFormat::R32_Float;

        case DXGI_FORMAT::DXGI_FORMAT_R8G8_UNORM:
            return PixelFormat::R8_G8_UNorm;
        case DXGI_FORMAT::DXGI_FORMAT_R8G8_SNORM:
            return PixelFormat::R8_G8_SNorm;
        case DXGI_FORMAT::DXGI_FORMAT_R8G8_UINT:
            return PixelFormat::R8_G8_UInt;
        case DXGI_FORMAT::DXGI_FORMAT_R8G8_SINT:
            return PixelFormat::R8_G8_SInt;

        case DXGI_FORMAT::DXGI_FORMAT_R16G16_UNORM:
            return PixelFormat::R16_G16_UNorm;
        case DXGI_FORMAT::DXGI_FORMAT_R16G16_SNORM:
            return PixelFormat::R16_G16_SNorm;
        case DXGI_FORMAT::DXGI_FORMAT_R16G16_UINT:
            return PixelFormat::R16_G16_UInt;
        case DXGI_FORMAT::DXGI_FORMAT_R16G16_SINT:
            return PixelFormat::R16_G16_SInt;
        case DXGI_FORMAT::DXGI_FORMAT_R16G16_FLOAT:
            //return VkFormat.R16g16b16a16Sfloat; ??
            return PixelFormat::R16_G16_Float;

        case DXGI_FORMAT::DXGI_FORMAT_R32G32_UINT:
            return PixelFormat::R32_G32_UInt;
        case DXGI_FORMAT::DXGI_FORMAT_R32G32_SINT:
            return PixelFormat::R32_G32_SInt;
        case DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT:
            return PixelFormat::R32_G32_Float;

        case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM:
            return PixelFormat::R8_G8_B8_A8_UNorm;
        case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            return PixelFormat::R8_G8_B8_A8_UNorm_SRgb;
        case DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM:
            return PixelFormat::B8_G8_R8_A8_UNorm;
        case DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            return PixelFormat::B8_G8_R8_A8_UNorm_SRgb;
        case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_SNORM:
            return PixelFormat::R8_G8_B8_A8_SNorm;
        case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UINT:
            return PixelFormat::R8_G8_B8_A8_UInt;
        case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_SINT:
            return PixelFormat::R8_G8_B8_A8_SInt;

        case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_UNORM:
            return PixelFormat::R16_G16_B16_A16_UNorm;
        case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_SNORM:
            return PixelFormat::R16_G16_B16_A16_SNorm;
        case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_UINT:
            return PixelFormat::R16_G16_B16_A16_UInt;
        case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_SINT:
            return PixelFormat::R16_G16_B16_A16_SInt;
        case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT:
            return PixelFormat::R16_G16_B16_A16_Float;

        case DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_UINT:
            return PixelFormat::R32_G32_B32_A32_UInt;
        case DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_SINT:
            return PixelFormat::R32_G32_B32_A32_SInt;
        case DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT:
            return PixelFormat::R32_G32_B32_A32_Float;

        case DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT::DXGI_FORMAT_BC1_TYPELESS:
            return PixelFormat::BC1_Rgba_UNorm;
        case DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM:
            return PixelFormat::BC2_UNorm;
        case DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM_SRGB:
            return PixelFormat::BC2_UNorm_SRgb;
        case DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM:
            return PixelFormat::BC3_UNorm;
        case DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM_SRGB:
            return PixelFormat::BC3_UNorm_SRgb;
        case DXGI_FORMAT::DXGI_FORMAT_BC4_UNORM:
            return PixelFormat::BC4_UNorm;
        case DXGI_FORMAT::DXGI_FORMAT_BC4_SNORM:
            return PixelFormat::BC4_SNorm;
        case DXGI_FORMAT::DXGI_FORMAT_BC5_UNORM:
            return PixelFormat::BC5_UNorm;
        case DXGI_FORMAT::DXGI_FORMAT_BC5_SNORM:
            return PixelFormat::BC5_SNorm;
        case DXGI_FORMAT::DXGI_FORMAT_BC7_UNORM:
            return PixelFormat::BC7_UNorm;
        case DXGI_FORMAT::DXGI_FORMAT_BC7_UNORM_SRGB:
            return PixelFormat::BC7_UNorm_SRgb;


        case DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            return PixelFormat::D32_Float_S8_UInt;
        case DXGI_FORMAT::DXGI_FORMAT_D24_UNORM_S8_UINT:
            return PixelFormat::D24_UNorm_S8_UInt;

        case DXGI_FORMAT::DXGI_FORMAT_R10G10B10A2_UNORM:
            return PixelFormat::R10_G10_B10_A2_UNorm;
        case DXGI_FORMAT::DXGI_FORMAT_R10G10B10A2_UINT:
            return PixelFormat::R10_G10_B10_A2_UInt;
        case DXGI_FORMAT::DXGI_FORMAT_R11G11B10_FLOAT:
            return PixelFormat::R11_G11_B10_Float;

        default:
            //throw new alloyException($"Invalid {nameof(PixelFormat)}: {format}");
            return (PixelFormat)0;

        }
    }
}

