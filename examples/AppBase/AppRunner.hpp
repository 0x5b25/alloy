
#include "IApp.hpp"
#include "RuntimeServices.hpp"

#include <chrono>
#include <GLFW/glfw3.h>

class AppRunner;

class TimeServiceImpl : public ITimeService {

    using Clock = std::chrono::steady_clock;

    uint64_t _ticksPerSec;
    uint64_t _ticksPerFixedUpdate;

    Clock::time_point _startTime;

    uint64_t _deltaTicks;
    Clock::time_point _frameStartTime;


public:
    TimeServiceImpl(float fixedUpdatePerSecond);

    virtual float GetElapsedSeconds() override;
    virtual float GetDeltaSeconds() override;
    virtual float GetFixedUpdateDeltaSeconds() override;

    std::uint64_t GetDeltaTicks() const { return _deltaTicks; }
    std::uint64_t GetFixedUpdateDeltaTicks() const { return _ticksPerFixedUpdate; }

    std::uint64_t GetCurrentTick() const;

    std::uint64_t BeginNewFrame();
};

class RenderServiceImpl : public IRenderService {
    AppRunner* _runner;

public:
    RenderServiceImpl(AppRunner* runner) : _runner(runner) {}

    virtual alloy_sp<alloy::IGraphicsDevice> GetDevice() override;
    virtual uint32_t GetCurrentFrameIndex() override;

    
    virtual void GetFrameBufferSize(uint32_t& width, uint32_t& height) override;
    
    virtual alloy::PixelFormat GetFrameBufferColorFormat() override;
    virtual alloy::PixelFormat GetFrameBufferDepthStencilFormat() override;
    virtual alloy::SampleCount GetFrameBufferSampleCount() override;
};

class AppRunner : public IAppRunner {

    friend class RenderServiceImpl;

    alloy::common::sp<alloy::IGraphicsDevice> _dev;
    alloy::common::sp<alloy::ISwapChain> _swapChain;

    //IApp *_pUserApp;

    alloy::SampleCount _msaaSampleCnt = alloy::SampleCount::x16;
    TimeServiceImpl _timeSvc;
    RenderServiceImpl _rndrSvc;

    GLFWwindow* _window;
    uint32_t _wndWidth, _wndHeight;

    struct MsaaRenderTarget {
        alloy_sp<alloy::IRenderTarget> color, depthStencil;
    } _msaaTarget;

    alloy_sp<alloy::ICommandList> _submission;
    uint32_t _fenceVal = 0;
    alloy_sp<alloy::IEvent> _submissionFence;

private:
    
    void SetupAlloyEnv();
    void SetupImGui();
    void TearDownImGui();
    void TearDownAlloyEnv();

    void CreatePerFrameResources();
    void ReleasePerFrameResources();

    uint32_t GetLastCompletedCommandIndex() const;
    void WaitForCommandComplete(uint32_t value);

    void BeginFrame();
    void ResizeSwapChainIfNecessary();

public:

    AppRunner(unsigned width, unsigned height, const std::string& wndName);
    ~AppRunner();

    
    virtual ITimeService* GetTimeService() override;
    virtual IRenderService* GetRenderService() override;
    
    virtual void* GetWindowHandle() override {return _window;}
    virtual void LockAndHideCursor() override;
    virtual void UnlockAndShowCursor() override;

    virtual int Run(IApp* pUserApp) override;

private:

};
