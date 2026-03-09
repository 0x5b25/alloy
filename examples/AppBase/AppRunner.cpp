#include "AppRunner.hpp"

#include <cmath>
#include <limits>

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
    _pUserApp = IApp::Create(this);
}

AppBase::~AppBase() {

    delete _pUserApp;
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
