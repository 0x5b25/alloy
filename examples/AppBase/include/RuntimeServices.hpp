#pragma once
#include <alloy/alloy.hpp>

template<typename T>
using alloy_sp = alloy::common::sp<T>;

class ITimeService {

public:
    virtual float GetElapsedSeconds() = 0;
    virtual float GetDeltaSeconds() = 0;
    virtual float GetFixedUpdateDeltaSeconds() = 0;

};


class IRenderService {

public:
    virtual alloy_sp<alloy::IGraphicsDevice> GetDevice() = 0;
    virtual uint32_t GetCurrentFrameIndex() = 0;

    virtual void GetFrameBufferSize(uint32_t& width, uint32_t& height) = 0;
    virtual alloy::PixelFormat GetFrameBufferColorFormat() = 0;
    virtual alloy::PixelFormat GetFrameBufferDepthStencilFormat() = 0;
    virtual alloy::SampleCount GetFrameBufferSampleCount() = 0;
};


class IRuntimeService {

public:
    virtual ITimeService* GetTimeService() = 0;
    virtual IRenderService* GetRenderService() = 0;

    virtual void* GetWindowHandle() = 0;
    virtual void LockAndHideCursor() = 0;
    virtual void UnlockAndShowCursor() = 0;
};
