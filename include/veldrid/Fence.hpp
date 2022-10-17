#pragma once


#include "veldrid/common/Macros.h"
#include "veldrid/DeviceResource.hpp"

#include <cstdint>

namespace Veldrid{

    class Fence : public DeviceResource{

    protected:

        Fence(sp<GraphicsDevice>&& dev) 
            : DeviceResource(std::move(dev)) {}

    public:
        virtual ~Fence() = default;

        virtual bool IsSignaled() const = 0;

        virtual void Reset() = 0;

    };

}
