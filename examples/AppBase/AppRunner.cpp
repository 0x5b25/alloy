#include "AppRunner.hpp"

#include <cmath>
#include <limits>
#include <iostream>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include "Utility/ImGuiAlloyBackend.hpp"

TimeServiceImpl::TimeServiceImpl(float fixedUpdatePerSecond) 
    : _ticksPerSec(Clock::period::den)
    , _ticksPerFixedUpdate(std::round((double)_ticksPerSec / fixedUpdatePerSecond))
    , _startTime(Clock::now())
{ }


std::uint64_t TimeServiceImpl::GetCurrentTick() const {
    auto now = Clock::now();
    auto delta = now - _startTime;
    return delta.count();
}


float TimeServiceImpl::GetElapsedSeconds() {

}

float TimeServiceImpl::GetDeltaSeconds() {

}

float TimeServiceImpl::GetFixedUpdateDeltaSeconds() {
    return _ticksPerFixedUpdate / (float)_ticksPerSec;
}

std::uint64_t TimeServiceImpl::BeginNewFrame() {

}

AppBase :: AppBase ()
    : _timeSvc(60.f)
{
    SetupAlloyEnv(swapChainSrc);
    SetupImGui();
    _pUserApp = IApp::Create(this);
}

AppBase::~AppBase() {

    delete _pUserApp;
    TearDownImGui();
    TearDownAlloyEnv();
}

int AppBase::Run() {
    const int MAX_FRAMESKIP = 5;

    const auto ticksPerFixedUpdate = _timeSvc.GetFixedUpdateDeltaTicks();

    auto prevFixedUpdateTick = _timeSvc.GetCurrentTick();
    int loops;
    float interpolation;

    bool shouldExit = false;
    while( !shouldExit ) {

        auto now = _timeSvc.BeginNewFrame();


        loops = 0;
        while( _timeSvc.GetCurrentTick() > prevFixedUpdateTick && loops < MAX_FRAMESKIP) {
            _pUserApp->FixedUpdate();

            prevFixedUpdateTick += ticksPerFixedUpdate;
            loops++;
        }

        //interpolation = float( GetTickCount() + SKIP_TICKS - next_game_tick )
        //                / float( SKIP_TICKS );
        _pUserApp->Update( /*interpolation*/ );

        shouldExit = _pUserApp->AppShouldExit();
    }

    return _pUserApp->GetExitCode();
}

void AppBase::SetupAlloyEnv(alloy::SwapChainSource* swapChainSrc) {
    auto ctx = alloy::IContext::Create(alloy::Backend::DX12);
    auto adp = ctx->EnumerateAdapters().front();
    
    alloy::IGraphicsDevice::Options opt{};
    opt.debug = true;
    opt.preferStandardClipSpaceYDirection = true;

    _dev = adp->RequestDevice(opt);
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

    alloy::ISwapChain::Description swapChainDesc{};
    swapChainDesc.source = swapChainSrc;
    swapChainDesc.initialWidth = initialWidth;
    swapChainDesc.initialHeight = initialHeight;
    swapChainDesc.depthFormat = alloy::PixelFormat::D24_UNorm_S8_UInt;
    swapChainDesc.backBufferCnt = _initSubmissionCnt;
    swapChain = dev->GetResourceFactory().CreateSwapChain(swapChainDesc);

    renderFinishFence = dev->GetResourceFactory().CreateSyncEvent();
}

void AppBase::SetupImGui() {
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

}

void AppBase::TearDownImGui() {
    ImGui_ImplAlloy_Shutdown();
    ImGui::DestroyContext();
}

void AppBase::TearDownAlloyEnv() {
    _dev->WaitForIdle();
}
