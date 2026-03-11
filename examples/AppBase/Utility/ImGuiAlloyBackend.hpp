#pragma once

#include "imgui.h"      // IMGUI_IMPL_API
#ifndef IMGUI_DISABLE

#include <alloy/alloy.hpp>

#include "BumpAllocator.hpp"

struct ImGui_ImplAlloy_InitInfo {
    alloy::common::sp<alloy::IGraphicsDevice> device;

    alloy::SampleCount msaaSampleCount;
    alloy::PixelFormat renderTargetFormat;
};

class ImGuiAlloyBackend;

struct ImGui_ImplAlloy_RenderState {
    ImGuiAlloyBackend* pBackend;
    alloy::IRenderCommandEncoder* pRenderPass;
};

// Follow "Getting Started" link and check examples/ folder to learn about using backends!
IMGUI_IMPL_API bool  ImGui_ImplAlloy_Init(const ImGui_ImplAlloy_InitInfo& info);
IMGUI_IMPL_API void  ImGui_ImplAlloy_Shutdown();
IMGUI_IMPL_API void  ImGui_ImplAlloy_NewFrame();
IMGUI_IMPL_API void  ImGui_ImplAlloy_NotifyFrameEnd();
IMGUI_IMPL_API void  ImGui_ImplAlloy_RenderDrawData(ImDrawData* draw_data, alloy::IRenderCommandEncoder& renderPass);


#endif // #ifndef IMGUI_DISABLE
