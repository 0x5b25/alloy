#pragma once

#include <unordered_map>
#include <deque>

#include "alloy/ResourceBarrier.hpp"

#include "TrackedResource.hpp"

namespace alloy::layers::AutoResourceUsageTracking
{
    
    class TextureLayoutLog {

        struct Pair {
            uint64_t gen;
            TextureLayout layout;
        };

        std::deque<Pair> log;

        // Indicates the oldest state 
        bool logTrimmed = false;

    public:
        void Add(uint64_t gen, TextureLayout layout);

        //Get the resource state up to given generation
        TextureLayout Get(uint64_t gen) const;

        TextureLayout GetLatest() const;

        // Trim the log up to given generation.
        // Normally means all commands from older generations are
        // confirmed completed on GPU.
        void Trim(uint64_t gen);

    };

    class CPUTimeline;

    class GPUTimeline {

        std::unordered_map<WeakTrackedRef<ITextureView>, TextureLayoutLog> _textures;

        void _CheckAndClearInvalidRefs();

        void _Trim(uint64_t gen);

        void _Clear() {
            _textures.clear();
        }

        uint64_t _lastSubmittedFence = 0;
        uint64_t _lastFinishedFence = 0;

    public:

        // Only buffers are tracked in timeline due to their
        // layouts presist across command lists. Buffers are
        // always in their initial state and don't need explicit
        // tracking


        virtual ~GPUTimeline() {}

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

        TextureLayout GetResourceState(WeakTrackedRef<ITextureView> resource,
                                               uint64_t atFenceValue
        ) const {
            auto it = _textures.find(resource);
            if(it == _textures.end()) return TextureLayout::Undefined;
            return it->second.Get(atFenceValue);
        }

        TextureLayout GetLatestResourceState(
            WeakTrackedRef<ITextureView> resource
        ) {
            auto it = _textures.find(resource);
            if(it == _textures.end()) return TextureLayout::Undefined;
            return it->second.GetLatest();
        }

        bool ContainsResource(WeakTrackedRef<ITextureView> resource) const {
            return _textures.find(resource) != _textures.end();
        }

        void RegisterTextureState(WeakTrackedRef<ITextureView> ref, 
                                    TextureLayout layout);

        //Notify that all resources are synced up to fenceValue
        // DX12 will sync all resources at submission end
        // Vulkan will sync all resources at semaphore signal
        void UpdateLastFinishedFence(uint64_t fenceValue) {
            _lastFinishedFence = fenceValue;
            _CheckAndClearInvalidRefs();
            _Trim(fenceValue);
        }
        
        uint64_t IncrementLastSubmittedFence() {
            return ++_lastSubmittedFence;
        }

        uint64_t GetLastFinishedFence() const { return _lastFinishedFence; }
        uint64_t GetLastSubmittedFence() const { return _lastSubmittedFence; }

        void PushStatesToCPUTimeline(CPUTimeline& dst, uint64_t fenceVal) const;
        void PushStatesToGPUTimeline(GPUTimeline& dst, uint64_t fenceVal) const;

    };

    // CPU timeline is a more special timeline: no API extrapolation,
    // every resource is in sync, thus no generation tracking needed
    class CPUTimeline {
        std::unordered_map<WeakTrackedRef<ITextureView>, TextureLayout> _currentState;
    public:

        TextureLayout PeekCurrentState(WeakTrackedRef<ITextureView> resource) const { 
            auto it = _currentState.find(resource);
            if(it == _currentState.end()) return TextureLayout::Undefined;
            return it->second;
        }

        void RegisterTextureState(WeakTrackedRef<ITextureView> ref, 
                                  TextureLayout layout
        ) {
            _currentState[ref] = layout;  
        }

        // CPU timelines will transfer the resource states out to the requesting
        // timeline during Submit(), and receive back on WaitForEvent().
        TextureLayout FetchCurrentState(WeakTrackedRef<ITextureView> resource) { 
            auto it = _currentState.find(resource);
            if(it == _currentState.end()) return TextureLayout::Undefined;
            auto oldState = it->second;
            it->second = TextureLayout::Undefined;
            return oldState;
        }

        // The "receive back" part, meant to be used in WaitForEvent() fron CPU 
        void ReceiveStateFrom(const GPUTimeline& src, uint64_t fenceValue) {
            src.PushStatesToCPUTimeline(*this, fenceValue);
        }

        // Due to the reason that resources can be destroyed on any thread,
        // we can't let resource directly report back and clear its reference.
        // This is intended to be called after "dirty" bit set and triggers 
        // deferred cleaning process on "resource creation APIs"
        void CheckAndClearInvalidRefs() {
            auto it = _currentState.cbegin();
            while(it != _currentState.cend()) {
                if(!it->first)  it = _currentState.erase(it);
                else            ++it;
            }
        }
    };
} // namespace alloy

