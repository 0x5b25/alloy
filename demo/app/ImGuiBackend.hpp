#pragma once

#include <alloy/GraphicsDevice.hpp>
#include <alloy/CommandList.hpp>
#include <alloy/BindableResource.hpp>
#include <alloy/Pipeline.hpp>
#include <alloy/Buffer.hpp>
#include <imgui.h>

class ImGui_ImplAlloy_InitInfo {

};

class ImGuiAlloyBackend {

    alloy::common::sp<alloy::IGraphicsDevice> gd;

    alloy::common::sp<alloy::IResourceLayout> pipelineLayout;
    alloy::common::sp<alloy::IGfxPipeline> pipeline;
    alloy::common::sp<alloy::IResourceSet> fontRS;

    alloy::common::sp<alloy::IBuffer> mvpBuffer, vertBuffer, indexBuffer;

    void _CreateDeviceObjects();

    void _CreateRenderPipeline();
    void _CreateBuffers();
    void _CreateFontsTexture();

    void _SetupRenderState(ImDrawData* draw_data,
                           alloy::IRenderCommandEncoder* enc);

public:
    ImGuiAlloyBackend(const alloy::common::sp<alloy::IGraphicsDevice>& gd);

    ~ImGuiAlloyBackend();

    void RenderDrawData(ImDrawData* draw_data,
                        const alloy::common::sp<alloy::IRenderTarget>& rt,
                        alloy::ICommandList* command_list);

};

struct ImGui_ImplAlloy_RenderState {
    ImGuiAlloyBackend* pBackend;
    alloy::IRenderCommandEncoder* pCmdList;
};

