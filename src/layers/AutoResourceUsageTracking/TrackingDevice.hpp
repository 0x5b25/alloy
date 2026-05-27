#pragma once

#include "alloy/layers/AutoResourceUsageTracking/ITrackingDevice.hpp"

#include "alloy/SyncObjects.hpp"
#include "alloy/CommandQueue.hpp"
#include "alloy/GraphicsDevice.hpp"

#include "TrackingTimeline.hpp"
#include "TrackingResourceFactory.hpp"

#include <queue>

namespace alloy::layers::AutoResourceUsageTracking {

    class TrackingCommandList;

    
    class TrackingEvent : public ITrackingEvent {
        GPUTimeline* _timeline;

        common::sp<IEvent> _inner;

    public:
        TrackingEvent(GPUTimeline* timeline,
                      common::sp<IEvent> event
        ) : _timeline(timeline)
          , _inner(event)
        { }

        virtual uint64_t GetSignaledValue() const override {return _inner->GetSignaledValue();}

        virtual bool WaitFromCPU(uint64_t expectedValue, uint32_t timeoutMs) override {
            return _inner->WaitFromCPU(expectedValue, timeoutMs);
        }

        IEvent* GetInner() const { return _inner.get(); }

        GPUTimeline* GetTimeline() const { return _timeline; }
    };

    
    class TrackingCommandQueue : public ITrackingCommandQueue
                               , public GPUTimeline 
    {
        TrackingDevice* _dev;
        TrackingEvent _trackingEvt;
        ICommandQueue* _inner;

        
        //Resource transition cmd pools
        struct CmdPoolInUse {
            common::sp<ICommandList> cmdList;
            uint64_t lastSubmittedFence;
        };
    
        std::queue<CmdPoolInUse> _cmdListsInUse;

        void _RecycleTransitionCmdBufs();
        common::sp<ICommandList> _GetOneTransitionCmdList();

        void _GetFinishedSubmissions();

        void _TransitResourceStatesBeforeSubmit(const TrackingCommandList& cmdList);
        void _MarkResourceStatesAfterSubmit(const TrackingCommandList& cmdList);

    public:
        TrackingCommandQueue(
            TrackingDevice* dev,
            common::sp<IEvent> event,
            ICommandQueue* queue
        )
            : _dev(dev)
            , _trackingEvt(this, std::move(event))
            , _inner(queue)
        { }

        TrackingDevice* GetTrackingDevice() const { return _dev; }
        virtual ITrackingEvent& GetTrackingEvent() override { return _trackingEvt; }

        // Also sync resource usage across timelines.
        virtual void EncodeWaitForTrackingEvent(ITrackingEvent* fence, uint64_t value) override;

        // Can still use IEvent, no resource usage syncing.
        virtual void EncodeSignalEvent(IEvent* fence, uint64_t value) override {
            _inner->EncodeSignalEvent(fence, value);
        }
        virtual void EncodeWaitForEvent(IEvent* fence, uint64_t value) override {
            _inner->EncodeWaitForEvent(fence, value);
        }

        virtual uint64_t SubmitCommand(ITrackingCommandList* cmd) override;

        ///#TODO: Add command buffer creation to here. Both Metal and Vulkan demands
        //command buffer are allocated from and submitted to the same queue.
        
        virtual common::sp<ITrackingCommandList> CreateCommandList()  override;

        void PrepareTextureForPresent(TrackedTexView* tex);

    };

    class TrackingDevice : public ITrackingDevice
                         , public TrackingResourceFactory<TrackingDevice>
                         , public CPUTimeline
    {
        common::sp<IGraphicsDevice> _inner;

        
        TrackingCommandQueue* _gfxQ;
        TrackingCommandQueue* _copyQ;
    public:
        TrackingDevice(common::sp<IGraphicsDevice> dev);
        ~TrackingDevice();
        
        IGraphicsDevice* GetInner() { return _inner.get(); }

        virtual ResourceFactory& GetResourceFactory() override { return *this; };

        virtual ISwapChain::State PresentToSwapChain(ISwapChain* sc) override;

        virtual ITrackingCommandQueue* GetGfxCommandQueue() override { return _gfxQ; }
        virtual ITrackingCommandQueue* GetCopyCommandQueue() override { return _copyQ; }
               
        virtual void WaitForIdle() override {
            _inner->WaitForIdle();
        }

        common::sp<ITexture> CreateTrackedTexture(const ITexture::Description& desc);

        
        IGraphicsDevice* GetInner() const { return _inner.get(); }
        
        // We don't need cross submission buffer state for now.
        // This function remains dormant
        //common::sp<IBuffer> CreateTrackedBuffer(const ITexture::Description& desc);
    };






}

