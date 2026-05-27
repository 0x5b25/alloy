#pragma once

#include "alloy/GraphicsDevice.hpp"
#include "alloy/CommandQueue.hpp"
#include "alloy/SyncObjects.hpp"

#include "ITrackingCommandList.hpp"

namespace alloy {
    
    class ITrackingCommandQueue;

    class ITrackingDevice : public common::RefCntBase{
    public:

        static common::sp<ITrackingDevice> Make(const common::sp<IGraphicsDevice>& dev);

        virtual alloy::ResourceFactory& GetResourceFactory() = 0;
        //virtual void SubmitCommand(const CommandList* cmd) = 0;
        virtual ISwapChain::State PresentToSwapChain(
            ISwapChain* sc) = 0;

        virtual ITrackingCommandQueue* GetGfxCommandQueue() = 0;
        virtual ITrackingCommandQueue* GetCopyCommandQueue() = 0;
               
        virtual void WaitForIdle() = 0;

    };

    // Special signal-able event, owned by a timeline.
    // Used for sync resource usage across timelines.
    // Auto incremented after SubmitCommand()
    class ITrackingEvent {

    public:
        virtual uint64_t GetSignaledValue() const = 0;

        virtual bool WaitFromCPU(uint64_t expectedValue, uint32_t timeoutMs) = 0;
        bool WaitFromCPU(uint64_t expectedValue) {
            return WaitFromCPU(expectedValue, (std::numeric_limits<std::uint32_t>::max)());
        }

    };

    class ITrackingCommandQueue {

    public:

        virtual ITrackingEvent& GetTrackingEvent() = 0;

        // Also sync resource usage across timelines.
        virtual void EncodeWaitForTrackingEvent(ITrackingEvent* fence, uint64_t value) = 0;

        // Can still use IEvent, no resource usage syncing.
        virtual void EncodeSignalEvent(IEvent* fence, uint64_t value) = 0;
        virtual void EncodeWaitForEvent(IEvent* fence, uint64_t value) = 0;

        virtual uint64_t SubmitCommand(ITrackingCommandList* cmd) = 0;

        ///#TODO: Add command buffer creation to here. Both Metal and Vulkan demands
        //command buffer are allocated from and submitted to the same queue.
        
        virtual common::sp<ITrackingCommandList> CreateCommandList() = 0;


    };

} // namespace alloy

