#include "ResourceStateTracker.hpp"

#include "alloy/common/RefCnt.hpp"

#include <algorithm>

namespace alloy::utils
{


    static bool _HasWriteAccess(ResourceAccess access) {
        using alloy::common::operator|;
        static const ResourceAccesses writeAccesses 
            = ResourceAccess::COMMON
            | ResourceAccess::RENDER_TARGET
            | ResourceAccess::UNORDERED_ACCESS
            | ResourceAccess::DepthStencilWritable
            | ResourceAccess::STREAM_OUTPUT
            | ResourceAccess::COPY_DEST
            | ResourceAccess::RESOLVE_DEST
            | ResourceAccess::RAYTRACING_ACCELERATION_STRUCTURE_WRITE
            ;

        return writeAccesses[access];
    }


    static bool NeedsMemoryBarrier(ResourceAccess before, ResourceAccess after) {
        if (before == ResourceAccess::COMMON) {
            return before != after;
        }
        //If any of the accesses are write, we need a memory barrier
        return _HasWriteAccess(before) || _HasWriteAccess(after);

        return false;
    }

    //void ITimeline::SyncToTrackerEvent(
    //    ITrackerEvent* syncEvent,
    //    uint64_t syncValue
    //) {
    //    if(!syncEvent->SyncTimelineToThis(*this, syncValue)) {
    //        //The sync event isn't resolved
    //    }
    //}

    BarrierActions CalculateBarriersBetweenStates(
        const ResourceStates& currLocalState,
        const ResourceStates& reqestedState,
        std::function<BufferState(ITrackedBuffer*)> fnGetNotFoundBufferState,
        std::function<TextureState(ITrackedTexture*)> fnGetNotFoundTextureState
    ) {
        BarrierActions barriers {};
        auto& bufferBarrierReqs = barriers.bufferBarrierActions;
        auto& textureBarrierReqs = barriers.textureBarrierActions;
        
        auto _GetResourceState = [&](
            const auto& states,
            auto fn,
            auto resource
        ) {
            {
                auto it = states.find(resource);
                if(it != states.end()) {
                    return it->second;
                }
            }
            return fn(resource);
        };

        auto _GetBufferState = [&](ITrackedBuffer* buffer) -> BufferState {
            return _GetResourceState(
                currLocalState.buffers,
                fnGetNotFoundBufferState,
                buffer
            );
        };

        auto _GetTextureState = [&](ITrackedTexture* texture) -> TextureState {
            return _GetResourceState(
                currLocalState.textures,
                fnGetNotFoundTextureState,
                texture
            );
        };

        for(auto& [buffer, stateReq] : reqestedState.buffers) {
            //BufferState state {};
            //Search for current recorded state
            auto state = _GetBufferState(buffer);

            if(state.stage == PipelineStage::None) {
                continue;
            }
            if(NeedsMemoryBarrier(state.access, stateReq.access)) {
                BufferBarrierAction action {
                    .before = state,
                    .after = stateReq,
                    .buffer = buffer
                };
                bufferBarrierReqs.push_back(action);
            }
        }

        for(auto& [texture, stateReq] : reqestedState.textures) {
            //TextureState state {};
            //Search for current recorded state
            auto state = _GetTextureState(texture);

            bool needsMemBarrier = false;
            if(state.stage != PipelineStage::None) {
                needsMemBarrier = NeedsMemoryBarrier(state.access, stateReq.access);
            }

            if( needsMemBarrier ||
                state.layout != stateReq.layout) {
                TextureBarrierAction action {
                    .before = state,
                    .after = stateReq,
                    .texture = texture
                };
                textureBarrierReqs.push_back(action);
            }
        }

        return barriers;
    }


    void GPUTimeline::RequestResourceStatesBeforeSubmit(
        const ResourceStates& states
    ){
        auto& cpuStates = _cpuTimeline.GetCurrentState();
        BarrierActions barriers = CalculateBarriersBetweenStates(
            _currentState,
            states,
            [&](ITrackedBuffer* buffer) -> BufferState {
                auto state = cpuStates.buffers[buffer];
                _currentState.buffers[buffer] = state;
                buffer->RegisterTimeline(this);
                return state;
            },
            [&](ITrackedTexture* texture) -> TextureState {
                auto state = cpuStates.textures[texture];
                _currentState.textures[texture] = state;
                texture->RegisterTimeline(this);
                return state;
            }
        );
        
        InsertBarriers(barriers);
    }
    void GPUTimeline::MarkResourceStatesAfterSubmit(
        const ResourceStates& states
    ) {
        _currentState.SyncTo(states);
    }




    //void GPUTimeline::RegisterSyncPoint(
    //    common::sp<ITrackerEvent> syncEvent,
    //    uint64_t syncValue
    //) {
    //    _CleanupSyncPoints();
    //    //_currentStateFinished.SyncTo(_currentStateInFlight);
    //    _syncPoints.push_back({ _currentStateFinished, syncEvent, syncValue });
    //    syncEvent->RegisterSyncPoint(this, syncValue);
    //}

    void ITrackerCmdQ::EncodeSignalEvent(IEvent* fence, uint64_t value) {
        auto trackerEvt = common::ref_sp(static_cast<ITrackerEvent*>(fence));
        trackerEvt->RegisterSyncPoint(this, value);
    }

    void ITrackerCmdQ::EncodeWaitForEvent(IEvent* fence, uint64_t value) {
        auto trackerEvt = common::ref_sp(static_cast<ITrackerEvent*>(fence));
        trackerEvt->SyncTimelineToThis(this, value);
    }

    void ITrackerCmdQ::SubmitCommand(ICommandList* cmd) {
        auto trackerCmdList = common::ref_sp(static_cast<ITrackerCmdList*>(cmd));
        auto& req = trackerCmdList->GetRequestedResourceStates();
        RequestResourceStatesBeforeSubmit(req);
        auto& res = trackerCmdList->GetFinalResourceStates();
        MarkResourceStatesAfterSubmit(res);
    }

    void ITrackerEvent::RegisterSyncPoint(ITimeline* timeline, uint64_t syncValue) {
        _signalingTimelines[timeline] = syncValue;
    }

    void ITrackerEvent::SyncTimelineToThis(ITimeline* timeline, uint64_t syncValue) {

        struct _Kvpair {
            ITimeline* timeline;
            uint64_t fenceValue;
        };

        std::vector<_Kvpair> timelinesSorted{};

        for(auto& [k, v] : _signalingTimelines) {
            if(k == timeline) {
                //Same queue signal and wait may cause deadlocks.
                //Put an assertion here for ease of debugging
                assert(v >= syncValue);
                continue;
            }
            timelinesSorted.push_back({k, v});
        }

        std::sort(timelinesSorted.begin(), timelinesSorted.end(),
            [](const _Kvpair& a, const _Kvpair& b) {
                return a.fenceValue < b.fenceValue;
            }
        );

        ResourceStates states {};
        for(auto&[k, _] : timelinesSorted) {
            states.SyncTo(k->GetCurrentState());
        }

        timeline->GetCurrentState().SyncTo(states);
    }


}
