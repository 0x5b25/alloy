#pragma once

#include "alloy/Texture.hpp"
#include "alloy/Buffer.hpp"
#include "alloy/CommandQueue.hpp"
#include "alloy/CommandList.hpp"
#include "alloy/ResourceBarrier.hpp"
#include "alloy/SyncObjects.hpp"

#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <map>
#include <functional>

namespace alloy {
    class IBuffer;
    class ITexture;
}

namespace alloy::utils
{

    class ITrackedBuffer;
    class ITrackedTexture;

    struct BufferState {
        PipelineStage stage;
        ResourceAccess access;

        bool operator==(const BufferState& other) const {
            return stage == other.stage && access == other.access;
        }
    };

    struct TextureState {
        PipelineStage stage;
        ResourceAccess access;
        TextureLayout layout;

        bool operator==(const TextureState& other) const {
            return stage == other.stage &&
                   access == other.access && 
                   layout == other.layout;
        }
    };

    struct ResourceStates {
        std::unordered_map<ITrackedBuffer*, BufferState> buffers;
        std::unordered_map<ITrackedTexture*, TextureState> textures;

        void SyncTo(const ResourceStates& other) {
            for(auto& [k, v] : other.buffers)
                buffers.insert_or_assign(k, v);
            for(auto& [k, v] : other.textures)
                textures.insert_or_assign(k, v);
        }

        void Clear() {
            buffers.clear();
            textures.clear();
        }
    };

    

    class ITrackerEvent;
    
    class ITimeline {

    public:
        virtual ~ITimeline() {}

        //virtual void RegisterSyncPoint(
        //    common::sp<ITrackerEvent> syncEvent,
        //    uint64_t syncValue) = 0;

        //virtual void SyncToTrackerEvent(
        //    ITrackerEvent* syncEvent,
        //    uint64_t syncValue);
        //
        //virtual void NotifySyncResolved(
        //    ITrackerEvent* syncEvent,
        //    uint64_t value,
        //    const ResourceStates& states
        //) = 0;

        virtual void RemoveResource(ITrackedBuffer* buffer) = 0;
        virtual void RemoveResource(ITrackedTexture* texture) = 0;

        virtual ResourceStates& GetCurrentState() = 0;

        //Notify that all resources are synced
        // DX12 will sync all resources at submission end
        // Vulkan will sync all resources at semaphore signal
        virtual void NotifySyncResources() = 0;

    };


    class CPUTimeline : public ITimeline {
        
        ResourceStates _currentState;
    public:

        ResourceStates& GetCurrentState() override { return _currentState; }

        void RegisterBufferState(const BufferState& state, ITrackedBuffer* buffer) {
            _currentState.buffers[buffer] = state;
        }
        void RegisterTextureState(const TextureState& request, ITrackedTexture* texture) {
            _currentState.textures[texture] = request;
        }

        void RemoveResource(ITrackedBuffer* buffer) override { _currentState.buffers.erase(buffer); }
        void RemoveResource(ITrackedTexture* texture) override { _currentState.textures.erase(texture); }

        void NotifySyncResources() override {
            for(auto&[k, v] : _currentState.buffers) {
                v.access = ResourceAccess::None;
                v.stage = PipelineStage::None;
            }

            for(auto&[k, v] : _currentState.textures) {
                v.access = ResourceAccess::None;
                v.stage = PipelineStage::None;
            }
        }

    };

    
    struct BufferBarrierAction {
        BufferState before, after;
        ITrackedBuffer* buffer;
    };
    struct TextureBarrierAction {
        TextureState before, after;
        ITrackedTexture* texture;
    };

    
    struct BarrierActions {
        std::vector<BufferBarrierAction> bufferBarrierActions;
        std::vector<TextureBarrierAction> textureBarrierActions;
    };


    BarrierActions CalculateBarriersBetweenStates(
        const ResourceStates& currLocalState,
        const ResourceStates& reqestedState,
        std::function<BufferState(ITrackedBuffer*)> fnGetNotFoundBufferState,
        std::function<TextureState(ITrackedTexture*)> fnGetNotFoundTextureState
    );
    
    class GPUTimeline : public ITimeline {
    public:
        
        struct SyncPoint {
            ResourceStates states;
            common::sp<ITrackerEvent> syncEvent;
            uint64_t syncValue;
        };

        

    protected:
        //std::deque<SyncPoint> _syncPoints;

        ResourceStates _currentState;

        CPUTimeline& _cpuTimeline;

        //void _CleanupSyncPoints() {
        //    while(!_syncPoints.empty()){
        //        auto& point = _syncPoints.front();
        //        auto signaled = point.syncEvent->GetSignaledValue();
        //        if(signaled >= point.syncValue) {
        //            _syncPoints.pop_front();
        //        }
        //        else break;
        //    }
        //}

        virtual void InsertBarriers(
            const BarrierActions& barriers) = 0;

    public:
        GPUTimeline(
            CPUTimeline& cpuTimeline
        ) 
            : _cpuTimeline(cpuTimeline)
        { }

        virtual ~GPUTimeline() {}

        virtual void RemoveResource(ITrackedBuffer* buffer) override {
            //_CleanupSyncPoints();
            _currentState.buffers.erase(buffer);
            //for(auto& pt : _syncPoints) {
            //    pt.states.buffers.erase(buffer);
            //}
        }
        
        virtual void RemoveResource(ITrackedTexture* texture) override {
            //_CleanupSyncPoints();
            _currentState.textures.erase(texture);
            //for(auto& pt : _syncPoints) {
            //    pt.states.textures.erase(texture);
            //}
        }

        //virtual void RegisterSyncPoint(
        //    common::sp<ITrackerEvent> syncEvent,
        //    uint64_t syncValue) override;

        //virtual void NotifySyncResolved(
        //    ITrackerEvent* syncEvent,
        //    const ResourceStates& states
        //) override;

        void RequestResourceStatesBeforeSubmit(
            const ResourceStates& states);

        virtual void MarkResourceStatesAfterSubmit(
            const ResourceStates& states);

        ResourceStates& GetCurrentState() override { return _currentState; }

        void NotifySyncResources() override {
            for(auto&[k, v] : _currentState.buffers) {
                v.access = ResourceAccess::None;
                v.stage = PipelineStage::None;
            }

            for(auto&[k, v] : _currentState.textures) {
                v.access = ResourceAccess::None;
                v.stage = PipelineStage::None;
            }
        }
    };


    class ITrackedTexture : public ITexture {
    protected:
        std::unordered_set<ITimeline*> timelines;

        
        ITrackedTexture( const ITexture::Description& desc ) : ITexture(desc) { };

    public:
        void RegisterTimeline(ITimeline* timeline) { 
            timelines.insert(timeline); 
        }
        virtual ~ITrackedTexture() override {
            for(auto& timeline : timelines)
                timeline->RemoveResource(this);
        }
    };

    class ITrackedBuffer : public IBuffer {
    protected:
        std::unordered_set<ITimeline*> timelines;

        
        ITrackedBuffer( const IBuffer::Description& desc ) : IBuffer(desc) { };
    public:
        void RegisterTimeline(ITimeline* timeline) { 
            timelines.insert(timeline); 
        }
        virtual ~ITrackedBuffer() override {
            for(auto& timeline : timelines)
                timeline->RemoveResource(this);
        }
    };


    class ITrackerCmdQ : public ICommandQueue, public GPUTimeline {

    protected:
    public:
        ITrackerCmdQ( CPUTimeline& cpuTimeline) : GPUTimeline(cpuTimeline){};
        //virtual ~ITrackerCmdQ() override;

        virtual void EncodeSignalEvent(IEvent* fence, uint64_t value) override;
        virtual void EncodeWaitForEvent(IEvent* fence, uint64_t value) override;

        virtual void SubmitCommand(ICommandList* cmd) override;

    };
    
    class ITrackerCmdList : public ICommandList{

    public:
        virtual const ResourceStates& GetRequestedResourceStates() = 0;
        virtual const ResourceStates& GetFinalResourceStates() = 0;
        
    };
    
    //Strict requirements for auto state tracking:
    // 1. monotonically increasing value
    // 2. no wait-before-signal
    // 3. no strict ordering dependency of states,
    //    i.e. state updates will be fetched from any fence value
    //    that >= expected value.
    class ITrackerEvent : public IEvent {


    public:

    protected:
        ITimeline* _currTimeline;

        //map for each timeline : last signaled value
        std::unordered_map<ITimeline*, uint64_t> _signalingTimelines;

    public:
        virtual void RegisterSyncPoint(ITimeline* timeline, uint64_t syncValue);

        virtual void SyncTimelineToThis(ITimeline* timeline, uint64_t syncValue);

    };

} // namespace alloy::utils
