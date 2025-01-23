#pragma once

#include <AppKit/AppKit.h>
#include <QuartzCore/QuartzCore.h>

#include "alloy/SwapChainSources.hpp"

struct MtlSurfaceContainer{
    CAMetalLayer* layer;
    bool isOwnSurface;//Is this surface managed by us
};


MtlSurfaceContainer CreateSurface(id<MTLDevice> gd, alloy::SwapChainSource* src);
