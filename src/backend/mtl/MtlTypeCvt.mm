//#include "MtlTypeCvt.h"
#include "MtlTypeCvt.h"

#include <cassert>

#include <Metal/Metal.h>

namespace alloy::mtl {
    MTLPixelFormat AlToMtlPixelFormat(::alloy::PixelFormat format)
    //MTLPixelFormat VdToVkPixelFormat(const PixelFormat& format, bool toDepthFormat)
    {
        using PixelFormat = ::alloy::PixelFormat;
        switch (format)
        {
        case PixelFormat::R8_UNorm:
            return MTLPixelFormatR8Unorm;
        case PixelFormat::R8_SNorm: return MTLPixelFormatR8Snorm;
        case PixelFormat::R8_UInt: return MTLPixelFormatR8Uint;
        case PixelFormat::R8_SInt: return MTLPixelFormatR8Sint;

        case PixelFormat::R16_UNorm: return MTLPixelFormatR16Unorm;
        case PixelFormat::R16_SNorm: return MTLPixelFormatR16Snorm;
        case PixelFormat::R16_UInt: return MTLPixelFormatR16Uint;
        case PixelFormat::R16_SInt: return MTLPixelFormatR16Sint;
        case PixelFormat::R16_Float: return MTLPixelFormatR16Float;

        case PixelFormat::R32_UInt: return MTLPixelFormatR32Uint;
        case PixelFormat::R32_SInt: return MTLPixelFormatR32Sint;
        case PixelFormat::R32_Float: return MTLPixelFormatR32Float;

        case PixelFormat::R8_G8_UNorm: return MTLPixelFormatRG8Unorm;
        case PixelFormat::R8_G8_SNorm: return MTLPixelFormatRG8Snorm;
        case PixelFormat::R8_G8_UInt: return MTLPixelFormatRG8Uint;
        case PixelFormat::R8_G8_SInt: return MTLPixelFormatRG8Sint;

        case PixelFormat::R16_G16_UNorm: return MTLPixelFormatRG16Unorm;
        case PixelFormat::R16_G16_SNorm: return MTLPixelFormatRG16Snorm;
        case PixelFormat::R16_G16_UInt: return MTLPixelFormatRG16Uint;
        case PixelFormat::R16_G16_SInt: return MTLPixelFormatRG16Sint;
        case PixelFormat::R16_G16_Float: return MTLPixelFormatRG16Float;

        case PixelFormat::R32_G32_UInt: return MTLPixelFormatRG32Uint;
        case PixelFormat::R32_G32_SInt: return MTLPixelFormatRG32Sint;
        case PixelFormat::R32_G32_Float:return MTLPixelFormatRG32Float;

        case PixelFormat::R8_G8_B8_A8_UNorm: return MTLPixelFormatRGBA8Unorm;
        case PixelFormat::R8_G8_B8_A8_UNorm_SRgb: return MTLPixelFormatRGBA8Unorm_sRGB;
        case PixelFormat::B8_G8_R8_A8_UNorm: return MTLPixelFormatBGRA8Unorm;
        case PixelFormat::B8_G8_R8_A8_UNorm_SRgb: return MTLPixelFormatBGRA8Unorm_sRGB;
        case PixelFormat::R8_G8_B8_A8_SNorm: return MTLPixelFormatRGBA8Snorm;
        case PixelFormat::R8_G8_B8_A8_UInt: return MTLPixelFormatRGBA8Uint;
        case PixelFormat::R8_G8_B8_A8_SInt: return MTLPixelFormatRGBA8Sint;

        case PixelFormat::R16_G16_B16_A16_UNorm: return MTLPixelFormatRGBA16Unorm;
        case PixelFormat::R16_G16_B16_A16_SNorm: return MTLPixelFormatRGBA16Snorm;
        case PixelFormat::R16_G16_B16_A16_UInt: return MTLPixelFormatRGBA16Uint;
        case PixelFormat::R16_G16_B16_A16_SInt: return MTLPixelFormatRGBA16Sint;
        case PixelFormat::R16_G16_B16_A16_Float: return MTLPixelFormatRGBA16Float;

        case PixelFormat::R32_G32_B32_A32_UInt: return MTLPixelFormatRGBA32Uint;
        case PixelFormat::R32_G32_B32_A32_SInt: return MTLPixelFormatRGBA32Sint;
        case PixelFormat::R32_G32_B32_A32_Float: return MTLPixelFormatRGBA32Float;

        //case PixelFormat::BC1_Rgb_UNorm: return MTLPixelFormatBC1_RGBA;
        //case PixelFormat::BC1_Rgb_UNorm_SRgb: return MTLPixelFormatBC1_RGBA_sRGB;
        case PixelFormat::BC1_Rgba_UNorm: return MTLPixelFormatBC1_RGBA;
        case PixelFormat::BC1_Rgba_UNorm_SRgb: return MTLPixelFormatBC1_RGBA_sRGB;
        case PixelFormat::BC2_UNorm: return MTLPixelFormatBC2_RGBA;
        case PixelFormat::BC2_UNorm_SRgb:return MTLPixelFormatBC2_RGBA_sRGB;
        case PixelFormat::BC3_UNorm: return MTLPixelFormatBC3_RGBA;
        case PixelFormat::BC3_UNorm_SRgb:return MTLPixelFormatBC3_RGBA_sRGB;
        case PixelFormat::BC4_UNorm: return MTLPixelFormatBC4_RUnorm;
        case PixelFormat::BC4_SNorm: return MTLPixelFormatBC4_RSnorm;
        case PixelFormat::BC5_UNorm: return MTLPixelFormatBC5_RGUnorm;
        case PixelFormat::BC5_SNorm: return MTLPixelFormatBC5_RGSnorm;
        case PixelFormat::BC7_UNorm: return MTLPixelFormatBC7_RGBAUnorm;
            //return VkFormat::VK_FORMAT_BC7_UNORM_BLOCK;
        case PixelFormat::BC7_UNorm_SRgb: return MTLPixelFormatBC7_RGBAUnorm_sRGB;
            //return VkFormat::VK_FORMAT_BC7_SRGB_BLOCK;

        case PixelFormat::ETC2_R8_G8_B8_UNorm: return MTLPixelFormatETC2_RGB8;
        case PixelFormat::ETC2_R8_G8_B8_A1_UNorm: return MTLPixelFormatETC2_RGB8A1;

        //Still have doubt for this one
        case PixelFormat::ETC2_R8_G8_B8_A8_UNorm: return MTLPixelFormatEAC_RGBA8;

        case PixelFormat::D32_Float_S8_UInt: return MTLPixelFormatDepth24Unorm_Stencil8;
            //return VkFormat::VK_FORMAT_D32_SFLOAT_S8_UINT;
        case PixelFormat::D24_UNorm_S8_UInt: return MTLPixelFormatDepth32Float_Stencil8;
            //return VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;

        case PixelFormat::R10_G10_B10_A2_UNorm: return MTLPixelFormatRGB10A2Unorm;
        case PixelFormat::R10_G10_B10_A2_UInt: return MTLPixelFormatRGB10A2Uint;
        case PixelFormat::R11_G11_B10_Float: return MTLPixelFormatRG11B10Float;

        default:
            //throw new VeldridException($"Invalid {nameof(PixelFormat)}: {format}");
            assert(false);
            return MTLPixelFormatInvalid;
        }
    }


    ::alloy::PixelFormat MtlToAlPixelFormat(MTLPixelFormat format) {

        using PixelFormat = ::alloy::PixelFormat;
        switch (format)
        {
        case MTLPixelFormatR8Unorm:
            return PixelFormat::R8_UNorm;
        case MTLPixelFormatR8Snorm: return PixelFormat::R8_SNorm;
        case MTLPixelFormatR8Uint:  return PixelFormat::R8_UInt;
        case MTLPixelFormatR8Sint:  return PixelFormat::R8_SInt;

        case MTLPixelFormatR16Unorm: return PixelFormat::R16_UNorm;
        case MTLPixelFormatR16Snorm: return PixelFormat::R16_SNorm;
        case MTLPixelFormatR16Uint:  return PixelFormat::R16_UInt;
        case MTLPixelFormatR16Sint:  return PixelFormat::R16_SInt;
        case MTLPixelFormatR16Float: return PixelFormat::R16_Float;

        case MTLPixelFormatR32Uint: return PixelFormat::R32_UInt;
        case MTLPixelFormatR32Sint: return PixelFormat::R32_SInt;
        case MTLPixelFormatR32Float: return PixelFormat::R32_Float;

        case MTLPixelFormatRG8Unorm: return PixelFormat::R8_G8_UNorm;
        case MTLPixelFormatRG8Snorm: return PixelFormat::R8_G8_SNorm;
        case MTLPixelFormatRG8Uint: return PixelFormat::R8_G8_UInt;
        case MTLPixelFormatRG8Sint: return PixelFormat::R8_G8_SInt;

        case MTLPixelFormatRG16Unorm: return PixelFormat::R16_G16_UNorm;
        case MTLPixelFormatRG16Snorm: return PixelFormat::R16_G16_SNorm;
        case MTLPixelFormatRG16Uint: return PixelFormat::R16_G16_UInt;
        case MTLPixelFormatRG16Sint: return PixelFormat::R16_G16_SInt;
        case MTLPixelFormatRG16Float: return PixelFormat::R16_G16_Float;

        case MTLPixelFormatRG32Uint: return PixelFormat::R32_G32_UInt;
        case MTLPixelFormatRG32Sint: return PixelFormat::R32_G32_SInt;
        case MTLPixelFormatRG32Float: return PixelFormat::R32_G32_Float;

        case MTLPixelFormatRGBA8Unorm: return PixelFormat::R8_G8_B8_A8_UNorm;
        case MTLPixelFormatRGBA8Unorm_sRGB: return PixelFormat::R8_G8_B8_A8_UNorm_SRgb;
        case MTLPixelFormatBGRA8Unorm: return PixelFormat::B8_G8_R8_A8_UNorm;
        case MTLPixelFormatBGRA8Unorm_sRGB: return PixelFormat::B8_G8_R8_A8_UNorm_SRgb;
        case MTLPixelFormatRGBA8Snorm: return PixelFormat::R8_G8_B8_A8_SNorm;
        case MTLPixelFormatRGBA8Uint: return PixelFormat::R8_G8_B8_A8_UInt;
        case MTLPixelFormatRGBA8Sint: return PixelFormat::R8_G8_B8_A8_SInt;

        case MTLPixelFormatRGBA16Unorm: return PixelFormat::R16_G16_B16_A16_UNorm;
        case MTLPixelFormatRGBA16Snorm: return PixelFormat::R16_G16_B16_A16_SNorm;
        case MTLPixelFormatRGBA16Uint: return PixelFormat::R16_G16_B16_A16_UInt;
        case MTLPixelFormatRGBA16Sint: return PixelFormat::R16_G16_B16_A16_SInt;
        case MTLPixelFormatRGBA16Float: return PixelFormat::R16_G16_B16_A16_Float;

        case MTLPixelFormatRGBA32Uint: return PixelFormat::R32_G32_B32_A32_UInt;
        case MTLPixelFormatRGBA32Sint: return PixelFormat::R32_G32_B32_A32_SInt;
        case MTLPixelFormatRGBA32Float: return PixelFormat::R32_G32_B32_A32_Float;

        //case PixelFormat::BC1_Rgb_UNorm: return MTLPixelFormatBC1_RGBA;
        //case PixelFormat::BC1_Rgb_UNorm_SRgb: return MTLPixelFormatBC1_RGBA_sRGB;
        case MTLPixelFormatBC1_RGBA: return PixelFormat::BC1_Rgba_UNorm;
        case MTLPixelFormatBC1_RGBA_sRGB: return PixelFormat::BC1_Rgba_UNorm_SRgb;
        case MTLPixelFormatBC2_RGBA: return PixelFormat::BC2_UNorm;
        case MTLPixelFormatBC2_RGBA_sRGB: return PixelFormat::BC2_UNorm_SRgb;
        case MTLPixelFormatBC3_RGBA: return PixelFormat::BC3_UNorm;
        case MTLPixelFormatBC3_RGBA_sRGB: return PixelFormat::BC3_UNorm_SRgb;
        case MTLPixelFormatBC4_RUnorm: return PixelFormat::BC4_UNorm;
        case MTLPixelFormatBC4_RSnorm: return PixelFormat::BC4_SNorm;
        case MTLPixelFormatBC5_RGUnorm: return PixelFormat::BC5_UNorm;
        case MTLPixelFormatBC5_RGSnorm: return PixelFormat::BC5_SNorm;
        case MTLPixelFormatBC7_RGBAUnorm: return PixelFormat::BC7_UNorm;
            //return VkFormat::VK_FORMAT_BC7_UNORM_BLOCK;
        case MTLPixelFormatBC7_RGBAUnorm_sRGB: return PixelFormat::BC7_UNorm_SRgb;
            //return VkFormat::VK_FORMAT_BC7_SRGB_BLOCK;

        case MTLPixelFormatETC2_RGB8: return PixelFormat::ETC2_R8_G8_B8_UNorm;
        case MTLPixelFormatETC2_RGB8A1: return PixelFormat::ETC2_R8_G8_B8_A1_UNorm;

        //Still have doubt for this one
        case MTLPixelFormatEAC_RGBA8: return PixelFormat::ETC2_R8_G8_B8_A8_UNorm;

        case MTLPixelFormatDepth24Unorm_Stencil8: return PixelFormat::D32_Float_S8_UInt;
            //return VkFormat::VK_FORMAT_D32_SFLOAT_S8_UINT;
        case MTLPixelFormatDepth32Float_Stencil8: return PixelFormat::D24_UNorm_S8_UInt;
            //return VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;

        case MTLPixelFormatRGB10A2Unorm: return PixelFormat::R10_G10_B10_A2_UNorm;
        case MTLPixelFormatRGB10A2Uint: return PixelFormat::R10_G10_B10_A2_UInt;
        case MTLPixelFormatRG11B10Float: return PixelFormat::R11_G11_B10_Float;

        default:
            //throw new VeldridException($"Invalid {nameof(PixelFormat)}: {format}");
            assert(false);
            return PixelFormat::Unknown;
        }
    }

    IRFormat AlToMtlShaderDataType(::alloy::ShaderDataType dataType) {
        switch (dataType) {

        case ShaderDataType::Float1: return IRFormatR32Float;
        case ShaderDataType::Float2: return IRFormatR32G32Float;
        case ShaderDataType::Float3: return IRFormatR32G32B32Float;
        case ShaderDataType::Float4: return IRFormatR32G32B32A32Float;
        case ShaderDataType::Byte2_Norm: return IRFormatR8G8Unorm;
        case ShaderDataType::Byte2:      return IRFormatR8G8Uint;
        case ShaderDataType::Byte4_Norm: return IRFormatR8G8B8A8Unorm;
        case ShaderDataType::Byte4:      return IRFormatR8G8B8A8Uint;
        case ShaderDataType::SByte2_Norm: return IRFormatR8G8Snorm;
        case ShaderDataType::SByte2:      return IRFormatR8G8Sint;
        case ShaderDataType::SByte4_Norm: return IRFormatR8G8B8A8Snorm;
        case ShaderDataType::SByte4:      return IRFormatR8G8B8A8Sint;
        case ShaderDataType::UShort2_Norm: return IRFormatR16G16Unorm;
        case ShaderDataType::UShort2:      return IRFormatR16G16Uint;
        case ShaderDataType::UShort4_Norm: return IRFormatR16G16B16A16Unorm;
        case ShaderDataType::UShort4: return IRFormatR16G16B16A16Uint;
        case ShaderDataType::Short2_Norm: return IRFormatR16G16Snorm;
        case ShaderDataType::Short2:      return IRFormatR16G16Sint;
        case ShaderDataType::Short4_Norm: return IRFormatR16G16B16A16Snorm;
        case ShaderDataType::Short4: return IRFormatR16G16B16A16Sint;
        case ShaderDataType::UInt1: return IRFormatR32Uint;
        case ShaderDataType::UInt2: return IRFormatR32G32Uint;
        case ShaderDataType::UInt3: return IRFormatR32G32B32Uint;
        case ShaderDataType::UInt4: return IRFormatR32G32B32A32Uint;
        case ShaderDataType::Int1: return IRFormatR32Sint;
        case ShaderDataType::Int2: return IRFormatR32G32Sint;
        case ShaderDataType::Int3: return IRFormatR32G32B32Sint;
        case ShaderDataType::Int4: return IRFormatR32G32B32A32Sint;
        case ShaderDataType::Half1: return IRFormatR16Float;
        case ShaderDataType::Half2: return IRFormatR16G16Float;
        case ShaderDataType::Half4: return IRFormatR16G16B16A16Float;
            break;
        }
    }


MTLIndexType AlToMtlIndexFormat(::alloy::IndexFormat format) {
    switch (format) {
        case ::alloy::IndexFormat::UInt16 : return MTLIndexTypeUInt16;
        case ::alloy::IndexFormat::UInt32 : return MTLIndexTypeUInt32;
            
        default:
            break;
    }
}


}
