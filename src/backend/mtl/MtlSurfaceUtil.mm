#include "MtlSurfaceUtil.hpp"

#include <Metal/Metal.h>
#include <Foundation/Foundation.h>
#include <QuartzCore/QuartzCore.h>

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>


MtlSurfaceContainer CreateSurface(id<MTLDevice> gd, Veldrid::SwapChainSource* src){
    
    MtlSurfaceContainer container{};
    
    switch(src->tag){
        case Veldrid::SwapChainSource::Tag::Opaque: {
            auto opaqueSource = (Veldrid::OpaqueSwapChainSource*)src;
            container.layer = (CAMetalLayer*)opaqueSource->handle;
            container.isOwnSurface = false;
        }break;
            
        case Veldrid::SwapChainSource::Tag::NSWindow: {
            auto nsWindowSource = (Veldrid::NSWindowSwapChainSource*)src;
            NSWindow* nsWindow = (NSWindow*)nsWindowSource->nsWindow;
            NSView* contentView = nsWindow.contentView;
            CGSize windowContentSize = contentView.frame.size;
            uint width = windowContentSize.width;
            uint height = windowContentSize.height;
            
            if (![contentView.layer isKindOfClass:[CAMetalLayer class]]) {
                auto _metalLayer = [CAMetalLayer layer];
                contentView.wantsLayer = true;
                contentView.layer = _metalLayer;
            }
            
            container.layer = (CAMetalLayer*)contentView.layer;
            container.isOwnSurface = true;
        }break;
            
        case Veldrid::SwapChainSource::Tag::NSView:{
            auto nsViewSource = (Veldrid::NSViewSwapChainSource*)src;
            NSView* contentView = (NSView*)nsViewSource->nsView;
            CGSize windowContentSize = contentView.frame.size;
            uint width = windowContentSize.width;
            uint height = windowContentSize.height;
            
            if (![contentView.layer isKindOfClass:[CAMetalLayer class]]) {
                auto _metalLayer = [CAMetalLayer layer];
                contentView.wantsLayer = true;
                contentView.layer = _metalLayer;
            }
            
            //[contentView release];
            
            container.layer = (CAMetalLayer*)contentView.layer;
            container.isOwnSurface = true;
        } break;
            
        //case Veldrid::SwapChainSource::Tag::UIView: {
            
        //} break;
            
        default: break;
    }
    
    return container;
    
}


