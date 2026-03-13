
#include "IApp.hpp"
#include "RuntimeServices.hpp"

#include <chrono>

class TimeServiceImpl : public ITimeService {

    using Clock = std::chrono::steady_clock;

    uint64_t _ticksPerSec;
    uint64_t _ticksPerFixedUpdate;

    Clock::time_point _startTime;


public:
    TimeServiceImpl(float fixedUpdatePerSecond);

    virtual float GetElapsedSeconds() override;
    virtual float GetDeltaSeconds() override;
    virtual float GetFixedUpdateDeltaSeconds() override;

    std::uint64_t GetDeltaTicks() const;
    std::uint64_t GetFixedUpdateDeltaTicks() const { return _ticksPerFixedUpdate; }

    std::uint64_t GetCurrentTick() const;

    std::uint64_t BeginNewFrame();
};

class AppBase : public IRuntimeService {

    alloy::common::sp<alloy::IGraphicsDevice> _dev;

    IApp *_pUserApp;

    TimeServiceImpl _timeSvc;

private:
    
    void SetupAlloyEnv(alloy::SwapChainSource* swapChainSrc);
    void SetupImGui();
    void TearDownImGui();
    void TearDownAlloyEnv();

public:

    AppBase();
    ~AppBase();

    int Run();

private:

};
