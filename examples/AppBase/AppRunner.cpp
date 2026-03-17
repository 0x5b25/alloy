#include "AppRunner.hpp"

#include <cmath>
#include <limits>
#include <iostream>
#include <stdexcept>
#include <format>

#include <GLFW/glfw3.h>

#if defined(VLD_PLATFORM_WIN32)

    //#include <Windows.h>
    
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>

#elif defined(VLD_PLATFORM_MACOS)

    #define GLFW_EXPOSE_NATIVE_COCOA
    #include <GLFW/glfw3native.h>

#endif

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include "Utility/ImGuiAlloyBackend.hpp"

TimeServiceImpl::TimeServiceImpl(float fixedUpdatePerSecond) 
    : _ticksPerSec(Clock::period::den)
    , _ticksPerFixedUpdate(std::round((double)_ticksPerSec / fixedUpdatePerSecond))
    , _startTime(Clock::now())
    , _deltaTicks(0)
    , _frameStartTime(_startTime)
{ }


std::uint64_t TimeServiceImpl::GetCurrentTick() const {
    //auto now = Clock::now();
    //auto delta = now - _startTime;
    //return delta.count();
    auto delta = _frameStartTime - _startTime;
    return delta.count();
}


float TimeServiceImpl::GetElapsedSeconds() {
    return GetCurrentTick() / (float)_ticksPerSec;
}

float TimeServiceImpl::GetDeltaSeconds() {
    return _deltaTicks / (float)_ticksPerSec;
}

float TimeServiceImpl::GetFixedUpdateDeltaSeconds() {
    return _ticksPerFixedUpdate / (float)_ticksPerSec;
}

std::uint64_t TimeServiceImpl::BeginNewFrame() {
    auto now = Clock::now();
    auto delta = now - _frameStartTime;
    _deltaTicks = delta.count();
    _frameStartTime = now;
    return 0;
}


alloy_sp<alloy::IGraphicsDevice> RenderServiceImpl::GetDevice() {
    return _runner->_dev;
}
uint32_t RenderServiceImpl::GetCurrentFrameIndex() {
    return _runner->_nextFrameIdx;
}

void RenderServiceImpl::GetFrameBufferSize(uint32_t& width, uint32_t& height) {
    width = _runner->_wndWidth;
    height = _runner->_wndHeight;
}


alloy::PixelFormat RenderServiceImpl::GetFrameBufferColorFormat() {
    return alloy::PixelFormat::B8_G8_R8_A8_UNorm;
}
alloy::PixelFormat RenderServiceImpl::GetFrameBufferDepthStencilFormat() {
    return alloy::PixelFormat::D24_UNorm_S8_UInt;
}
alloy::SampleCount RenderServiceImpl::GetFrameBufferSampleCount() {
    return _runner->_msaaSampleCnt;
}

AppRunner :: AppRunner (unsigned width,
                        unsigned height,
                        const std::string& wndName
  ) : _timeSvc(60.f)
    , _rndrSvc(this)
    , _window(nullptr)
    , _wndWidth(0)
    , _wndHeight(0)
{
    

    //startup
    if (glfwInit() != GLFW_TRUE)
        throw std::runtime_error("GLFW init failed");

    /* Create a windowed mode window and its OpenGL context */
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    _window = glfwCreateWindow(width, height, wndName.c_str(), NULL, NULL);
    if (!_window) {
        glfwTerminate();
        throw std::runtime_error("GLFW window creation failed");
    }

    SetupAlloyEnv();
    SetupImGui();
}

AppRunner::~AppRunner() {
    TearDownImGui();
    TearDownAlloyEnv();
    glfwTerminate();
}


ITimeService* AppRunner::GetTimeService() { return &_timeSvc; }
IRenderService* AppRunner::GetRenderService() { return &_rndrSvc; }

int AppRunner::Run(IApp* pUserApp) {
    const int MAX_FRAMESKIP = 5;

    const auto ticksPerFixedUpdate = _timeSvc.GetFixedUpdateDeltaTicks();

    auto prevFixedUpdateTick = _timeSvc.GetCurrentTick();
    int loops;
    float interpolation;

    while(!glfwWindowShouldClose(_window)) {
        /* Poll for and process events */
        glfwPollEvents();

        ResizeSwapChainIfNecessary();

        auto frameIdx = WaitForNextFrameResourceAvailable();
        auto& frameRes = _perFrameResources[frameIdx];

        auto preFence = frameRes.submissionFenceValue;

        //Submission fence = frame index + 1
        if(preFence) {
            pUserApp->OnFrameComplete(preFence - 1);
        }

        BeginFrame();
        pUserApp->OnFrameBegin(_fenceVal);
        
        //Logic update
        
        loops = 0;
        while( _timeSvc.GetCurrentTick() > prevFixedUpdateTick && loops < MAX_FRAMESKIP) {
            pUserApp->FixedUpdate();

            prevFixedUpdateTick += ticksPerFixedUpdate;
            loops++;
        }

        //interpolation = float( GetTickCount() + SKIP_TICKS - next_game_tick )
        //                / float( SKIP_TICKS );
        pUserApp->Update( /*interpolation*/ );

        pUserApp->OnDrawGui();
        
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();

        //Begin render


        auto& tgt = frameRes.msaaTarget;
        auto fb = _swapChain->GetBackBuffer();
        auto fbDesc = fb->GetDesc();

        auto cmdQ = _dev->GetGfxCommandQueue();
        auto commandList = cmdQ->CreateCommandList();
        {
            
            bool msaaEnabled = _msaaSampleCnt > alloy::SampleCount::x1;
            commandList->Begin();

            alloy::RenderPassAction passAction{};
            auto& ctAct = passAction.colorTargetActions.emplace_back();
            ctAct.loadAction = alloy::LoadAction::Clear;
            ctAct.storeAction = alloy::StoreAction::Store;
            ctAct.clearColor = {0.9, 0.1, 0.3, 1};
            if(msaaEnabled) {
                ctAct.target = tgt.color;
                ctAct.msaaResolveTarget = fbDesc.colorAttachments.front();
            } else {
                ctAct.target = fbDesc.colorAttachments.front();
            }

            auto& dtAct = passAction.depthTargetAction.emplace();
            dtAct.loadAction = alloy::LoadAction::Clear;
            dtAct.storeAction = alloy::StoreAction::DontCare;
            dtAct.clearDepth = 10.f;
            if(msaaEnabled) {
                dtAct.target = tgt.depthStencil;
                dtAct.msaaResolveTarget = fbDesc.depthAttachment;
                dtAct.msaaResolveMode = alloy::MSAADepthResolveMode::Min;
            } else {
                dtAct.target = fbDesc.depthAttachment;
            }

            auto& stAct = passAction.stencilTargetAction.emplace();
            stAct.loadAction = alloy::LoadAction::Load;
            stAct.storeAction = alloy::StoreAction::Store;
            stAct.target = tgt.depthStencil;
            stAct.clearStencil = 0xA;
            //stAct.msaaResolveTarget = fbDesc.depthAttachment;

            auto& pass = commandList->BeginRenderPass(passAction);


            pUserApp->OnRenderFrame(pass);

            ImGui_ImplAlloy_RenderDrawData(draw_data, pass);


            commandList->EndPass();
            commandList->End();
        }

        auto fenceVal = ++_fenceVal;
        
        cmdQ->SubmitCommand(commandList.get());
        cmdQ->EncodeSignalEvent(_submissionFence.get(), fenceVal);

        frameRes.submissionFenceValue = fenceVal;
        frameRes.submission = commandList;
        
        _dev->PresentToSwapChain(_swapChain.get());
    }

    return pUserApp->GetExitCode();
}

void AppRunner::SetupAlloyEnv() {
    
    /*******************************
     * Create device
     *******************************/
    
    alloy::IContext::Options ctxOpt{};
    ctxOpt.debug = true;

    auto ctx = alloy::IContext::Create(alloy::Backend::DX12, ctxOpt);
    auto adp = ctx->EnumerateAdapters().front();
    
    alloy::IGraphicsDevice::Options devOpt{};
    devOpt.debug = true;
    devOpt.preferStandardClipSpaceYDirection = true;

    _dev = adp->RequestDevice(devOpt);
    //dev = alloy::CreateVulkanGraphicsDevice(opt);
    //dev = alloy::CreateDX12GraphicsDevice(opt);

    auto& adpInfo = adp->GetAdapterInfo();

    std::cout << "Picked device:\n";
    std::cout << "    Vendor ID   : " << std::hex << adpInfo.vendorID << "\n";
    std::cout << "    Device ID   : " << std::hex << adpInfo.deviceID << "\n";
    std::cout << "    Name        : " <<             adpInfo.deviceName << "\n";
    std::cout << "    API version : " << std::dec << (std::string)(adpInfo.apiVersion) << std::endl;
    //std::cout << "    API version : " << std::dec << adpInfo.apiVersion.major;
    //                            std::cout << "." << adpInfo.apiVersion.minor;
    //                            std::cout << "." << adpInfo.apiVersion.subminor;
    //                            std::cout << "." << adpInfo.apiVersion.patch << std::endl;

    auto maxSampleCnt = adp->GetAdapterInfo().limits.maxMSAASampleCount;
    if(_msaaSampleCnt > maxSampleCnt) {
        _msaaSampleCnt = maxSampleCnt;
    }

    auto& factory = _dev->GetResourceFactory();
    

    /*******************************
     * Create swap chain
     *******************************/
    
#if defined(VLD_PLATFORM_WIN32)
    auto hWnd = glfwGetWin32Window(_window);
    auto hInst = GetModuleHandle(NULL);
    alloy::Win32SwapChainSource scSrc{ hWnd, nullptr };
#elif defined(VLD_PLATFORM_MACOS)
    //Get the NSWindow
    auto hWnd = glfwGetCocoaWindow(window);
    alloy::NSWindowSwapChainSource scSrc(hWnd);
#endif

    int w, h;
    glfwGetFramebufferSize(_window, &w, &h);

    _wndWidth = w, _wndHeight = h;

    alloy::ISwapChain::Description swapChainDesc{};
    swapChainDesc.source = &scSrc;
    swapChainDesc.initialWidth = w;
    swapChainDesc.initialHeight = h;
    swapChainDesc.depthFormat = alloy::PixelFormat::D24_UNorm_S8_UInt;
    swapChainDesc.backBufferCnt = _maxFramesInFlight;
    _swapChain = factory.CreateSwapChain(swapChainDesc);

    CreatePerFrameResources();

    _submissionFence = factory.CreateSyncEvent();
}

void AppRunner::SetupImGui() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
    ImGui_ImplGlfw_InitForOther(_window, true);
    ImGui_ImplAlloy_InitInfo rbInitInfo {
        .device = _dev,
        .msaaSampleCount = _msaaSampleCnt,
        .renderTargetFormat = alloy::PixelFormat::B8_G8_R8_A8_UNorm
    };

    ImGui_ImplAlloy_Init(rbInitInfo);
}

void AppRunner::TearDownImGui() {
    ImGui_ImplAlloy_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void AppRunner::TearDownAlloyEnv() {
    _dev->WaitForIdle();
}


void AppRunner::ReleasePerFrameResources() {
    for(auto& slot : _perFrameResources) {
        slot.msaaTarget = {};
        slot.submission = nullptr;
    }
}

void AppRunner::CreatePerFrameResources() {
    if(!_perFrameResources.empty()) {
        //Destroy old targets
        ReleasePerFrameResources();
        //_nextFrameIdx = 0;
    } else {
        _perFrameResources.resize(_maxFramesInFlight);
    }

    auto w = _swapChain->GetWidth();
    auto h = _swapChain->GetHeight();

    auto& factory = _dev->GetResourceFactory();
    for(auto i = 0; i < _maxFramesInFlight; ++i) {

        auto& slot = _perFrameResources[i];

        auto& tgt = slot.msaaTarget;

        alloy::ITexture::Description msaaTgtDesc {};
        msaaTgtDesc.type = alloy::ITexture::Description::Type::Texture2D;
        msaaTgtDesc.sampleCount = _msaaSampleCnt;
        msaaTgtDesc.width = w;
        msaaTgtDesc.height = h;
        msaaTgtDesc.depth = 1;
        msaaTgtDesc.mipLevels = 1;
        msaaTgtDesc.arrayLayers = 1;
        msaaTgtDesc.format = alloy::PixelFormat::B8_G8_R8_A8_UNorm;
        msaaTgtDesc.usage.renderTarget = 1;
        {
            auto tex = factory.CreateTexture(msaaTgtDesc);
            auto view = factory.CreateTextureView(tex);
            tgt.color = factory.CreateRenderTarget(view);

            tex->SetDebugName(std::format("MSAAColorTgt_frame{}", i));
        }

        msaaTgtDesc.format = alloy::PixelFormat::D24_UNorm_S8_UInt;
        msaaTgtDesc.usage.renderTarget = 0;
        msaaTgtDesc.usage.depthStencil = 1;
        {
            auto tex = factory.CreateTexture(msaaTgtDesc);
            auto view = factory.CreateTextureView(tex);
            tgt.depthStencil = factory.CreateRenderTarget(view);

            tex->SetDebugName(std::format("MSAADSTgt_frame{}", i));
        }
    }
}

uint32_t AppRunner::WaitForNextFrameResourceAvailable() {

    auto& nextFrameResSlot = _perFrameResources[_nextFrameIdx];

    if(nextFrameResSlot.submissionFenceValue != 0) {
        auto expVal = nextFrameResSlot.submissionFenceValue;
        auto lastCompletedFenceVal = GetLastCompletedCommandIndex();

        if(lastCompletedFenceVal < expVal) {
            WaitForCommandComplete(expVal);
        }
    }

    //nextFrameResSlot.submission = nullptr;
    //nextFrameResSlot.submissionFenceValue = 0;

    auto retVal =  _nextFrameIdx;

    _nextFrameIdx++;
    if(_nextFrameIdx >= _maxFramesInFlight) 
        _nextFrameIdx = 0;

    return retVal;
}

void AppRunner::ResizeSwapChainIfNecessary() {
    int currWidth, currHeight;
    glfwGetWindowSize(_window, &currWidth, &currHeight);
    if( (currWidth != _wndWidth) || (currHeight != _wndHeight) ) {
        while (currWidth == 0 || currHeight == 0) {
            glfwGetWindowSize(_window, &currWidth, &currHeight);
            glfwWaitEvents();
        }
        _dev->WaitForIdle();
        _swapChain->Resize(currWidth, currHeight);
        _wndWidth = currWidth;
        _wndHeight = currHeight;

        CreatePerFrameResources();
    }
}

uint32_t AppRunner::GetLastCompletedCommandIndex() const { 
    return _submissionFence->GetSignaledValue();
}

void AppRunner::WaitForCommandComplete(uint32_t value) {
    _submissionFence->WaitFromCPU(value);
}


void AppRunner::BeginFrame() {
    auto now = _timeSvc.BeginNewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui_ImplAlloy_NewFrame();
}

void AppRunner::UnlockAndShowCursor() {
    glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetInputMode(_window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
}

void AppRunner::LockAndHideCursor() {
    glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    //Enable raw input to receive mosue movement after cursor lock
    glfwSetInputMode(_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
}

IAppRunner* IAppRunner::Create(unsigned width, unsigned height, const std::string& wndName) {
    return new AppRunner(width, height, wndName);
}

