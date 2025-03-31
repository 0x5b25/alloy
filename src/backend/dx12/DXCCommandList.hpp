#pragma once

#include "alloy/common/RefCnt.hpp"
#include "alloy/CommandList.hpp"

#include <vector>
#include <functional>
#include <span>
#include <unordered_map>
#include <unordered_set>

#include "DXCPipeline.hpp"
#include "DXCBindableResource.hpp"
#include "DXCFrameBuffer.hpp"


//platform specific headers
#include <dxgi1_4.h> //Guaranteed by DX12
#include <d3d12.h>

//TODO: a system to track image layouts inside a command buffer and
//insert image layout transition commands when necessary. also each
//VulkanTexture should record its last known layout(changed by tracking
// system during a command buffer commit).

//Because the unordered nature of command buffer execute order, multiple
// command buffer referencing the same texture

namespace alloy::dxc
{
    class DXCDevice;
    class DXCBuffer;
    class DXCTexture;
    class DXCCmdEncBase;

    
    class DXCCommandList: public ICommandList{
        
    public:

        struct ResourceStates {
            std::unordered_map<DXCBuffer*, D3D12_RESOURCE_STATES> buffers;
            std::unordered_map<DXCTexture*, D3D12_RESOURCE_STATES> textures;

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


    protected:
        common::sp<DXCDevice> _dev;
        ID3D12CommandAllocator* _cmdAlloc;
        ID3D12GraphicsCommandList* _cmdList;
        
        std::vector<DXCCmdEncBase*> _passes;
        DXCCmdEncBase* _currentPass;

        //std::vector<_RenderPassInfo> _rndPasses;

        //Resources used
        std::unordered_set<common::sp<RefCntBase>> _devRes;

        //DX12 resource state decay rule:
        //The following resources will decay when an 
        //ExecuteCommandLists operation is completed on the GPU:
        //  1. Resources being accessed on a Copy queue, or
        //  2. Buffer resources on any queue type, or
        //  3. Texture resources on any queue type that have the 
        //       D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS flag set, or
        //  4. Any resource implicitly promoted to a read-only state.
        //only the texture states need explicit request and update.
        decltype(ResourceStates::textures) _requestedStates, _finalStates;

        //sp<VulkanPipelineBase> _currentPipeline;
        //std::vector<sp<VulkanResourceSet>> _currentResourceSets;

        //renderpasses
        //std::set<sp<VulkanFramebuffer>> _currRenderPassFBs;

    public:
        DXCCommandList(
            const common::sp<DXCDevice>& dev,
            ID3D12CommandAllocator* pAllocator,
            ID3D12GraphicsCommandList* pList
        ) 
            : _dev(dev)
            , _cmdAlloc(pAllocator)
            , _cmdList(pList)
            , _currentPass(nullptr)
        {}

        virtual ~DXCCommandList() override;

        
        static common::sp<DXCCommandList> Make(const common::sp<DXCDevice>& dev, D3D12_COMMAND_LIST_TYPE type);
        ID3D12CommandList* GetHandle() const { return _cmdList; }
        
        virtual void Begin() override;
        virtual void End() override;
#if 0
        virtual void ClearColorTarget(
            std::uint32_t slot, 
            float r, float g, float b, float a) override;

        virtual void ClearDepthStencil(float depth, std::uint8_t stencil) override;
#endif 
        

        ///#TODO: add load, store and clearcolor handling for more efficient operation
        virtual IRenderCommandEncoder& BeginRenderPass(const RenderPassAction&) override;
        virtual IComputeCommandEncoder& BeginComputePass() override;
        virtual ITransferCommandEncoder& BeginTransferPass() override;
        //virtual IBaseCommandEncoder* BeginWithBasicEncoder() = 0;

        virtual void EndPass() override;

        
        virtual void Barrier(const std::vector<alloy::BarrierDescription>&) override;

        virtual void PushDebugGroup(const std::string& name) override;

        virtual void PopDebugGroup() override;

        virtual void InsertDebugMarker(const std::string& name) override;

        const auto& GetRequestedResourceStates() const { return _requestedStates; }
        const auto& GetFinalResourceStates() const { return _finalStates; }
    };


    struct DXCCmdEncBase {

        DXCDevice* dev;
        DXCCommandList* cmdList;

        std::unordered_set<common::sp<common::RefCntBase>> resources;
        DXCCommandList::ResourceStates resStates;
        std::vector<std::function<void()>> recordedCmds;
        
        DXCCmdEncBase(DXCDevice* dev,
                      DXCCommandList* cmdList) 
            : dev(dev)
            , cmdList(cmdList)
        { }

        virtual ~DXCCmdEncBase() {}

        //End commandpass will write commands into command list
        virtual void EndPass();

        
        ID3D12GraphicsCommandList* GetCmdList() const;

        
        void RegisterBufferUsage(
            DXCBuffer* buffer,
            D3D12_RESOURCE_STATES state
        );

        void RegisterTexUsage(
            DXCTexture* tex,
            D3D12_RESOURCE_STATES state
        );

        void RegisterResourceSet(DXCResourceSet* d3drs);
    };

    struct DXCRenderCmdEnc : public IRenderCommandEncoder, public DXCCmdEncBase {

        
        DXCGraphicsPipeline* _currentPipeline;

        RenderPassAction _fb;

        DXCRenderCmdEnc(
            DXCDevice *dev,
            DXCCommandList *cmdList,
            const RenderPassAction &act
        );

        virtual ~DXCRenderCmdEnc() {}

        virtual void SetPipeline(const common::sp<IGfxPipeline>&) override;

        virtual void SetVertexBuffer(
            std::uint32_t index, const common::sp<BufferRange>& buffer) override;
    
        virtual void SetIndexBuffer(
            const common::sp<BufferRange>& buffer, IndexFormat format) override;

        
        virtual void SetGraphicsResourceSet(const common::sp<IResourceSet>& rs) override;
        
        virtual void SetPushConstants(
            std::uint32_t pushConstantIndex,
            const std::span<uint32_t>& data,
            std::uint32_t destOffsetIn32BitValues) override;

        virtual void SetViewports(const std::span<Viewport>& viewport) override;
        virtual void SetFullViewports() override;

        virtual void SetScissorRects(const std::span<Rect>& ) override;
        virtual void SetFullScissorRects() override;


        virtual void Draw(
            std::uint32_t vertexCount, std::uint32_t instanceCount,
            std::uint32_t vertexStart, std::uint32_t instanceStart) override;
        
        virtual void DrawIndexed(
            std::uint32_t indexCount, std::uint32_t instanceCount, 
            std::uint32_t indexStart, std::uint32_t vertexOffset, 
            std::uint32_t instanceStart) override;
#if 0
        virtual void DrawIndirect(
            const common::sp<IBuffer>& indirectBuffer, 
            std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride) override;

        virtual void DrawIndexedIndirect(
            const common::sp<IBuffer>& indirectBuffer, 
            std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride) override;
#endif
        virtual void WaitForFenceBeforeStages(const common::sp<IFence>&, const PipelineStages&) override {}
        virtual void UpdateFenceAfterStages(const common::sp<IFence>&, const PipelineStages&) override {}
    

    };

    struct DXCComputeCmdEnc : public IComputeCommandEncoder, public DXCCmdEncBase {
        
        DXCComputePipeline* _currentPipeline;

        DXCComputeCmdEnc(DXCDevice* dev, DXCCommandList* cmdList)
            : DXCCmdEncBase{ dev, cmdList }
        { }

        virtual void SetPipeline(const common::sp<IComputePipeline>&) override;

        virtual void SetComputeResourceSet(
            const common::sp<IResourceSet>& rs
            /*const std::vector<std::uint32_t>& dynamicOffsets*/) override;
        
        virtual void SetPushConstants(
            std::uint32_t pushConstantIndex,
            const std::span<uint32_t>& data,
            std::uint32_t destOffsetIn32BitValues) override;

        /// <summary>
        /// Dispatches a compute operation from the currently-bound compute state of this Pipeline.
        /// </summary>
        /// <param name="groupCountX">The X dimension of the compute thread groups that are dispatched.</param>
        /// <param name="groupCountY">The Y dimension of the compute thread groups that are dispatched.</param>
        /// <param name="groupCountZ">The Z dimension of the compute thread groups that are dispatched.</param>
        virtual void Dispatch(std::uint32_t groupCountX,
                              std::uint32_t groupCountY,
                              std::uint32_t groupCountZ) override;

        ///:TODO: Needs further confirmation about indirect commands
#if 0
        /// Issues an indirect compute dispatch command based on the information contained in the given indirect
        /// <see cref="DeviceBuffer"/>. The information stored in the indirect Buffer should conform to the structure of
        /// <see cref="IndirectDispatchArguments"/>.
        /// <param name="indirectBuffer">The indirect Buffer to read from. Must have been created with the
        /// <see cref="BufferUsage.IndirectBuffer"/> flag.</param>
        /// <param name="offset">An offset, in bytes, from the start of the indirect buffer from which the draw commands will be
        /// read. This value must be a multiple of 4.</param>
        virtual void DispatchIndirect(const sp<Buffer>& indirectBuffer, std::uint32_t offset) override
#endif

        
        virtual void WaitForFenceBeforeStages(const common::sp<IFence>&, const PipelineStages&) override {}
        virtual void UpdateFenceAfterStages(const common::sp<IFence>&, const PipelineStages&) override {}    
    };

    struct DXCTransferCmdEnc : public ITransferCommandEncoder, public DXCCmdEncBase {
        DXCTransferCmdEnc(DXCDevice* dev, DXCCommandList* cmdList)
            : DXCCmdEncBase{ dev, cmdList }
        { }

        virtual void CopyBuffer(
            const common::sp<BufferRange>& source,
            const common::sp<BufferRange>& destination,
            std::uint32_t sizeInBytes) override;
                

        virtual void CopyBufferToTexture(
            const common::sp<BufferRange>& src,
            std::uint32_t srcBytesPerRow,
            std::uint32_t srcBytesPerImage,
            const common::sp<ITexture>& dst,
            const Point3D& dstOrigin,
            std::uint32_t dstMipLevel,
            std::uint32_t dstBaseArrayLayer,
            const Size3D& copySize
        ) override;

        virtual void CopyTextureToBuffer(
            const common::sp<ITexture>& src,
            const Point3D& srcOrigin,
            std::uint32_t srcMipLevel,
            std::uint32_t srcBaseArrayLayer,
            const common::sp<BufferRange>& dst,
            std::uint32_t dstBytesPerRow,
            std::uint32_t dstBytesPerImage,
            const Size3D& copySize
        ) override;

        virtual void CopyTexture(
            const common::sp<ITexture>& src,
            const Point3D& srcOrigin,
            std::uint32_t srcMipLevel,
            std::uint32_t srcBaseArrayLayer,
            const common::sp<ITexture>& dst,
            const Point3D& dstOrigin,
            std::uint32_t dstMipLevel,
            std::uint32_t dstBaseArrayLayer,
            const Size3D& copySize) override;

        virtual void ResolveTexture(const common::sp<ITexture>& source, const common::sp<ITexture>& destination) override;
        
        virtual void GenerateMipmaps(const common::sp<ITexture>& texture) override;

        virtual void WaitForFence(const common::sp<IFence>&) override {}
        virtual void UpdateFence(const common::sp<IFence>&) override {}
    };



    class DXCCommandList6 : public DXCCommandList {

        ID3D12GraphicsCommandList6* GetCmdList() const { return static_cast<ID3D12GraphicsCommandList6*>(_cmdList); }

    public:
        DXCCommandList6(
            const common::sp<DXCDevice>& dev,
            ID3D12CommandAllocator* pAllocator,
            ID3D12GraphicsCommandList6* pList
        ) 
            : DXCCommandList(dev, pAllocator, pList)
        {}


    };


    class DXCCommandList7 : public DXCCommandList6 {

        ID3D12GraphicsCommandList7* GetCmdList() const { return static_cast<ID3D12GraphicsCommandList7*>(_cmdList); }
    public:
        DXCCommandList7(
            const common::sp<DXCDevice>& dev,
            ID3D12CommandAllocator* pAllocator,
            ID3D12GraphicsCommandList7* pList
        ) 
            : DXCCommandList6(dev, pAllocator, pList)
        {}

        
        
        virtual void Barrier(const std::vector<alloy::BarrierDescription>&) override;
    };
    

} // namespace alloy


