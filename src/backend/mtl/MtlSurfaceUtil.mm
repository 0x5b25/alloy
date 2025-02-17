#include "MtlSurfaceUtil.h"

#include "alloy/common/Macros.h"

#include <Metal/Metal.h>
#include <Foundation/Foundation.h>
#include <QuartzCore/QuartzCore.h>

#ifdef VLD_PLATFORM_MACOS
    #import <AppKit/AppKit.h>
#endif

#if defined(VLD_PLATFORM_IOS) || defined(VLD_PLATFORM_IOS_SIM) || defined(VLD_PLATFORM_MACCATALYST)
    #import <UIKit/UIKit.h>
#endif

#import <QuartzCore/QuartzCore.h>


CAMetalLayer* CreateSurface(id<MTLDevice> gd, alloy::SwapChainSource* src){
    
    CAMetalLayer* container = nullptr;
    
    switch(src->tag){
        case alloy::SwapChainSource::Tag::Opaque: {
            auto opaqueSource = (alloy::OpaqueSwapChainSource*)src;
            container = (CAMetalLayer*)opaqueSource->handle;
            [container retain];
        }break;

#ifdef VLD_PLATFORM_MACOS
        case alloy::SwapChainSource::Tag::NSWindow: {
            auto nsWindowSource = (alloy::NSWindowSwapChainSource*)src;
            NSWindow* nsWindow = (NSWindow*)nsWindowSource->nsWindow;
            NSView* contentView = nsWindow.contentView;
            
            if (![contentView.layer isKindOfClass:[CAMetalLayer class]]) {
                auto _metalLayer = [CAMetalLayer layer];
                contentView.wantsLayer = true;
                contentView.layer = _metalLayer;
            } else {
                [contentView.layer retain];
            }
            container = (CAMetalLayer*)contentView.layer;
        }break;
            
        case alloy::SwapChainSource::Tag::NSView:{
            auto nsViewSource = (alloy::NSViewSwapChainSource*)src;
            NSView* contentView = (NSView*)nsViewSource->nsView;
            
            if (![contentView.layer isKindOfClass:[CAMetalLayer class]]) {
                auto _metalLayer = [CAMetalLayer layer];
                contentView.wantsLayer = true;
                contentView.layer = _metalLayer;
            } else {
                [contentView.layer retain];
            }
            //[contentView release];
            container = (CAMetalLayer*)contentView.layer;
        } break;
#endif       


#if defined(VLD_PLATFORM_IOS) || defined(VLD_PLATFORM_IOS_SIM) || defined(VLD_PLATFORM_MACCATALYST)
        case alloy::SwapChainSource::Tag::UIView: {
            auto nsViewSource = (alloy::UIViewSwapChainSource*)src;
            UIView* contentView = (UIView*)nsViewSource->uiView;
            
            //UIView can't replace its backing layers like those AppKit views
            //We need to insert the CAMetalLayer into UIView's sublayers
            if (![contentView.layer isKindOfClass:[CAMetalLayer class]]) {
                auto _metalLayer = [CAMetalLayer layer];
                _metalLayer.frame = contentView.frame;
                _metalLayer.opaque = true;
                [contentView.layer addSublayer:_metalLayer];
            } else {
                [contentView.layer retain];
            }
            //[contentView release];
            container = (CAMetalLayer*)contentView.layer;
        } break;
#endif
        default: break;
    }
    
    return container;
    
}


