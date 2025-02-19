#pragma once

#include <cstdint>
#include "alloy/common/Macros.h"


namespace alloy
{

    struct SwapChainSource {
        enum class Tag : std::uint32_t{
            Opaque,
            Win32,
            NSView,
            UIView,
            NSWindow,
        } tag;

        SwapChainSource(SwapChainSource::Tag tag) :tag(tag) {}
    };

    struct OpaqueSwapChainSource : SwapChainSource
    {
        void* handle;

        OpaqueSwapChainSource(void* handle)
            : SwapChainSource(SwapChainSource::Tag::Opaque),
            handle(handle)
        {}
    };
#ifdef VLD_PLATFORM_WIN32
    struct Win32SwapChainSource : SwapChainSource
    {
        void *hWnd,*hInstance;

        Win32SwapChainSource(void* hWnd, void* hInstance)
            : SwapChainSource(SwapChainSource::Tag::Win32),
            hWnd(hWnd), hInstance(hInstance)
        {}
    };
#endif

#ifdef VLD_PLATFORM_MACOS
    struct NSViewSwapChainSource : SwapChainSource
    {
        void* nsView;

        NSViewSwapChainSource(void* nsView)
            : SwapChainSource(SwapChainSource::Tag::NSView),
            nsView(nsView)
        {}
    };

    struct NSWindowSwapChainSource : SwapChainSource
    {
        void* nsWindow;

        NSWindowSwapChainSource(void* nsWindow)
        : SwapChainSource(SwapChainSource::Tag::NSWindow),
        nsWindow(nsWindow)
        {}
    };


#endif


#if defined(VLD_PLATFORM_IOS) || defined(VLD_PLATFORM_IOS_SIM) || defined(VLD_PLATFORM_MACCATALYST)
    struct UIViewSwapChainSource : SwapChainSource
    {
        void* uiView;

        UIViewSwapChainSource(void* uiView)
            : SwapChainSource(SwapChainSource::Tag::UIView)
            , uiView(uiView)
        { }
    };
#endif
    /*
    internal class UwpSwapchainSource : SwapchainSource
    {
        public object SwapChainPanelNative{ get; }
        public float LogicalDpi{ get; }

            public UwpSwapchainSource(object swapChainPanelNative, float logicalDpi)
        {
            SwapChainPanelNative = swapChainPanelNative;
            LogicalDpi = logicalDpi;
        }
    }

    internal class XlibSwapchainSource : SwapchainSource
    {
        public IntPtr Display{ get; }
        public IntPtr Window{ get; }

            public XlibSwapchainSource(IntPtr display, IntPtr window)
        {
            Display = display;
            Window = window;
        }
    }

    internal class WaylandSwapchainSource : SwapchainSource
    {
        public IntPtr Display{ get; }
        public IntPtr Surface{ get; }

            public WaylandSwapchainSource(IntPtr display, IntPtr surface)
        {
            Display = display;
            Surface = surface;
        }
    }



    internal class AndroidSurfaceSwapchainSource : SwapchainSource
    {
        public IntPtr Surface{ get; }
        public IntPtr JniEnv{ get; }

            public AndroidSurfaceSwapchainSource(IntPtr surfaceHandle, IntPtr jniEnv)
        {
            Surface = surfaceHandle;
            JniEnv = jniEnv;
        }
    }

    internal */
}

