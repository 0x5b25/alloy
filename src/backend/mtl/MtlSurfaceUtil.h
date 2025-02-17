#pragma once

#include <QuartzCore/QuartzCore.h>

#include "alloy/SwapChainSources.hpp"

struct MtlSurfaceContainer{
    CAMetalLayer* layer;
    bool isOwnSurface;//Is this surface managed by us
};


CAMetalLayer* CreateSurface(id<MTLDevice> gd, alloy::SwapChainSource* src);
