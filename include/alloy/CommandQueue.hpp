#pragma once

#include "alloy/common/Macros.h"
#include "alloy/CommandList.hpp"

namespace alloy{

    class IGraphicsDevice;
    class ICommandList;
    class IEvent;

    class ICommandQueue {


    public:
        virtual ~ICommandQueue() = default;

        //virtual bool WaitForSignal(std::uint64_t timeoutNs) = 0;
        //bool WaitForSignal() {
        //    return WaitForSignal((std::numeric_limits<std::uint64_t>::max)());
        //}
        //virtual bool IsSignaled() const = 0;

        //virtual void Reset() = 0;

        virtual void EncodeSignalEvent(IEvent* fence, uint64_t value) = 0;
        virtual void EncodeWaitForEvent(IEvent* fence, uint64_t value) = 0;

        virtual void SubmitCommand(ICommandList* cmd) = 0;

        ///#TODO: Add command buffer creation to here. Both Metal and Vulkan demands
        //command buffer are allocated from and submitted to the same queue.
        
        virtual common::sp<ICommandList> CreateCommandList() = 0;

        virtual void* GetNativeHandle() const = 0;
    };

}
