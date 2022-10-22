#pragma once

#include "veldrid/DeviceResource.hpp"
#include "veldrid/Framebuffer.hpp"
#include "veldrid/SwapChainSources.hpp"

#include <cstdint>
#include <optional>

namespace Veldrid
{
    

    
    class SwapChain : public DeviceResource{

    public:
        struct Description
        {
            // The <see cref="SwapchainSource"/> which will be used as the target of rendering operations.
            // This is a window-system-specific object which differs by platform.
            // Currently surface is bound to the acutal device, this source has no effect.
            SwapChainSource* source;
            
            std::uint32_t initialWidth, initialHeight;
            
            // The optional format of the depth target of the Swapchain's Framebuffer.
            // If non-null, this must be a valid depth Texture format.
            // If null, then no depth target will be created.
            std::optional<PixelFormat> depthFormat;

            // Indicates whether presentation of the Swapchain will be synchronized to the window system's vertical refresh rate.
            bool syncToVerticalBlank;

            // Indicates whether the color target of the Swapchain will use an sRGB PixelFormat.
            bool colorSrgb;
        };
        
    protected:
        Description description;

        SwapChain(
            sp<GraphicsDevice>&& dev,
            const Description& desc
        ) 
            : DeviceResource(std::move(dev))
            , description(desc) {}

    public:

        virtual sp<Framebuffer> GetFramebuffer() = 0;

        virtual void Resize(
            std::uint32_t width, 
            std::uint32_t height) = 0;

        virtual bool IsSyncToVerticalBlank() const = 0;
        virtual void SetSyncToVerticalBlank(bool sync) = 0;

        virtual std::uint32_t GetWidth() const = 0;
        virtual std::uint32_t GetHeight() const = 0;

    };
    
} // namespace Veldrid

