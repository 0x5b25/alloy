#pragma once

#include <alloy/alloy.hpp>

//define USE_PIX

void SetupCaptureEvent(alloy::common::sp<alloy::IGraphicsDevice> gd);
void StartCapture();
void StopCapture();
