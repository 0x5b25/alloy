#pragma once

#include "RuntimeServices.hpp"

#include <alloy/alloy.hpp>

class IApp {

public:
    IApp() {}

    virtual ~IApp() {}

    virtual bool AppShouldExit() = 0;
    virtual int GetExitCode() = 0;

    virtual void FixedUpdate() = 0;
    virtual void Update() = 0;

    virtual void OnDrawGui() = 0;
    virtual void OnRenderFrame(alloy::IRenderCommandEncoder& renderPass) = 0;

    static IApp* Create(IRuntimeService* pRts);

};
