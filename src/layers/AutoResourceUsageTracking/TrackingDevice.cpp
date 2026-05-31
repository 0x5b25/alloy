#include "TrackingDevice.hpp"

#include "TrackedResource.hpp"
#include "TrackingCommandList.hpp"

#include <format>

namespace alloy::layers::AutoResourceUsageTracking
{
    
    TrackingDevice::TrackingDevice(common::sp<IGraphicsDevice> dev)
        : _inner(std::move(dev)) 
    {
        auto& factory = _inner->GetResourceFactory();

        if(_inner->GetGfxCommandQueue()) {
            auto evt = factory.CreateSyncEvent();
            _gfxQ = new TrackingCommandQueue(this, std::move(evt), _inner->GetGfxCommandQueue());
        }
    }

    TrackingDevice::~TrackingDevice() {
        delete _gfxQ;
        delete _copyQ;
    }

    
    common::sp<ITexture> TrackingDevice::CreateTrackedTexture(
        const ITexture::Description& desc
    ) {
        auto trackedTex = new TrackedTexture(
            common::ref_sp(this), 
            _inner->GetResourceFactory().CreateTexture(desc)
        );

        //RegisterTextureState(
        //    trackedTex->GetTrackedRef(),
        //    TextureLayout::Undefined
        //);

        return common::sp{trackedTex};
    }

    ISwapChain::State TrackingDevice::PresentToSwapChain(ISwapChain* sc) {

        auto scImpl = PtrCast<TrackedSwapChain>(sc);

        auto trackedFb = PtrCast<TrackedSCTexView>(scImpl->GetBackBuffer().get());


        _gfxQ->PrepareTextureForPresent(trackedFb);

        // For now. Need to add rendertarget->presentable layout transition
        _inner->PresentToSwapChain(sc);

        return ISwapChain::State::Optimal;
    }

    
    void TrackingCommandQueue::EncodeWaitForTrackingEvent(
        ITrackingEvent* fence,
        uint64_t value
    ) {
        auto pEvent = static_cast<TrackingEvent*>(fence);
        auto srcTimeline = pEvent->GetTimeline();
        assert(srcTimeline != this);

        srcTimeline->PushStatesToGPUTimeline(*this, value);
        _inner->EncodeWaitForEvent(pEvent->GetInner(), value);
    }


    void TrackingCommandQueue::_GetFinishedSubmissions() {
        auto completedVal = _trackingEvt.GetSignaledValue();
        UpdateLastFinishedFence(completedVal);
    }

    
    void TrackingCommandQueue::_RecycleTransitionCmdBufs() {
        auto completedVal = GetLastFinishedFence();
        while(!_cmdListsInUse.empty()) {
            auto& entry = _cmdListsInUse.front();
            if(entry.lastSubmittedFence <= completedVal) {
                entry.cmdList.reset();
                _cmdListsInUse.pop();
            }
            else {
                break;
            }
        }
    }
    
    common::sp<ICommandList> TrackingCommandQueue::_GetOneTransitionCmdList() {
        _RecycleTransitionCmdBufs();

        auto cmdList = _inner->CreateCommandList();

        _cmdListsInUse.push({
            cmdList,
            GetLastSubmittedFence()
        });

        return cmdList;
    }


    
    void TrackingCommandQueue::_TransitResourceStatesBeforeSubmit(
        const TrackingCommandList& cmdList
    ) {
        auto& resStates = cmdList.GetResourceStateReqs();

        auto& cpuTimeline = _dev;

        std::vector<alloy::BarrierOp> barriers;

        for(auto& [texture, stateReq] : resStates) {

            auto trackedRef = texture->GetTrackedRef();

            //Search for current recorded state
            TextureLayout currState = TextureLayout::Undefined;
            if(ContainsResource(trackedRef)) {
                currState = GetLatestResourceState(trackedRef);
            } else {
                currState = cpuTimeline->FetchCurrentState(trackedRef);
            }

            if(currState != stateReq.layout) {
                using common::operator|;
                barriers.emplace_back(TextureBarrierOp{
                    .texture = common::ref_sp(texture), 
                    // BOTTOM_OF_PIPE
                    .from = {
                        .stages = PipelineStage::AllCommands,
                        .access = {},
                        .layout = currState,
                    },
                    // TOP_OF_PIPE
                    .to = {
                        .stages = PipelineStage::AllCommands,
                        .access = {},
                        .layout = stateReq.layout,
                    }
                });
            }
        }

        if(!barriers.empty()) {
            auto gfxCmdList = _GetOneTransitionCmdList();
            gfxCmdList->Barrier(barriers);
            gfxCmdList->End();

            std::string debugName = std::format("ResTransCmdList_fence#{}", GetLastSubmittedFence());
            gfxCmdList->SetDebugName( debugName );

            _inner->SubmitCommand( gfxCmdList.get() );
        }

    }

    void TrackingCommandQueue::_MarkResourceStatesAfterSubmit(
        const TrackingCommandList& cmdList
    ) {
        auto& resStates = cmdList.GetFinalResourceStates();
        for(auto&[tex, stateReq] : resStates) {
            RegisterTextureState(tex->GetTrackedRef(), stateReq.layout);
        }
    }


    uint64_t TrackingCommandQueue::SubmitCommand(ITrackingCommandList* cmdIf) {
        auto cmd = static_cast<TrackingCommandList*>(cmdIf);
        
        //Will also trim states list
        _GetFinishedSubmissions();

        auto fenceValue = IncrementLastSubmittedFence();
        
        _TransitResourceStatesBeforeSubmit(*cmd);
        _MarkResourceStatesAfterSubmit(*cmd);

        _inner->SubmitCommand(cmd->GetInner());

        //Signal the fence
        _inner->EncodeSignalEvent(_trackingEvt.GetInner(), fenceValue);

        return fenceValue;
    }

    common::sp<ITrackingCommandList> TrackingCommandQueue::CreateCommandList() {
        return common::make_sp<TrackingCommandList>(
            common::ref_sp(_dev),
            this,
            _inner->CreateCommandList()
        );
    }

    
    void TrackingCommandQueue::PrepareTextureForPresent(TrackedTexView* tex) {
        auto& cpuTimeline = _dev;
        
        auto trackedRef = tex->GetTrackedRef();

        TextureLayout currLayout = TextureLayout::Undefined;
        if(ContainsResource(trackedRef)) {
            currLayout = GetLatestResourceState(trackedRef);
        } else {
            currLayout = cpuTimeline->FetchCurrentState(trackedRef);
        }

        if(currLayout != TextureLayout::Present) {

            auto fenceValue = IncrementLastSubmittedFence();
            auto cmdBuf = _GetOneTransitionCmdList();

            BarrierOp op[1] ={
                TextureBarrierOp{
                    .texture = common::ref_sp(tex),
                    .from = {
                        .stages = PipelineStage::AllCommands,
                        .access = {},
                        .layout = currLayout,
                    },
                    .to = {
                        .stages = PipelineStage::AllCommands,
                        .access = {},
                        .layout = TextureLayout::Present,
                    },
                }
            };

            cmdBuf->Begin();
            cmdBuf->Barrier(op);
            cmdBuf->End();

            std::string debugName = std::format("PrepPresentCmdList_fence#{}", fenceValue);
            cmdBuf->SetDebugName( debugName );

            _inner->SubmitCommand( cmdBuf.get() );

            //Signal the fence
            _inner->EncodeSignalEvent(_trackingEvt.GetInner(), fenceValue);


            RegisterTextureState(trackedRef, TextureLayout::Present);
        }
    }
}

