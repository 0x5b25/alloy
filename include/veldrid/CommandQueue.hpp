#pragma once

#include "veldrid/common/Macros.h"
#include "veldrid/DeviceResource.hpp"
#include "veldrid/CommandList.hpp"

namespace Veldrid{

    class GraphicsDevice;
    class CommandList;
    class Fence;

    class CommandQueue {


    public:
        virtual ~CommandQueue() = default;

        //virtual bool WaitForSignal(std::uint64_t timeoutNs) = 0;
        //bool WaitForSignal() {
        //    return WaitForSignal((std::numeric_limits<std::uint64_t>::max)());
        //}
        //virtual bool IsSignaled() const = 0;

        //virtual void Reset() = 0;

        virtual void EncodeSignalFence(Fence* fence, uint64_t value) = 0;
        virtual void EncodeWaitForFence(Fence* fence, uint64_t value) = 0;

        virtual void SubmitCommand(CommandList* cmd) = 0;

        //#TODO: Add command buffer creation to here. Both Metal and Vulkan demands
        //command buffer are allocated from and submitted to the same queue.
        
        virtual sp<CommandList> CreateCommandList() = 0;
    };

}
