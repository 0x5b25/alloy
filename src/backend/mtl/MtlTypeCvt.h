#pragma once

#include "alloy/Types.hpp"

#import <Metal/Metal.h>
#include <metal_irconverter/ir_format.h>

namespace alloy::mtl {

    MTLPixelFormat AlToMtlPixelFormat(::alloy::PixelFormat format);
    ::alloy::PixelFormat MtlToAlPixelFormat(MTLPixelFormat format);

    IRFormat AlToMtlShaderDataType(::alloy::ShaderDataType dataType);

    MTLIndexType AlToMtlIndexFormat(::alloy::IndexFormat format);
                 //AlToMtlIndexFormat(::alloy::IndexFormat format)
}
