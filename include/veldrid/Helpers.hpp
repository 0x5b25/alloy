#pragma once

#include "veldrid/common/RefCnt.hpp"
#include "veldrid/Types.hpp"
#include "veldrid/Texture.hpp"

#include <cstdint>
#include <cmath>
#include <type_traits>

namespace Veldrid::Helpers
{

    template<typename T>
    using base_type =
        typename std::remove_cv<
            typename std::remove_reference<T>::type
        >::type;

    std::uint32_t Clamp(std::uint32_t value, std::uint32_t min, std::uint32_t max);

    

    std::uint32_t GetDimension(std::uint32_t largestLevelDimension, std::uint32_t mipLevel);

    void GetMipDimensions(
        const Veldrid::Texture::Description& texDesc, 
        std::uint32_t mipLevel, 
        std::uint32_t& width, std::uint32_t& height, std::uint32_t& depth);

    void GetMipLevelAndArrayLayer(
        const Veldrid::Texture::Description& texDesc, std::uint32_t subresource, 
        std::uint32_t& mipLevel, std::uint32_t& arrayLayer);


    std::uint64_t ComputeSubresourceOffset(
        const Veldrid::Texture::Description& texDesc, std::uint32_t mipLevel, std::uint32_t arrayLayer);

    std::uint32_t ComputeArrayLayerOffset(
        const Veldrid::Texture::Description& texDesc, std::uint32_t arrayLayer);

    std::uint32_t ComputeMipOffset(
        const Veldrid::Texture::Description& texDesc, std::uint32_t mipLevel);

    namespace FormatHelpers{
        /// <summary>
        /// Given a pixel format, returns the number of bytes required to store
        /// a single pixel.
        /// Compressed formats may not be used with this method as the number of
        /// bytes per pixel is variable.
        /// </summary>
        /// <param name="format">An uncompressed pixel format</param>
        /// <returns>The number of bytes required to store a single pixel in the given format</returns>
        std::uint32_t GetSizeInBytes(const PixelFormat& format);

        /// <summary>
        /// Given a vertex element format, returns the number of bytes required
        /// to store an element in that format.
        /// </summary>
        /// <param name="format">A vertex element format</param>
        /// <returns>The number of bytes required to store an element in the given format</returns>
        std::uint32_t GetSizeInBytes(const ShaderDataType& format);

        std::uint32_t GetSizeInBytes(ShaderConstantType type);

        std::int32_t GetElementCount(ShaderDataType format);

        std::uint32_t GetSampleCountUInt32(
            Veldrid::Texture::Description::SampleCount sampleCount);

        bool IsStencilFormat(PixelFormat format);

        bool IsDepthStencilFormat(PixelFormat format);

        bool IsCompressedFormat(PixelFormat format);

        std::uint32_t GetRowPitch(
            std::uint32_t width, const PixelFormat& format);

        std::uint32_t GetBlockSizeInBytes(const PixelFormat& format);

        bool IsSrgbCounterpart(const PixelFormat& viewFormat, const PixelFormat& realFormat);

        bool IsFormatViewCompatible(
            const PixelFormat& viewFormat, const PixelFormat& realFormat);

        

        std::uint32_t GetNumRows(std::uint32_t height, const PixelFormat& format);

        std::uint32_t GetDepthPitch(std::uint32_t rowPitch, std::uint32_t height, PixelFormat format);

        std::uint32_t GetRegionSize(std::uint32_t width, std::uint32_t height, std::uint32_t depth, PixelFormat format);

        Veldrid::Texture::Description::SampleCount GetSampleCount(
            std::uint32_t samples);

        PixelFormat GetViewFamilyFormat(const PixelFormat& format);
    }
} // namespace Veldrid
