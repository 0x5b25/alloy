#pragma once

#include "RuntimeServices.hpp"

#include <alloy/alloy.hpp>

class IApp {

public:
    IApp() {}

    virtual ~IApp() {}
    virtual int GetExitCode() = 0;

    virtual void FixedUpdate() = 0;
    virtual void Update() = 0;

    virtual void OnDrawGui() = 0;
    virtual void OnRenderFrame(alloy::IRenderCommandEncoder& renderPass) = 0;

    virtual void OnFrameComplete(uint32_t frameIdx) {}
    virtual void OnFrameBegin(uint32_t frameIdx) {}

};

class IAppRunner : public IRuntimeService{

public:
    IAppRunner() {}
    virtual ~IAppRunner() {}


    virtual int Run(IApp* pUserApp) = 0;
    static IAppRunner* Create(unsigned width, unsigned height, const std::string& wndName); 

};
