#pragma once

#include <cstdint>

namespace Veldrid
{

    struct SwapChainSource {
        enum class Tag : std::uint32_t{
            Opaque,
            Win32,
        } tag;

        SwapChainSource(SwapChainSource::Tag tag) :tag(tag) {}
    };

    struct OpaqueSwapchainSource : SwapChainSource
    {
        void* handle;

        OpaqueSwapchainSource(void* handle)
            : SwapChainSource(SwapChainSource::Tag::Opaque),
            handle(handle)
        {}
    };
       
    struct Win32SwapchainSource : SwapChainSource
    {
        void *hWnd,*hInstance;

        Win32SwapchainSource(void* hWnd, void* hInstance)
            : SwapChainSource(SwapChainSource::Tag::Win32),
            hWnd(hWnd), hInstance(hInstance)
        {}
    };
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

    internal class NSWindowSwapchainSource : SwapchainSource
    {
        public IntPtr NSWindow{ get; }

            public NSWindowSwapchainSource(IntPtr nsWindow)
        {
            NSWindow = nsWindow;
        }
    }

    internal class UIViewSwapchainSource : SwapchainSource
    {
        public IntPtr UIView{ get; }

            public UIViewSwapchainSource(IntPtr uiView)
        {
            UIView = uiView;
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

    internal class NSViewSwapchainSource : SwapchainSource
    {
        public IntPtr NSView{ get; }

            public NSViewSwapchainSource(IntPtr nsView)
        {
            NSView = nsView;
        }
    }*/
}

