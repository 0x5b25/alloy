#pragma once

#include <cstdint>

namespace alloy
{
    /// <summary>
    /// The format of an individual vertex element.
    /// </summary>
    enum class ShaderDataType : std::uint8_t
    {
        /// <summary>
        /// One 32-bit floating point value.
        /// </summary>
        Float1,
        /// <summary>
        /// Two 32-bit floating point values.
        /// </summary>
        Float2,
        /// <summary>
        /// Three 32-bit floating point values.
        /// </summary>
        Float3,
        /// <summary>
        /// Four 32-bit floating point values.
        /// </summary>
        Float4,
        /// <summary>
        /// Two 8-bit unsigned normalized integers.
        /// </summary>
        Byte2_Norm,
        /// <summary>
        /// Two 8-bit unisgned integers.
        /// </summary>
        Byte2,
        /// <summary>
        /// Four 8-bit unsigned normalized integers.
        /// </summary>
        Byte4_Norm,
        /// <summary>
        /// Four 8-bit unsigned integers.
        /// </summary>
        Byte4,
        /// <summary>
        /// Two 8-bit signed normalized integers.
        /// </summary>
        SByte2_Norm,
        /// <summary>
        /// Two 8-bit signed integers.
        /// </summary>
        SByte2,
        /// <summary>
        /// Four 8-bit signed normalized integers.
        /// </summary>
        SByte4_Norm,
        /// <summary>
        /// Four 8-bit signed integers.
        /// </summary>
        SByte4,
        /// <summary>
        /// Two 16-bit unsigned normalized integers.
        /// </summary>
        UShort2_Norm,
        /// <summary>
        /// Two 16-bit unsigned integers.
        /// </summary>
        UShort2,
        /// <summary>
        /// Four 16-bit unsigned normalized integers.
        /// </summary>
        UShort4_Norm,
        /// <summary>
        /// Four 16-bit unsigned integers.
        /// </summary>
        UShort4,
        /// <summary>
        /// Two 16-bit signed normalized integers.
        /// </summary>
        Short2_Norm,
        /// <summary>
        /// Two 16-bit signed integers.
        /// </summary>
        Short2,
        /// <summary>
        /// Four 16-bit signed normalized integers.
        /// </summary>
        Short4_Norm,
        /// <summary>
        /// Four 16-bit signed integers.
        /// </summary>
        Short4,
        /// <summary>
        /// One 32-bit unsigned integer.
        /// </summary>
        UInt1,
        /// <summary>
        /// Two 32-bit unsigned integers.
        /// </summary>
        UInt2,
        /// <summary>
        /// Three 32-bit unsigned integers.
        /// </summary>
        UInt3,
        /// <summary>
        /// Four 32-bit unsigned integers.
        /// </summary>
        UInt4,
        /// <summary>
        /// One 32-bit signed integer.
        /// </summary>
        Int1,
        /// <summary>
        /// Two 32-bit signed integers.
        /// </summary>
        Int2,
        /// <summary>
        /// Three 32-bit signed integers.
        /// </summary>
        Int3,
        /// <summary>
        /// Four 32-bit signed integers.
        /// </summary>
        Int4,
        /// <summary>
        /// One 16-bit floating point value.
        /// </summary>
        Half1,
        /// <summary>
        /// Two 16-bit floating point values.
        /// </summary>
        Half2,
        /// <summary>
        /// Four 16-bit floating point values.
        /// </summary>
        Half4,
    };

    enum class ShaderConstantType : std::uint8_t
    {
        /// <summary>
        /// A boolean.
        /// </summary>
        Bool,
        /// <summary>
        /// A 16-bit unsigned integer.
        /// </summary>
        UInt16,
        /// <summary>
        /// A 16-bit signed integer.
        /// </summary>
        Int16,
        /// <summary>
        /// A 32-bit unsigned integer.
        /// </summary>
        UInt32,
        /// <summary>
        /// A 32-bit signed integer.
        /// </summary>
        Int32,
        /// <summary>
        /// A 64-bit unsigned integer.
        /// </summary>
        UInt64,
        /// <summary>
        /// A 64-bit signed integer.
        /// </summary>
        Int64,
        /// <summary>
        /// A 32-bit floating-point value.
        /// </summary>
        Float,
        /// <summary>
        /// A 64-bit floating-point value.
        /// </summary>
        Double,
    };

    enum class PixelFormat : std::uint8_t{
        Unknown,
        R8_G8_B8_A8_UNorm,
        B8_G8_R8_A8_UNorm,
        R8_UNorm,
        R16_UNorm,
        R32_G32_B32_A32_Float,
        R32_Float,
        BC3_UNorm,
        D24_UNorm_S8_UInt,
        D32_Float_S8_UInt,
        R32_G32_B32_A32_UInt,
        R8_G8_SNorm,
        BC1_Rgb_UNorm,
        BC1_Rgba_UNorm,
        BC2_UNorm,
        R10_G10_B10_A2_UNorm,
        R10_G10_B10_A2_UInt,
        R11_G11_B10_Float,
        R8_SNorm,
        R8_UInt,
        R8_SInt,
        R16_SNorm,
        R16_UInt,
        R16_SInt,
        R16_Float,
        R32_UInt,
        R32_SInt,
        R8_G8_UNorm,
        R8_G8_UInt,
        R8_G8_SInt,
        R16_G16_UNorm,
        R16_G16_SNorm,
        R16_G16_UInt,
        R16_G16_SInt,
        R16_G16_Float,
        R32_G32_UInt,
        R32_G32_SInt,
        R32_G32_Float,
        R8_G8_B8_A8_SNorm,
        R8_G8_B8_A8_UInt,
        R8_G8_B8_A8_SInt,
        R16_G16_B16_A16_UNorm,
        R16_G16_B16_A16_SNorm,
        R16_G16_B16_A16_UInt,
        R16_G16_B16_A16_SInt,
        R16_G16_B16_A16_Float,
        R32_G32_B32_A32_SInt,
        ETC2_R8_G8_B8_UNorm,
        ETC2_R8_G8_B8_A1_UNorm,
        ETC2_R8_G8_B8_A8_UNorm,
        BC4_UNorm,
        BC4_SNorm,
        BC5_UNorm,
        BC5_SNorm,
        BC7_UNorm,
        R8_G8_B8_A8_UNorm_SRgb,
        B8_G8_R8_A8_UNorm_SRgb,
        BC1_Rgb_UNorm_SRgb,
        BC1_Rgba_UNorm_SRgb,
        BC2_UNorm_SRgb,
        BC3_UNorm_SRgb,
        BC7_UNorm_SRgb,
    };

    /// <summary>
    /// The format of index data used in a <see cref="DeviceBuffer"/>.
    /// </summary>
    enum class IndexFormat : std::uint8_t
    {
        /// <summary>
        /// Each index is a 16-bit unsigned integer (System.UInt16).
        /// </summary>
        UInt16,
        /// <summary>
        /// Each index is a 32-bit unsigned integer (System.UInt32).
        /// </summary>
        UInt32,
    };

    /// <summary>
    /// Describes a 3-dimensional region.
    /// </summary>
    struct Viewport
    {
        /// The minimum XY value.
        float x, y;
        
        float width, height;
        
        // The minimum depth.
        float minDepth;
        
        // The maximum depth.
        float maxDepth;
    };

    struct Rect {
        std::uint32_t x, y;
        std::uint32_t width, height;
    };

    struct Point3D {
        
        std::uint32_t x, y, z;
    };

    struct Size3D {
        
        std::uint32_t width, height, depth;
    };

    struct Box {
        Point3D origin;
        Size3D size;
    };

    struct Color4f {
        float r,g,b,a;
    };

    enum class HostAccess{
        None,

        //Equivalent to DX12 READ_BACK heap, preferred system memory
        //Typical usage: GPU write, host read
        SystemMemoryPreferRead,

        //Equivalent to DX12 UPLOAD heap, preferred device local memory
        //Typical usage: Host write, GPU read
        SystemMemoryPreferWrite,

        //For resizable bar enabled devices, fast for CPU writes,
        //normally use non-cached WRITE_COMBINED policy
        //Typical usage: Host write, GPU read
        PreferDeviceMemory
    };

    enum class SampleCount{
        x1, x2, x4, x8, x16, x32,
    };
} // namespace alloy

