#pragma once

#include <volk.h>

#include "alloy/common/RefCnt.hpp"
#include "alloy/CommandList.hpp"

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "VulkanPipeline.hpp"
#include "VulkanBindableResource.hpp"
#include "VulkanFramebuffer.hpp"

//TODO: a system to track image layouts inside a command buffer and
//insert image layout transition commands when necessary. also each
//VulkanTexture should record its last known layout(changed by tracking
// system during a command buffer commit).

//Because the unordered nature of command buffer execute order, multiple
// command buffer referencing the same texture

namespace alloy::vk
{
    class VulkanDevice;
    class VulkanBuffer;
    class VulkanTexture;
    struct _CmdPoolContainer;
    class VkCmdEncBase;

    
    class VulkanCommandList : public ICommandList{

    public:
        struct BufferState {
            VkPipelineStageFlags stage;
            VkAccessFlags2 access;
        };

        struct TextureState {
            VkPipelineStageFlags stage;
            VkAccessFlags2 access;
            VkImageLayout layout;
        };

        struct ResourceStates {
            std::unordered_map<VulkanBuffer*, BufferState> buffers;
            std::unordered_map<VulkanTexture*, TextureState> textures;

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
        common::sp<VulkanDevice> _dev;
        VkCommandBuffer _cmdBuf;
        common::sp<_CmdPoolContainer> _cmdPool;

        std::vector<VkCmdEncBase*> _passes;
        VkCmdEncBase *_currentPass;

        //Resources used
        std::unordered_set<common::sp<RefCntBase>> _devRes;

        //_DevResRegistry _resReg;
        //std::unordered_set<sp<DeviceResource>> _miscResReg;

        //sp<VulkanPipelineBase> _currentPipeline;
        //std::vector<sp<VulkanResourceSet>> _currentResourceSets;

        //renderpasses
        //std::set<sp<VulkanFramebuffer>> _currRenderPassFBs;
        ResourceStates _requestedStates, _finalStates;


    public:
        VulkanCommandList(
            const common::sp<VulkanDevice>& dev,
            VkCommandBuffer cmdBuf,
            common::sp<_CmdPoolContainer>&& alloc
        );

        ~VulkanCommandList();

        //static sp<CommandList> Make(const sp<VulkanDevice>& dev);
        const VkCommandBuffer& GetHandle() const { return _cmdBuf; }
        VulkanDevice* GetDevice() const { return _dev.get(); }
        
        virtual void Begin() override;
        virtual void End() override;

        virtual IRenderCommandEncoder& BeginRenderPass(const RenderPassAction&) override;
        virtual IComputeCommandEncoder& BeginComputePass() override;
        virtual ITransferCommandEncoder& BeginTransferPass() override;
        //virtual IBaseCommandEncoder* BeginWithBasicEncoder() = 0;

        virtual void EndPass() override;
            

        virtual void PushDebugGroup(const std::string& name, const Color4f&) override;

        virtual void PopDebugGroup() override;

        virtual void InsertDebugMarker(const std::string& name, const Color4f&) override;

        
        virtual void Barrier(const std::vector<alloy::BarrierDescription>& barriers) override;
        void TransitionTextureToDefaultLayout(
            const std::vector<common::sp<ITexture>>& textures
        ) override;
        const ResourceStates& GetRequestedResourceStates() const {
            return _requestedStates;
        }
        const ResourceStates& GetFinalResourceStates() const {
            return _finalStates;
        }
        

    };

#if 0
    //Register data access and insert pipeline where necessary
    class _DevResRegistry {

        //VkMemoryBarrier{};
        //VkBufferMemoryBarrier{};
        //VkImageMemoryBarrier{};

        struct BufRef {
            VkPipelineStageFlags stage;
            VkAccessFlags access;
        };

        struct TexRef {
            VkPipelineStageFlags stage;
            VkAccessFlags access;
            VkImageLayout layout;
        };


        std::unordered_set<common::sp<common::RefCntBase>> _res;
        std::unordered_map<VulkanBuffer*, BufRef> _bufRefs;
        std::unordered_map<VulkanTexture*, TexRef> _texRefs;

        struct BufSyncInfo{
            VulkanBuffer* resource;
            BufRef prevUsage, currUsage;
        };

        struct TexSyncInfo {
            VulkanTexture* resource;
            TexRef prevUsage, currUsage;
        };

        std::vector<BufSyncInfo> _bufSyncs;
        std::vector<TexSyncInfo> _texSyncs;

    public:

        void RegisterBufferUsage(
            const sp<Buffer>& buffer,
            VkPipelineStageFlags stage,
            VkAccessFlags access
        );

        void RegisterTexUsage(
            const sp<Texture>& tex,
            VkImageLayout requiredLayout,
            VkPipelineStageFlags stage,
            VkAccessFlags access
        );

        void ModifyTexUsage(
            const sp<Texture>& tex,
            VkImageLayout layout,
            VkPipelineStageFlags stage,
            VkAccessFlags access
        );

        bool InsertPipelineBarrierIfNecessary(
            VkCommandBuffer cb
        );

    };
#endif
    //class VulkanPipeline;

    struct VkCmdEncBase {

        VulkanDevice* dev;
        VkCommandBuffer cmdList;

        std::unordered_set<common::sp<common::RefCntBase>> resources;

        VulkanCommandList::ResourceStates firstState, lastState;

        std::vector<std::function<void(VkCommandBuffer)>> recordedCmds;

        VkCmdEncBase(VulkanDevice* dev,
                     VkCommandBuffer cmdList) 
            : dev(dev)
            , cmdList(cmdList) {}

        virtual ~VkCmdEncBase() {}

        virtual void EndPass() {
            for(auto& cmd: recordedCmds) {
                cmd(cmdList);
            }
        }

        void RegisterBufferUsage(
            VulkanBuffer* buffer,
            const VulkanCommandList::BufferState& state
        );

        void RegisterTexUsage(
            VulkanTexture* tex,
            const VulkanCommandList::TextureState& state
        );

        void RegisterResourceSet(VulkanResourceSet* rs);

    };

    struct VkRenderCmdEnc : public IRenderCommandEncoder, public VkCmdEncBase {
        using super = VkCmdEncBase;
        VulkanGraphicsPipeline* _currentPipeline;
        RenderPassAction _fb;

        VkRenderCmdEnc(VulkanDevice* dev,
                        VkCommandBuffer cmdList,
                        const RenderPassAction& fb );

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

        
        virtual void EndPass() override;
    };

    
    struct VkComputeCmdEnc : public IComputeCommandEncoder, public VkCmdEncBase {
        
        using super = VkCmdEncBase;
        VulkanComputePipeline* _currentPipeline;

        VkComputeCmdEnc(VulkanDevice* dev, VkCommandBuffer cmdList)
            : VkCmdEncBase{ dev, cmdList }
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

    struct VkTransferCmdEnc : public ITransferCommandEncoder, public VkCmdEncBase {
        
        using super = VkCmdEncBase;

        VkTransferCmdEnc(VulkanDevice* dev, VkCommandBuffer cmdList)
            : VkCmdEncBase{ dev, cmdList }
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



    

} // namespace alloy


