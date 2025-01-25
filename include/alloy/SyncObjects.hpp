#pragma once


#include "alloy/common/Macros.h"
#include "alloy/common/RefCnt.hpp"

#include <cstdint>
#include <limits>

namespace alloy{

    //For cross render pass within same command buffer synchornizatons
    class IFence : public common::RefCntBase{

    public:

    };

    //For cross device / cross command buffer synchornizatons
    class IEvent : public common::RefCntBase{

    public:
        virtual uint64_t GetSignaledValue() = 0;

        virtual void SignalFromCPU(uint64_t signalValue) = 0;
        virtual bool WaitFromCPU(uint64_t expectedValue, uint32_t timeoutMs) = 0;
        bool WaitFromCPU(uint64_t expectedValue) {
            return WaitFromCPU(expectedValue, (std::numeric_limits<std::uint32_t>::max)());
        }

    };

}
