#pragma once

#include <string>

#include "d3d12.h"

namespace alloy::dxc
{
    void EncodePIXBeginEvent(ID3D12GraphicsCommandList* context, std::uint64_t color, const std::string& str);
    void EncodePIXEndEvent(ID3D12GraphicsCommandList* context);
    void EncodePIXMarker(ID3D12GraphicsCommandList* context, std::uint64_t color, const std::string& str);
} // namespace alloy::dxc

