#pragma once


#include "veldrid/common/Macros.h"
#include "veldrid/DeviceResource.hpp"

#include <cstdint>
#include <limits>

namespace Veldrid{

    class Fence : public DeviceResource{

    protected:

        Fence(const sp<GraphicsDevice>& dev) 
            : DeviceResource(dev) {}

    public:
        virtual ~Fence() = default;

        //virtual bool WaitForSignal(std::uint64_t timeoutNs) = 0;
        //bool WaitForSignal() {
        //    return WaitForSignal((std::numeric_limits<std::uint64_t>::max)());
        //}
        //virtual bool IsSignaled() const = 0;

        //virtual void Reset() = 0;

        virtual uint64_t GetCurrentValue() = 0;
        virtual void SignalFromCPU(uint64_t signalValue) = 0;
        virtual bool WaitFromCPU(uint64_t expectedValue, uint32_t timeoutMs) = 0;
        bool WaitFromCPU(uint64_t expectedValue) {
            return WaitFromCPU(expectedValue, (std::numeric_limits<std::uint32_t>::max)());
        }

    };

    class Semaphore : public DeviceResource {

    protected:

        Semaphore(const sp<GraphicsDevice>& dev)
            : DeviceResource(dev) {}

    public:
        virtual ~Semaphore() = default;

    };

}
