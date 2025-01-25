
#include "app/App.hpp"
#include "app/HLSLCompiler.hpp"
#include "app/ImGuiBackend.hpp"
#include <backends/imgui_impl_glfw.h>
#include <imgui.h>

#include <alloy/GraphicsDevice.hpp>
#include <alloy/ResourceFactory.hpp>
#include <alloy/CommandList.hpp>
#include <alloy/CommandQueue.hpp>
#include <alloy/backend/Backends.hpp>
#include <alloy/SwapChain.hpp>

using namespace alloy;

class ImguiApp : public AppBase {

    //Basic resources:
    alloy::common::sp<alloy::IGraphicsDevice> dev;
    alloy::common::sp<alloy::ISwapChain> swapChain;
    //Swapchain backbuffers requires image layout transitions
    //from undefined to target layout. This counter corresponds
    //to swapchain backbuffer count
    uint32_t _initSubmissionCnt = 2;
    
    alloy::common::sp<alloy::IEvent> renderFinishFence;
    uint64_t renderFinishFenceValue = 0;

    //IMGUI backend
    ImGuiAlloyBackend* pBackend;

public:
    
    ImguiApp() : AppBase(800, 600, "Hello ImGui") {}

private:

    void SetupImGui() {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsLight();
        ImGui_ImplGlfw_InitForOther(window, true);

        //Setup alloy backend
        pBackend = new ImGuiAlloyBackend(dev);
    }

    void TearDownImGui() {
        delete pBackend;
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void SetupAlloyEnv(alloy::SwapChainSource* swapChainSrc) {
        
        alloy::IGraphicsDevice::Options opt{};
        opt.debug = true;
        opt.preferStandardClipSpaceYDirection = true;
        //dev = alloy::CreateVulkanGraphicsDevice(opt, swapChainSrc);
        //dev = alloy::CreateVulkanGraphicsDevice(opt);
        dev = alloy::CreateDX12GraphicsDevice(opt);

        alloy::ISwapChain::Description swapChainDesc{};
        swapChainDesc.source = swapChainSrc;
        swapChainDesc.initialWidth = initialWidth;
        swapChainDesc.initialHeight = initialHeight;
        swapChainDesc.depthFormat = alloy::PixelFormat::D24_UNorm_S8_UInt;
        swapChainDesc.backBufferCnt = _initSubmissionCnt;
        swapChain = dev->GetResourceFactory().CreateSwapChain(swapChainDesc);

        renderFinishFence = dev->GetResourceFactory().CreateSyncEvent();
    }

    void TearDownAlloyEnv() {
        dev->WaitForIdle();
    }

    bool ResizeSwapChainIfNecessary() {
        int currWidth, currHeight;
        GetFramebufferSize(currWidth, currHeight);
        if( (currWidth != initialWidth) || (currHeight != initialHeight) ) {
            while (currWidth == 0 || currHeight == 0) {
                GetFramebufferSize(currWidth, currHeight);
                glfwWaitEvents();
            }
            dev->WaitForIdle();
            swapChain->Resize(currWidth, currHeight);
            initialWidth = currWidth;
            initialHeight = currHeight;
            _initSubmissionCnt = 2;

            return true;
        }

        return false;
    }

protected:
    virtual void OnAppStart(
        alloy::SwapChainSource* swapChainSrc,
        unsigned surfaceWidth,
        unsigned surfaceHeight) override
    {
        SetupAlloyEnv(swapChainSrc);
        SetupImGui();
    }

    virtual void OnAppExit() override {
        TearDownImGui();
        TearDownAlloyEnv();
    }

    //true: continue, false: exit
    virtual bool OnAppUpdate(float deltaSec) override {

        auto isResized = ResizeSwapChainIfNecessary();

        //Render scene
        auto& factory = dev->GetResourceFactory();
        //Get one drawable
        auto backBuffer = swapChain->GetBackBuffer();
        
        auto backBufferDesc = backBuffer->GetDesc();

        auto gfxQ = dev->GetGfxCommandQueue();
        auto _commandList = gfxQ->CreateCommandList();
        //Record command buffer
        _commandList->Begin();

        {
            auto initialLayout = alloy::TextureLayout::UNDEFINED;
            
            bool isInitSubmission = false;
            if(_initSubmissionCnt > 0) {
                _initSubmissionCnt--;
                isInitSubmission = true;
            }

            // Indicate that the back buffer will be used as a render target.
            //auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            //m_commandList->ResourceBarrier(1, &barrier);
            alloy::BarrierDescription texBarrier{
                .memBarrier = {
                    .stagesBefore = isInitSubmission ? alloy::PipelineStages{} : alloy::PipelineStage::All,
                    .stagesAfter = alloy::PipelineStage::RENDER_TARGET,
                    .accessBefore = isInitSubmission ? alloy::ResourceAccesses{} : alloy::ResourceAccess::COMMON,
                    .accessAfter = alloy::ResourceAccess::RENDER_TARGET,
                },

                .resourceInfo = alloy::TextureBarrierResource {
                    .fromLayout = isInitSubmission? initialLayout : alloy::TextureLayout::PRESENT,
                    .toLayout = alloy::TextureLayout::RENDER_TARGET,
                    .resource = backBufferDesc.colorAttachments[0]->GetTexture().GetTextureObject()
                }
                //.barriers = { texBarrier, dsBarrier }
            };


            //if(_isFirstSubmission) {
            alloy::BarrierDescription dsBarrier{
                .memBarrier = {
                    .stagesBefore = isInitSubmission ? alloy::PipelineStages{} : alloy::PipelineStage::All,
                    .stagesAfter = alloy::PipelineStage::DEPTH_STENCIL,
                    .accessBefore = isInitSubmission ? alloy::ResourceAccesses{} : alloy::ResourceAccess::COMMON,
                    .accessAfter = alloy::ResourceAccess::DEPTH_STENCIL_WRITE,
                },

                .resourceInfo = alloy::TextureBarrierResource {
                    .fromLayout = isInitSubmission? initialLayout : alloy::TextureLayout::COMMON,
                    .toLayout = alloy::TextureLayout::DEPTH_STENCIL_WRITE,
                    .resource = backBufferDesc.depthAttachment->GetTexture().GetTextureObject()
                }
            };
                //_isFirstSubmission = false;
            //}
            
            _commandList->Barrier({ texBarrier, dsBarrier });
        }
        
        auto fbDesc = swapChain->GetBackBuffer()->GetDesc();
        
        alloy::RenderPassAction passAction{};
        auto& ctAct = passAction.colorTargetActions.emplace_back();
        ctAct.loadAction = alloy::LoadAction::Clear;
        ctAct.storeAction = alloy::StoreAction::Store;
        ctAct.clearColor = {0.9, 0.1, 0.3, 1};
        ctAct.target = fbDesc.colorAttachments.front();

        auto& dtAct = passAction.depthTargetAction.emplace();
        dtAct.loadAction = alloy::LoadAction::Clear;
        dtAct.storeAction = alloy::StoreAction::DontCare;
        dtAct.target = fbDesc.depthAttachment;
        
        auto& stAct = passAction.stencilTargetAction.emplace();
        stAct.loadAction = alloy::LoadAction::Load;
        stAct.storeAction = alloy::StoreAction::Store;
        stAct.target = fbDesc.depthAttachment;

        auto& pass = _commandList->BeginRenderPass(passAction);
        //_commandList->BeginRenderPass(fb);
        
        _commandList->EndPass();
        

        //Wait for previous render to complete
        //renderFinishFence->WaitFromCPU(renderFinishFenceValue);
        //renderFinishFenceValue++;
        //Update uniformbuffer
        //memcpy(ubMapped, &ubo, sizeof(ubo));
        
        

        //StopCapture();

        //Process UI
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        ImGui::ShowDemoWindow();
        
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();

        //Render UI
        //Alloy backend render
        pBackend->RenderDrawData(draw_data, fbDesc.colorAttachments.front(), _commandList.get());

        // Indicate that the back buffer will now be used to present.
        {
            //auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            //m_commandList->ResourceBarrier(1, &barrier);
            alloy::BarrierDescription texBarrier{
                .memBarrier = {
                    .stagesBefore = alloy::PipelineStage::RENDER_TARGET,
                    .stagesAfter = alloy::PipelineStage::All,
                    .accessBefore = alloy::ResourceAccess::RENDER_TARGET,
                    .accessAfter = alloy::ResourceAccess::COMMON,
                },

                .resourceInfo = alloy::TextureBarrierResource {
                    .fromLayout = alloy::TextureLayout::RENDER_TARGET,
                    .toLayout = alloy::TextureLayout::PRESENT,
                    .resource = backBufferDesc.colorAttachments[0]->GetTexture().GetTextureObject()
                }
                //.barriers = { texBarrier, dsBarrier }
            };


            //if(_isFirstSubmission) {
            alloy::BarrierDescription dsBarrier{
                .memBarrier = {
                    .stagesBefore = alloy::PipelineStage::DEPTH_STENCIL,
                    .stagesAfter = alloy::PipelineStage::All,
                    .accessBefore = alloy::ResourceAccess::DEPTH_STENCIL_WRITE,
                    .accessAfter = alloy::ResourceAccess::COMMON,
                },

                .resourceInfo = alloy::TextureBarrierResource {
                    .fromLayout = alloy::TextureLayout::DEPTH_STENCIL_WRITE,
                    .toLayout = alloy::TextureLayout::COMMON,
                    .resource = backBufferDesc.depthAttachment->GetTexture().GetTextureObject()
                }
            };

            _commandList->Barrier({ texBarrier, dsBarrier });
        }

        _commandList->End();

        //Submit
        gfxQ->SubmitCommand(_commandList.get());
        //Present
        //Wait for command finish
        
        renderFinishFenceValue++;
        gfxQ->EncodeSignalEvent(renderFinishFence.get(), renderFinishFenceValue );
        // /cmd = _commandList;
        //Wait for render to complete
        renderFinishFence->WaitFromCPU(renderFinishFenceValue);

        /* Swap front and back buffers */
        //glfwSwapBuffers(window);
        dev->PresentToSwapChain(
            swapChain.get());

        return true;
    }

};

int main() {
	ImguiApp app;
	app.Run();
}
