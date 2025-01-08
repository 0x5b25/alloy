#include "VkTypeCvt.hpp"


namespace alloy::vk {

    VkSamplerAddressMode VdToVkSamplerAddressMode(ISampler::Description::AddressMode mode)
    {
        using SamplerAddressMode = typename ISampler::Description::AddressMode;
        switch (mode)
        {
        case SamplerAddressMode::Wrap:
            return VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerAddressMode::Mirror:
            return VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case SamplerAddressMode::Clamp:
            return VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SamplerAddressMode::Border:
            return VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default:
            return VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
        }
    }

    void GetFilterParams(
        ISampler::Description::SamplerFilter filter,
        VkFilter& minFilter,
        VkFilter& magFilter,
        VkSamplerMipmapMode& mipmapMode)
    {
        using SamplerFilter = typename ISampler::Description::SamplerFilter;
        switch (filter)
        {
        case SamplerFilter::Anisotropic:
            minFilter = VkFilter::VK_FILTER_LINEAR;
            magFilter = VkFilter::VK_FILTER_LINEAR;
            mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        case SamplerFilter::MinPoint_MagPoint_MipPoint:
            minFilter = VkFilter::VK_FILTER_NEAREST;
            magFilter = VkFilter::VK_FILTER_NEAREST;
            mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case SamplerFilter::MinPoint_MagPoint_MipLinear:
            minFilter = VkFilter::VK_FILTER_NEAREST;
            magFilter = VkFilter::VK_FILTER_NEAREST;
            mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        case SamplerFilter::MinPoint_MagLinear_MipPoint:
            minFilter = VkFilter::VK_FILTER_NEAREST;
            magFilter = VkFilter::VK_FILTER_LINEAR;
            mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case SamplerFilter::MinPoint_MagLinear_MipLinear:
            minFilter = VkFilter::VK_FILTER_NEAREST;
            magFilter = VkFilter::VK_FILTER_LINEAR;
            mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        case SamplerFilter::MinLinear_MagPoint_MipPoint:
            minFilter = VkFilter::VK_FILTER_LINEAR;
            magFilter = VkFilter::VK_FILTER_NEAREST;
            mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case SamplerFilter::MinLinear_MagPoint_MipLinear:
            minFilter = VkFilter::VK_FILTER_LINEAR;
            magFilter = VkFilter::VK_FILTER_NEAREST;
            mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        case SamplerFilter::MinLinear_MagLinear_MipPoint:
            minFilter = VkFilter::VK_FILTER_LINEAR;
            magFilter = VkFilter::VK_FILTER_LINEAR;
            mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case SamplerFilter::MinLinear_MagLinear_MipLinear:
            minFilter = VkFilter::VK_FILTER_LINEAR;
            magFilter = VkFilter::VK_FILTER_LINEAR;
            mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        default:
            break;
        }
    }

     

    VkDescriptorType VdToVkDescriptorType(
        IBindableResource::ResourceKind kind, 
        IResourceLayout::Description::ElementDescription::Options options)
    {
        using ResourceKind = typename IBindableResource::ResourceKind;
        bool dynamicBinding = false;//(options.dynamicBinding) != 0;
        bool writable = (options.writable) != 0;
        switch (kind)
        {
        case ResourceKind::UniformBuffer:
            return dynamicBinding 
                ? VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC 
                : VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case ResourceKind::StorageBuffer:
            return dynamicBinding 
                ? VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
                : VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case ResourceKind::Texture:
            return writable
                ? VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                : VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case ResourceKind::Sampler:
            return VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER;
        default:
            return VkDescriptorType::VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    }

     VkSampleCountFlagBits VdToVkSampleCount(SampleCount sampleCount)
    {
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

    VkStencilOp VdToVkStencilOp(DepthStencilStateDescription::StencilBehavior::Operation op)
    {
        using StencilOperation = typename DepthStencilStateDescription::StencilBehavior::Operation;
        switch (op)
        {
        case StencilOperation::Keep:
            return VkStencilOp::VK_STENCIL_OP_KEEP;
        case StencilOperation::Zero:
            return VkStencilOp::VK_STENCIL_OP_ZERO;
        case StencilOperation::Replace:
            return VkStencilOp::VK_STENCIL_OP_REPLACE;
        case StencilOperation::IncrementAndClamp:
            return VkStencilOp::VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case StencilOperation::DecrementAndClamp:
            return VkStencilOp::VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case StencilOperation::Invert:
            return VkStencilOp::VK_STENCIL_OP_INVERT;
        case StencilOperation::IncrementAndWrap:
            return VkStencilOp::VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case StencilOperation::DecrementAndWrap:
            return VkStencilOp::VK_STENCIL_OP_DECREMENT_AND_WRAP;
        default:
            return VkStencilOp::VK_STENCIL_OP_MAX_ENUM;
        }
    }

    VkPolygonMode VdToVkPolygonMode(RasterizerStateDescription::PolygonFillMode fillMode)
    {
        using PolygonFillMode = typename RasterizerStateDescription::PolygonFillMode;
        switch (fillMode)
        {
        case PolygonFillMode::Solid:
            return VkPolygonMode::VK_POLYGON_MODE_FILL;
        case PolygonFillMode::Wireframe:
            return VkPolygonMode::VK_POLYGON_MODE_LINE;
        default:
            return VkPolygonMode::VK_POLYGON_MODE_MAX_ENUM;
        }
    }

     VkCullModeFlags VdToVkCullMode(RasterizerStateDescription::FaceCullMode cullMode)
    {
        switch (cullMode)
        {
        case RasterizerStateDescription::FaceCullMode::Back:
            return VkCullModeFlagBits::VK_CULL_MODE_BACK_BIT;
        case RasterizerStateDescription::FaceCullMode::Front:
            return VkCullModeFlagBits::VK_CULL_MODE_FRONT_BIT;
        case RasterizerStateDescription::FaceCullMode::None:
            return VkCullModeFlagBits::VK_CULL_MODE_NONE;
        default:
            return VkCullModeFlagBits::VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
        }
    }

     VkBlendOp VdToVkBlendOp(BlendStateDescription::BlendFunction func)
    {
        switch (func)
        {
        case BlendStateDescription::BlendFunction::Add:
            return VkBlendOp::VK_BLEND_OP_ADD;
        case BlendStateDescription::BlendFunction::Subtract:
            return VkBlendOp::VK_BLEND_OP_SUBTRACT;
        case BlendStateDescription::BlendFunction::ReverseSubtract:
            return VkBlendOp::VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendStateDescription::BlendFunction::Minimum:
            return VkBlendOp::VK_BLEND_OP_MIN;
        case BlendStateDescription::BlendFunction::Maximum:
            return VkBlendOp::VK_BLEND_OP_MAX;
        default:
            return VkBlendOp::VK_BLEND_OP_MAX_ENUM;
        }
    }

    VkColorComponentFlags VdToVkColorWriteMask(
        const BlendStateDescription::ColorWriteMask& mask
    ) {
        VkColorComponentFlags flags = 0;

        if (mask.r) flags |= VkColorComponentFlagBits::VK_COLOR_COMPONENT_R_BIT;
        if (mask.g) flags |= VkColorComponentFlagBits::VK_COLOR_COMPONENT_G_BIT;
        if (mask.b) flags |= VkColorComponentFlagBits::VK_COLOR_COMPONENT_B_BIT;
        if (mask.a) flags |= VkColorComponentFlagBits::VK_COLOR_COMPONENT_A_BIT;

        return flags;
    }

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

    

     VkFormat VdToVkShaderDataType(ShaderDataType format)
    {
        switch (format)
        {
        case ShaderDataType::Float1:
            return VkFormat::VK_FORMAT_R32_SFLOAT;
        case ShaderDataType::Float2:
            return VkFormat::VK_FORMAT_R32G32_SFLOAT;
        case ShaderDataType::Float3:
            return VkFormat::VK_FORMAT_R32G32B32_SFLOAT;
        case ShaderDataType::Float4:
            return VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT;
        case ShaderDataType::Byte2_Norm:
            return VkFormat::VK_FORMAT_R8G8_UNORM;
        case ShaderDataType::Byte2:
            return VkFormat::VK_FORMAT_R8G8_UINT;
        case ShaderDataType::Byte4_Norm:
            return VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
        case ShaderDataType::Byte4:
            return VkFormat::VK_FORMAT_R8G8B8A8_UINT;
        case ShaderDataType::SByte2_Norm:
            return VkFormat::VK_FORMAT_R8G8_SNORM;
        case ShaderDataType::SByte2:
            return VkFormat::VK_FORMAT_R8G8_SINT;
        case ShaderDataType::SByte4_Norm:
            return VkFormat::VK_FORMAT_R8G8B8A8_SNORM;
        case ShaderDataType::SByte4:
            return VkFormat::VK_FORMAT_R8G8B8A8_SINT;
        case ShaderDataType::UShort2_Norm:
            return VkFormat::VK_FORMAT_R16G16_UNORM;
        case ShaderDataType::UShort2:
            return VkFormat::VK_FORMAT_R16G16_UINT;
        case ShaderDataType::UShort4_Norm:
            return VkFormat::VK_FORMAT_R16G16B16A16_UNORM;
        case ShaderDataType::UShort4:
            return VkFormat::VK_FORMAT_R16G16B16A16_UINT;
        case ShaderDataType::Short2_Norm:
            return VkFormat::VK_FORMAT_R16G16_SNORM;
        case ShaderDataType::Short2:
            return VkFormat::VK_FORMAT_R16G16_SINT;
        case ShaderDataType::Short4_Norm:
            return VkFormat::VK_FORMAT_R16G16B16A16_SNORM;
        case ShaderDataType::Short4:
            return VkFormat::VK_FORMAT_R16G16B16A16_SINT;
        case ShaderDataType::UInt1:
            return VkFormat::VK_FORMAT_R32_UINT;
        case ShaderDataType::UInt2:
            return VkFormat::VK_FORMAT_R32G32_UINT;
        case ShaderDataType::UInt3:
            return VkFormat::VK_FORMAT_R32G32B32_UINT;
        case ShaderDataType::UInt4:
            return VkFormat::VK_FORMAT_R32G32B32A32_UINT;
        case ShaderDataType::Int1:
            return VkFormat::VK_FORMAT_R32_SINT;
        case ShaderDataType::Int2:
            return VkFormat::VK_FORMAT_R32G32_SINT;
        case ShaderDataType::Int3:
            return VkFormat::VK_FORMAT_R32G32B32_SINT;
        case ShaderDataType::Int4:
            return VkFormat::VK_FORMAT_R32G32B32A32_SINT;
        case ShaderDataType::Half1:
            return VkFormat::VK_FORMAT_R16_SFLOAT;
        case ShaderDataType::Half2:
            return VkFormat::VK_FORMAT_R16G16_SFLOAT;
        case ShaderDataType::Half4:
            return VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT;
        default:
            return VkFormat::VK_FORMAT_MAX_ENUM;
        }
    }

    VkShaderStageFlags VdToVkShaderStages(IShader::Stages stages){
        VkShaderStageFlags flag = 0;
        if (stages[IShader::Stage::Vertex])                 flag |= VK_SHADER_STAGE_VERTEX_BIT;
        if (stages[IShader::Stage::Geometry])               flag |= VK_SHADER_STAGE_GEOMETRY_BIT;
        if (stages[IShader::Stage::TessellationControl])    flag |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        if (stages[IShader::Stage::TessellationEvaluation]) flag |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        if (stages[IShader::Stage::Fragment])               flag |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (stages[IShader::Stage::Compute])                flag |= VK_SHADER_STAGE_COMPUTE_BIT;
        return flag;

    }

    VkShaderStageFlagBits VdToVkShaderStageSingle(IShader::Stage stage) {
        switch(stage) {
        case IShader::Stage::Vertex:                 return VK_SHADER_STAGE_VERTEX_BIT;
        case IShader::Stage::Geometry:               return VK_SHADER_STAGE_GEOMETRY_BIT;
        case IShader::Stage::TessellationControl:    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case IShader::Stage::TessellationEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case IShader::Stage::Fragment:               return VK_SHADER_STAGE_FRAGMENT_BIT;
        case IShader::Stage::Compute:                return VK_SHADER_STAGE_COMPUTE_BIT;
        default: return VkShaderStageFlagBits::VK_SHADER_STAGE_ALL;
        }
    }

     
    VkBorderColor VdToVkSamplerBorderColor(ISampler::Description::BorderColor borderColor)
    {
        using SamplerBorderColor = typename ISampler::Description::BorderColor;
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


    VkIndexType VdToVkIndexFormat(IndexFormat format)
    {
        switch (format)
        {
        case IndexFormat::UInt16:
            return VkIndexType::VK_INDEX_TYPE_UINT16;
        case IndexFormat::UInt32:
            return VkIndexType::VK_INDEX_TYPE_UINT32;
        default:
            return VkIndexType::VK_INDEX_TYPE_MAX_ENUM;
        }
    }

    VkBlendFactor VdToVkBlendFactor(alloy::BlendStateDescription::BlendFactor factor)
    {
        using BlendFactor = typename alloy::BlendStateDescription::BlendFactor;
        switch (factor)
        {
        case BlendFactor::Zero:
            return VkBlendFactor::VK_BLEND_FACTOR_ZERO;
        case BlendFactor::One:
            return VkBlendFactor::VK_BLEND_FACTOR_ONE;
        case BlendFactor::SourceAlpha:
            return VkBlendFactor::VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::InverseSourceAlpha:
            return VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DestinationAlpha:
            return VkBlendFactor::VK_BLEND_FACTOR_DST_ALPHA;
        case BlendFactor::InverseDestinationAlpha:
            return VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case BlendFactor::SourceColor:
            return VkBlendFactor::VK_BLEND_FACTOR_SRC_COLOR;
        case BlendFactor::InverseSourceColor:
            return VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DestinationColor:
            return VkBlendFactor::VK_BLEND_FACTOR_DST_COLOR;
        case BlendFactor::InverseDestinationColor:
            return VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case BlendFactor::BlendFactor:
            return VkBlendFactor::VK_BLEND_FACTOR_CONSTANT_COLOR;
        case BlendFactor::InverseBlendFactor:
            return VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        default:
            return VkBlendFactor::VK_BLEND_FACTOR_MAX_ENUM;
        }
    }


    VkCompareOp VdToVkCompareOp(ComparisonKind comparisonKind)
    {
        switch (comparisonKind)
        {
        case ComparisonKind::Never:
            return VkCompareOp::VK_COMPARE_OP_NEVER;
        case ComparisonKind::Less:
            return VkCompareOp::VK_COMPARE_OP_LESS;
        case ComparisonKind::Equal:
            return VkCompareOp::VK_COMPARE_OP_EQUAL;
        case ComparisonKind::LessEqual:
            return VkCompareOp::VK_COMPARE_OP_LESS_OR_EQUAL;
        case ComparisonKind::Greater:
            return VkCompareOp::VK_COMPARE_OP_GREATER;
        case ComparisonKind::NotEqual:
            return VkCompareOp::VK_COMPARE_OP_NOT_EQUAL;
        case ComparisonKind::GreaterEqual:
            return VkCompareOp::VK_COMPARE_OP_GREATER_OR_EQUAL;
        case ComparisonKind::Always:
            return VkCompareOp::VK_COMPARE_OP_ALWAYS;
        default:
            return VkCompareOp::VK_COMPARE_OP_MAX_ENUM;
        }
    }

    VkFormat VdToVkPixelFormat(const PixelFormat& format, bool toDepthFormat)
    {
        switch (format)
        {
        case PixelFormat::R8_UNorm:
            return VkFormat::VK_FORMAT_R8_UNORM;
        case PixelFormat::R8_SNorm:
            return VkFormat::VK_FORMAT_R8_SNORM;
        case PixelFormat::R8_UInt:
            return VkFormat::VK_FORMAT_R8_UINT;
        case PixelFormat::R8_SInt:
            return VkFormat::VK_FORMAT_R8_SINT;

        case PixelFormat::R16_UNorm:
            return toDepthFormat
                ? VkFormat::VK_FORMAT_D16_UNORM
                : VkFormat::VK_FORMAT_R16_UNORM;
        case PixelFormat::R16_SNorm:
            return VkFormat::VK_FORMAT_R16_SNORM;
        case PixelFormat::R16_UInt:
            return VkFormat::VK_FORMAT_R16_UINT;
        case PixelFormat::R16_SInt:
            return VkFormat::VK_FORMAT_R16_SINT;
        case PixelFormat::R16_Float:
            return VkFormat::VK_FORMAT_R16_SFLOAT;

        case PixelFormat::R32_UInt:
            return VkFormat::VK_FORMAT_R32_UINT;
        case PixelFormat::R32_SInt:
            return VkFormat::VK_FORMAT_R32_SINT;
        case PixelFormat::R32_Float:
            return toDepthFormat
                ? VkFormat::VK_FORMAT_D32_SFLOAT
                : VkFormat::VK_FORMAT_R32_SFLOAT;

        case PixelFormat::R8_G8_UNorm:
            return VkFormat::VK_FORMAT_R8G8_UNORM;
        case PixelFormat::R8_G8_SNorm:
            return VkFormat::VK_FORMAT_R8G8_SNORM;
        case PixelFormat::R8_G8_UInt:
            return VkFormat::VK_FORMAT_R8G8_UINT;
        case PixelFormat::R8_G8_SInt:
            return VkFormat::VK_FORMAT_R8G8_SINT;

        case PixelFormat::R16_G16_UNorm:
            return VkFormat::VK_FORMAT_R16G16_UNORM;
        case PixelFormat::R16_G16_SNorm:
            return VkFormat::VK_FORMAT_R16G16_SNORM;
        case PixelFormat::R16_G16_UInt:
            return VkFormat::VK_FORMAT_R16G16_UINT;
        case PixelFormat::R16_G16_SInt:
            return VkFormat::VK_FORMAT_R16G16_SINT;
        case PixelFormat::R16_G16_Float:
            //return VkFormat.R16g16b16a16Sfloat; ??
            return VkFormat::VK_FORMAT_R16G16_SFLOAT;

        case PixelFormat::R32_G32_UInt:
            return VkFormat::VK_FORMAT_R32G32_UINT;
        case PixelFormat::R32_G32_SInt:
            return VkFormat::VK_FORMAT_R32G32_SINT;
        case PixelFormat::R32_G32_Float:
            return VkFormat::VK_FORMAT_R32G32_SFLOAT;

        case PixelFormat::R8_G8_B8_A8_UNorm:
            return VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
        case PixelFormat::R8_G8_B8_A8_UNorm_SRgb:
            return VkFormat::VK_FORMAT_R8G8B8A8_SRGB;
        case PixelFormat::B8_G8_R8_A8_UNorm:
            return VkFormat::VK_FORMAT_B8G8R8A8_UNORM;
        case PixelFormat::B8_G8_R8_A8_UNorm_SRgb:
            return VkFormat::VK_FORMAT_B8G8R8A8_SRGB;
        case PixelFormat::R8_G8_B8_A8_SNorm:
            return VkFormat::VK_FORMAT_R8G8B8A8_SNORM;
        case PixelFormat::R8_G8_B8_A8_UInt:
            return VkFormat::VK_FORMAT_R8G8B8A8_UINT;
        case PixelFormat::R8_G8_B8_A8_SInt:
            return VkFormat::VK_FORMAT_R8G8B8A8_SINT;

        case PixelFormat::R16_G16_B16_A16_UNorm:
            return VkFormat::VK_FORMAT_R16G16B16A16_UNORM;
        case PixelFormat::R16_G16_B16_A16_SNorm:
            return VkFormat::VK_FORMAT_R16G16B16A16_SNORM;
        case PixelFormat::R16_G16_B16_A16_UInt:
            return VkFormat::VK_FORMAT_R16G16B16A16_UINT;
        case PixelFormat::R16_G16_B16_A16_SInt:
            return VkFormat::VK_FORMAT_R16G16B16A16_SINT;
        case PixelFormat::R16_G16_B16_A16_Float:
            return VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT;

        case PixelFormat::R32_G32_B32_A32_UInt:
            return VkFormat::VK_FORMAT_R32G32B32A32_UINT;
        case PixelFormat::R32_G32_B32_A32_SInt:
            return VkFormat::VK_FORMAT_R32G32B32A32_SINT;
        case PixelFormat::R32_G32_B32_A32_Float:
            return VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT;

        case PixelFormat::BC1_Rgb_UNorm:
            return VkFormat::VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case PixelFormat::BC1_Rgb_UNorm_SRgb:
            return VkFormat::VK_FORMAT_BC1_RGB_SRGB_BLOCK;
        case PixelFormat::BC1_Rgba_UNorm:
            return VkFormat::VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case PixelFormat::BC1_Rgba_UNorm_SRgb:
            return VkFormat::VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case PixelFormat::BC2_UNorm:
            return VkFormat::VK_FORMAT_BC2_UNORM_BLOCK;
        case PixelFormat::BC2_UNorm_SRgb:
            return VkFormat::VK_FORMAT_BC2_SRGB_BLOCK;
        case PixelFormat::BC3_UNorm:
            return VkFormat::VK_FORMAT_BC3_UNORM_BLOCK;
        case PixelFormat::BC3_UNorm_SRgb:
            return VkFormat::VK_FORMAT_BC3_SRGB_BLOCK;
        case PixelFormat::BC4_UNorm:
            return VkFormat::VK_FORMAT_BC4_UNORM_BLOCK;
        case PixelFormat::BC4_SNorm:
            return VkFormat::VK_FORMAT_BC4_SNORM_BLOCK;
        case PixelFormat::BC5_UNorm:
            return VkFormat::VK_FORMAT_BC5_UNORM_BLOCK;
        case PixelFormat::BC5_SNorm:
            return VkFormat::VK_FORMAT_BC5_SNORM_BLOCK;
        case PixelFormat::BC7_UNorm:
            return VkFormat::VK_FORMAT_BC7_UNORM_BLOCK;
        case PixelFormat::BC7_UNorm_SRgb:
            return VkFormat::VK_FORMAT_BC7_SRGB_BLOCK;

        case PixelFormat::ETC2_R8_G8_B8_UNorm:
            return VkFormat::VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
        case PixelFormat::ETC2_R8_G8_B8_A1_UNorm:
            return VkFormat::VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
        case PixelFormat::ETC2_R8_G8_B8_A8_UNorm:
            return VkFormat::VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;

        case PixelFormat::D32_Float_S8_UInt:
            return VkFormat::VK_FORMAT_D32_SFLOAT_S8_UINT;
        case PixelFormat::D24_UNorm_S8_UInt:
            return VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;

        case PixelFormat::R10_G10_B10_A2_UNorm:
            return VkFormat::VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case PixelFormat::R10_G10_B10_A2_UInt:
            return VkFormat::VK_FORMAT_A2B10G10R10_UINT_PACK32;
        case PixelFormat::R11_G11_B10_Float:
            return VkFormat::VK_FORMAT_B10G11R11_UFLOAT_PACK32;

        default:
            //throw new alloyException($"Invalid {nameof(PixelFormat)}: {format}");
            return VkFormat::VK_FORMAT_UNDEFINED;
        }
    }

    PixelFormat VkToVdPixelFormat(VkFormat vkFormat)
    {
        switch (vkFormat) {
        case VkFormat::VK_FORMAT_R8_UNORM:
            return PixelFormat::R8_UNorm;
        case VkFormat::VK_FORMAT_R8_SNORM:
            return PixelFormat::R8_SNorm;
        case VkFormat::VK_FORMAT_R8_UINT:
            return PixelFormat::R8_UInt;
        case VkFormat::VK_FORMAT_R8_SINT:
            return PixelFormat::R8_SInt;

        case VkFormat::VK_FORMAT_D16_UNORM:
        case VkFormat::VK_FORMAT_R16_UNORM:
            return PixelFormat::R16_UNorm;
        case VkFormat::VK_FORMAT_R16_SNORM:
            return PixelFormat::R16_SNorm;
        case VkFormat::VK_FORMAT_R16_UINT:
            return PixelFormat::R16_UInt;
        case VkFormat::VK_FORMAT_R16_SINT:
            return PixelFormat::R16_SInt;
        case VkFormat::VK_FORMAT_R16_SFLOAT:
            return PixelFormat::R16_Float;

        case VkFormat::VK_FORMAT_R32_UINT:
            return PixelFormat::R32_UInt;
        case VkFormat::VK_FORMAT_R32_SINT:
            return PixelFormat::R32_SInt;
        case VkFormat::VK_FORMAT_D32_SFLOAT:
        case VkFormat::VK_FORMAT_R32_SFLOAT:
            return PixelFormat::R32_Float;

        case VkFormat::VK_FORMAT_R8G8_UNORM:
            return PixelFormat::R8_G8_UNorm;
        case VkFormat::VK_FORMAT_R8G8_SNORM:
            return PixelFormat::R8_G8_SNorm;
        case VkFormat::VK_FORMAT_R8G8_UINT:
            return PixelFormat::R8_G8_UInt;
        case VkFormat::VK_FORMAT_R8G8_SINT:
            return PixelFormat::R8_G8_SInt;

        case VkFormat::VK_FORMAT_R16G16_UNORM:
            return PixelFormat::R16_G16_UNorm;
        case VkFormat::VK_FORMAT_R16G16_SNORM:
            return PixelFormat::R16_G16_SNorm;
        case VkFormat::VK_FORMAT_R16G16_UINT:
            return PixelFormat::R16_G16_UInt;
        case VkFormat::VK_FORMAT_R16G16_SINT:
            return PixelFormat::R16_G16_SInt;
        case VkFormat::VK_FORMAT_R16G16_SFLOAT:
            //return VkFormat.R16g16b16a16Sfloat; ??
            return PixelFormat::R16_G16_Float;

        case VkFormat::VK_FORMAT_R32G32_UINT:
            return PixelFormat::R32_G32_UInt;
        case VkFormat::VK_FORMAT_R32G32_SINT:
            return PixelFormat::R32_G32_SInt;
        case VkFormat::VK_FORMAT_R32G32_SFLOAT:
            return PixelFormat::R32_G32_Float;

        case VkFormat::VK_FORMAT_R8G8B8A8_UNORM:
            return PixelFormat::R8_G8_B8_A8_UNorm;
        case VkFormat::VK_FORMAT_R8G8B8A8_SRGB:
            return PixelFormat::R8_G8_B8_A8_UNorm_SRgb;
        case VkFormat::VK_FORMAT_B8G8R8A8_UNORM:
            return PixelFormat::B8_G8_R8_A8_UNorm;
        case VkFormat::VK_FORMAT_B8G8R8A8_SRGB:
            return PixelFormat::B8_G8_R8_A8_UNorm_SRgb;
        case VkFormat::VK_FORMAT_R8G8B8A8_SNORM:
            return PixelFormat::R8_G8_B8_A8_SNorm;
        case VkFormat::VK_FORMAT_R8G8B8A8_UINT:
            return PixelFormat::R8_G8_B8_A8_UInt;
        case VkFormat::VK_FORMAT_R8G8B8A8_SINT:
            return PixelFormat::R8_G8_B8_A8_SInt;

        case VkFormat::VK_FORMAT_R16G16B16A16_UNORM:
            return PixelFormat::R16_G16_B16_A16_UNorm;
        case VkFormat::VK_FORMAT_R16G16B16A16_SNORM:
            return PixelFormat::R16_G16_B16_A16_SNorm;
        case VkFormat::VK_FORMAT_R16G16B16A16_UINT:
            return PixelFormat::R16_G16_B16_A16_UInt;
        case VkFormat::VK_FORMAT_R16G16B16A16_SINT:
            return PixelFormat::R16_G16_B16_A16_SInt;
        case VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT:
            return PixelFormat::R16_G16_B16_A16_Float;

        case VkFormat::VK_FORMAT_R32G32B32A32_UINT:
            return PixelFormat::R32_G32_B32_A32_UInt;
        case VkFormat::VK_FORMAT_R32G32B32A32_SINT:
            return PixelFormat::R32_G32_B32_A32_SInt;
        case VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT:
            return PixelFormat::R32_G32_B32_A32_Float;

        case VkFormat::VK_FORMAT_BC1_RGB_UNORM_BLOCK:
            return PixelFormat::BC1_Rgb_UNorm;
        case VkFormat::VK_FORMAT_BC1_RGB_SRGB_BLOCK:
            return PixelFormat::BC1_Rgb_UNorm_SRgb;
        case VkFormat::VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            return PixelFormat::BC1_Rgba_UNorm;
        case VkFormat::VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
            return PixelFormat::BC1_Rgba_UNorm_SRgb;
        case VkFormat::VK_FORMAT_BC2_UNORM_BLOCK:
            return PixelFormat::BC2_UNorm;
        case VkFormat::VK_FORMAT_BC2_SRGB_BLOCK:
            return PixelFormat::BC2_UNorm_SRgb;
        case VkFormat::VK_FORMAT_BC3_UNORM_BLOCK:
            return PixelFormat::BC3_UNorm;
        case VkFormat::VK_FORMAT_BC3_SRGB_BLOCK:
            return PixelFormat::BC3_UNorm_SRgb;
        case VkFormat::VK_FORMAT_BC4_UNORM_BLOCK:
            return PixelFormat::BC4_UNorm;
        case VkFormat::VK_FORMAT_BC4_SNORM_BLOCK:
            return PixelFormat::BC4_SNorm;
        case VkFormat::VK_FORMAT_BC5_UNORM_BLOCK:
            return PixelFormat::BC5_UNorm;
        case VkFormat::VK_FORMAT_BC5_SNORM_BLOCK:
            return PixelFormat::BC5_SNorm;
        case VkFormat::VK_FORMAT_BC7_UNORM_BLOCK:
            return PixelFormat::BC7_UNorm;
        case VkFormat::VK_FORMAT_BC7_SRGB_BLOCK:
            return PixelFormat::BC7_UNorm_SRgb;

        case VkFormat::VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
            return PixelFormat::ETC2_R8_G8_B8_UNorm;
        case VkFormat::VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
            return PixelFormat::ETC2_R8_G8_B8_A1_UNorm;
        case VkFormat::VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
            return PixelFormat::ETC2_R8_G8_B8_A8_UNorm;

        case VkFormat::VK_FORMAT_D32_SFLOAT_S8_UINT:
            return PixelFormat::D32_Float_S8_UInt;
        case VkFormat::VK_FORMAT_D24_UNORM_S8_UINT:
            return PixelFormat::D24_UNorm_S8_UInt;

        case VkFormat::VK_FORMAT_A2B10G10R10_UNORM_PACK32:
            return PixelFormat::R10_G10_B10_A2_UNorm;
        case VkFormat::VK_FORMAT_A2B10G10R10_UINT_PACK32:
            return PixelFormat::R10_G10_B10_A2_UInt;
        case VkFormat::VK_FORMAT_B10G11R11_UFLOAT_PACK32:
            return PixelFormat::R11_G11_B10_Float;

        default:
            //throw new alloyException($"Invalid {nameof(PixelFormat)}: {format}");
            return (PixelFormat)0;

        }
    }
}
