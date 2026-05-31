#pragma once

#include "alloy/CommandList.hpp"
#include "alloy/common/RefCnt.hpp"

namespace alloy {

    class ITrackingCommandList : public common::RefCntBase {

    public:
        // No Barrier() exposed. Use automatic resource state tracking
        // and barrier insertion (including layout transiton)

        // Use TransitionTextureToDefaultLayout() to request texture
        // layout trasition to default/generic layout to be used on
        // dedicated transfer queue.

        virtual void Begin() = 0;
        virtual void End() = 0;

        /////#TODO: add load, store and clearcolor handling for more efficient operation
        virtual IRenderCommandEncoder& BeginRenderPass( const RenderPassAction&,
                                                        const PassResourceUsage& ) = 0;
        virtual IComputeCommandEncoder& BeginComputePass( const PassResourceUsage& ) = 0;
        virtual ITransferCommandEncoder& BeginTransferPass() = 0;
        //virtual IBaseCommandEncoder* BeginWithBasicEncoder() = 0;

        virtual void SetDebugName(const std::string& ) = 0;

        virtual void EndPass() = 0;

        virtual void TransitionTextureToDefaultLayout(
            const std::vector<common::sp<ITextureView>>& textures
        ) = 0;

        virtual void PushDebugGroup(const std::string& name,const Color4f& color) = 0;

        virtual void PopDebugGroup() = 0;

        virtual void InsertDebugMarker(const std::string& name,const Color4f& color) = 0;
    
    };

}
