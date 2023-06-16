#pragma once

#include <AppKit/AppKit.h>
#include <QuartzCore/QuartzCore.h>

#include "veldrid/SwapChainSources.hpp"

struct MtlSurfaceContainer{
    CAMetalLayer* layer;
    bool isOwnSurface;//Is this surface managed by us
};


MtlSurfaceContainer CreateSurface(id<MTLDevice> gd, Veldrid::SwapChainSource* src);
