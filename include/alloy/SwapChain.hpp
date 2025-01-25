#pragma once

#include "alloy/FrameBuffer.hpp"
#include "alloy/SwapChainSources.hpp"
#include "alloy/SyncObjects.hpp"

#include <cstdint>
#include <optional>

namespace alloy
{
    

    
    class ISwapChain : public common::RefCntBase{

    public:
        enum class State {
            //Swap chain is operational and optimal
            Optimal, 
            //Swap chain is operational, but properties don't match exactly, recreation is advised.
            Suboptimal, 
            //Swap chain is inoperational, maybe size is changed, recreation is needed.
            OutOfDate,
            //Swap chain is inoperational, an error has occured.
            Error
        };

        struct Description
        {
            // The <see cref="SwapchainSource"/> which will be used as the target of rendering operations.
            // This is a window-system-specific object which differs by platform.
            SwapChainSource* source;

            ///#TODO: add surface format selection. Currently default to RGBA8_UNORM as it's supported
            //on most devices.
            
            std::uint32_t initialWidth, initialHeight;

            //How many images inside this swapchain. Can't go below 2
            ///#TODO: add swapchain capability query interface
            uint32_t backBufferCnt;
            
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

        ISwapChain(const Description& desc) : description(desc) {}

    public:
        //ISwapChain() {}
        virtual common::sp<IFrameBuffer> GetBackBuffer() = 0;

        virtual void Resize(
            std::uint32_t width, 
            std::uint32_t height) = 0;

        virtual bool IsSyncToVerticalBlank() const = 0;
        virtual void SetSyncToVerticalBlank(bool sync) = 0;

        virtual std::uint32_t GetWidth() const = 0;
        virtual std::uint32_t GetHeight() const = 0;

        //virtual State SwapBuffers() = 0;
    };
    
} // namespace alloy

