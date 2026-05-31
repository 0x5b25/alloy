#pragma once

#include "alloy/ResourceBarrier.hpp"
#include "alloy/CommandList.hpp"

#include "alloy/layers/AutoResourceUsageTracking/ITrackingCommandList.hpp"

#include "TrackedResource.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

namespace alloy::layers::AutoResourceUsageTracking {

    class TrackingCommandQueue;
    
    struct TrackingCmdEncBase;

    class TrackingCommandList : public ITrackingCommandList {
    public:

        // A more detailed tracking within a command list
        // than the presisting layout tracing in TrackingTimeline.hpp
        struct BufferState {
            PipelineStages stage;
            ResourceAccesses access;
        };

        struct TextureState {
            PipelineStages stage;
            ResourceAccesses access;
            TextureLayout layout;
        };

        struct ResourceStates {
            std::unordered_map<IBuffer*, BufferState> buffers;
            std::unordered_map<TrackedTexView*, TextureState> textures;

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

    private:

        ResourceStates _requestedStates, _finalStates;

        common::sp<TrackingDevice> _dev;
        TrackingCommandQueue* _cmdQ;
        common::sp<ICommandList> _inner;


        std::vector<TrackingCmdEncBase*> _passes;
        TrackingCmdEncBase *_currentPass;

        std::unordered_set<common::sp<RefCntBase>> _resRefs;

        void _EndCurrentActivePass();
        void _BeginDummyPassIfNoActivePass();

    public:

        TrackingCommandList(
            common::sp<TrackingDevice> dev,
            TrackingCommandQueue* cmdQ,
            common::sp<ICommandList> inner
        );

        virtual ~TrackingCommandList() override;

        const std::unordered_map<TrackedTexView*, TextureState>& 
            GetResourceStateReqs() const {return _requestedStates.textures;}

            
        auto& GetFinalResourceStates() const {
            return _finalStates.textures;
        }

        ICommandList* GetInner() const {return _inner.get();}

        //Delegates
        virtual void Begin() override;
        virtual void End() override;

        virtual IRenderCommandEncoder& BeginRenderPass(const RenderPassAction&) override;
        virtual IComputeCommandEncoder& BeginComputePass() override;
        virtual ITransferCommandEncoder& BeginTransferPass() override;
        
        virtual void SetDebugName(const std::string& name) override {
            _inner->SetDebugName(name);
        }

        virtual void EndPass() override;

        virtual void TransitionTextureToDefaultLayout(
            const std::vector<common::sp<ITextureView>>& textures
        ) override;

        virtual void PushDebugGroup(const std::string& name,const Color4f& color) override;

        virtual void PopDebugGroup() override;

        virtual void InsertDebugMarker(const std::string& name,const Color4f& color) override;
    
    };


    struct TrackingCmdEncBase {
        TrackingCommandList* cmdList;
        
        // No resource holding here:
        //   1. the inner cmdList will hold used resources after
        //     the recorded commands are replayed
        //   2. Before replay, strong references are holded in 
        //     recorded std::function
        //std::unordered_set<common::sp<common::RefCntBase>> resources;

        TrackingCommandList::ResourceStates firstState, lastState;

        std::vector<std::function<void(ICommandList*)>> recordedCmds;

        TrackingCmdEncBase(TrackingCommandList* cmdList)
            : cmdList(cmdList) {}

        virtual ~TrackingCmdEncBase();

        virtual void EndPass() {
            for(auto& cmd: recordedCmds) {
                cmd(cmdList->GetInner());
            }

            cmdList->EndPass();
        }

        void RegisterBufferUsage(
            IBuffer* buffer,
            const TrackingCommandList::BufferState& state
        );

        void RegisterTexUsage(
            TrackedTexView* tex,
            const TrackingCommandList::TextureState& state
        );

        void RegisterResourceSet(IResourceSet* rs);

        void PushDebugGroup(const std::string& name, const Color4f&);
        void PopDebugGroup();
        void InsertDebugMarker(const std::string& name, const Color4f&);

    };


    struct TrackingRndCmdEnc : public IRenderCommandEncoder
                             , public TrackingCmdEncBase
    {
        IRenderCommandEncoder* inner;

        TrackingRndCmdEnc(
            TrackingCommandList* cmdList,
            const RenderPassAction& fb
        );

        //Delegates
        virtual void SetPipeline(const common::sp<IGfxPipeline>&) override;
        virtual void SetPipeline(const common::sp<IMeshShaderPipeline>&) override;

        virtual void SetVertexBuffer(
            std::uint32_t index, const common::sp<BufferRange>& buffer) override;
    
        virtual void SetIndexBuffer(
            const common::sp<BufferRange>& buffer, IndexFormat format) override;

         virtual void SetDescriptorHeaps(
            const common::sp<IResourceDescriptorHeap>& resourceHeap,
            const common::sp<ISamplerDescriptorHeap>& samplerHeap
        ) override {
            assert(false);
        }

        virtual void SetPushConstants(
            std::uint32_t pushConstantIndex,
            std::span<const uint32_t> data,
            std::uint32_t destOffsetIn32BitValues) override;

        virtual void SetGraphicsResourceSet(
            const common::sp<IResourceSet>& rs
            /*const std::vector<std::uint32_t>& dynamicOffsets*/) override;

        virtual void SetGraphicsMutableResourceSet(
            const common::sp<IMutableResourceSet>& rs) override;

        virtual void SetViewports(std::span<const Viewport> viewport) override;
        
        virtual void SetFullViewport() override;

        virtual void SetScissorRects(std::span<const Rect> ) override;
        
        virtual void SetFullScissorRect() override;

        
        virtual void Draw(
            std::uint32_t vertexCount, std::uint32_t instanceCount,
            std::uint32_t vertexStart, std::uint32_t instanceStart) override;
        void Draw(std::uint32_t vertexCount){Draw(vertexCount, 1, 0, 0);}
        
        virtual void DrawIndexed(
            std::uint32_t indexCount, std::uint32_t instanceCount, 
            std::uint32_t indexStart, std::uint32_t vertexOffset, 
            std::uint32_t instanceStart) override;
        

        virtual void DispatchMesh(std::uint32_t groupCountX,
                                  std::uint32_t groupCountY,
                                  std::uint32_t groupCountZ) override;
    };
    
    struct TrackingCompCmdEnc : public IComputeCommandEncoder
                              , public TrackingCmdEncBase 
    {
        TrackingCommandList* cmdList;
        IComputeCommandEncoder* inner;

        
        TrackingCompCmdEnc(
            TrackingCommandList* cmdList
        );

        virtual void SetPipeline(const common::sp<IComputePipeline>&) override;

        virtual void SetComputeResourceSet(
            const common::sp<IResourceSet>& rs
            /*const std::vector<std::uint32_t>& dynamicOffsets*/) override;

        virtual void SetComputeMutableResourceSet(
            const common::sp<IMutableResourceSet>& rs) override;

            
        virtual void SetDescriptorHeaps(
            const common::sp<IResourceDescriptorHeap>& resourceHeap,
            const common::sp<ISamplerDescriptorHeap>& samplerHeap
        ) override {
            assert(false);
        }

        virtual void SetPushConstants(
            std::uint32_t pushConstantIndex,
            std::span<const uint32_t> data,
            std::uint32_t destOffsetIn32BitValues) override;

        virtual void Dispatch(std::uint32_t groupCountX, std::uint32_t groupCountY, std::uint32_t groupCountZ) override;

       
    };

    struct TrackingXferCmdEnc : public ITransferCommandEncoder
                              , public TrackingCmdEncBase
    {
        TrackingCommandList* cmdList;
        ITransferCommandEncoder* inner;

        
        TrackingXferCmdEnc(
            TrackingCommandList* cmdList
        );

        virtual void CopyBuffer(
            const common::sp<BufferRange>& source,
            const common::sp<BufferRange>& destination,
            std::uint32_t sizeInBytes) override;
                

        virtual void CopyBufferToTexture(
            const common::sp<BufferRange>& src,
            std::uint32_t srcBytesPerRow,
            std::uint32_t srcBytesPerImage,
            const common::sp<ITextureView>& dst,
            const Point3D& dstOrigin,
            std::uint32_t dstMipLevel,
            std::uint32_t dstBaseArrayLayer,
            const Size3D& copySize
        ) override;

        virtual void CopyTextureToBuffer(
            const common::sp<ITextureView>& src,
            const Point3D& srcOrigin,
            std::uint32_t srcMipLevel,
            std::uint32_t srcBaseArrayLayer,
            const common::sp<BufferRange>& dst,
            std::uint32_t srcBytesPerRow,
            std::uint32_t srcBytesPerImage,
            const Size3D& copySize
        ) override;
        
        virtual void CopyTexture(
            const common::sp<ITextureView>& src,
            const Point3D& srcOrigin,
            std::uint32_t srcMipLevel,
            std::uint32_t srcBaseArrayLayer,
            const common::sp<ITextureView>& dst,
            const Point3D& dstOrigin,
            std::uint32_t dstMipLevel,
            std::uint32_t dstBaseArrayLayer,
            const Size3D& copySize) override;

        virtual void GenerateMipmaps(const common::sp<ITexture>& texture) override;
    };
}
