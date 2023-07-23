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

        virtual bool WaitForSignal(std::uint64_t timeoutNs) = 0;
        bool WaitForSignal() {
            return WaitForSignal(std::numeric_limits<std::uint64_t>::max());
        }
        virtual bool IsSignaled() const = 0;

        virtual void Reset() = 0;

    };

    class Semaphore : public DeviceResource {

    protected:

        Semaphore(const sp<GraphicsDevice>& dev)
            : DeviceResource(dev) {}

    public:
        virtual ~Semaphore() = default;

    };

}
