#pragma once

#include <volk.h>

#include "veldrid/common/RefCnt.hpp"
#include "veldrid/CommandList.hpp"

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

namespace Veldrid
{
    class VulkanDevice;
    class VulkanBuffer;
    class VulkanTexture;
    struct _CmdPoolContainer;

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


        std::unordered_set<sp<DeviceResource>> _res;
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

    //class VulkanPipeline;

    class VulkanCommandList : public CommandList{

        VkCommandBuffer _cmdBuf;
        sp<_CmdPoolContainer> _cmdPool;

        struct _ResSetHolder{
            bool isNewlyChanged;
            sp<VulkanResourceSet> resSet;
            std::vector<std::uint32_t> offsets;


            bool IsValid() const {return resSet != nullptr;}
        };
        std::vector<_ResSetHolder> _resourceSets;
        VulkanPipelineBase* _currentPipeline;

        struct _RenderPassInfo{
            //Pipeline resources
            sp<VulkanFramebufferBase> fb;
            //sp<VulkanPipelineBase> pipeline;
            std::vector<std::optional<VkClearColorValue>> clearColorTargets;
            std::optional<VkClearDepthStencilValue> clearDSTarget;

            //bool IsComputePass() const {
            //    return pipeline->IsComputePipeline();
            //}
        };
        //renderpasses

        std::vector<_RenderPassInfo> _rndPasses;
        _RenderPassInfo *_currentRenderPass;

        //Resources used
        std::unordered_set<sp<DeviceResource>> _devRes;

        _DevResRegistry _resReg;
        std::unordered_set<sp<DeviceResource>> _miscResReg;

        //sp<VulkanPipelineBase> _currentPipeline;
        //std::vector<sp<VulkanResourceSet>> _currentResourceSets;

        //renderpasses
        //std::set<sp<VulkanFramebuffer>> _currRenderPassFBs;

        VulkanCommandList(const sp<GraphicsDevice>& dev) : CommandList(dev){}

    public:
        ~VulkanCommandList();

        static sp<CommandList> Make(const sp<VulkanDevice>& dev);
        const VkCommandBuffer& GetHandle() const { return _cmdBuf; }
        
        virtual void Begin() override;
        virtual void End() override;

        virtual void SetPipeline(const sp<Pipeline>&) override;

        virtual void SetVertexBuffer(
            std::uint32_t index, const sp<Buffer>& buffer, std::uint32_t offset = 0) override;
    
        virtual void SetIndexBuffer(
            const sp<Buffer>& buffer, IndexFormat format, std::uint32_t offset = 0) override;

        
        virtual void SetGraphicsResourceSet(
            std::uint32_t slot, 
            const sp<ResourceSet>& rs, 
            const std::vector<std::uint32_t>& dynamicOffsets) override;
            
        virtual void SetComputeResourceSet(
            std::uint32_t slot, 
            const sp<ResourceSet>& rs, 
            const std::vector<std::uint32_t>& dynamicOffsets) override;

        virtual void BeginRenderPass(const sp<Framebuffer>& fb) override;
        virtual void EndRenderPass() override;

        virtual void ClearColorTarget(
            std::uint32_t slot, 
            float r, float g, float b, float a) override;

        virtual void ClearDepthStencil(float depth, std::uint8_t stencil) override;
    

        virtual void SetViewport(std::uint32_t index, const Viewport& viewport) override;
        virtual void SetFullViewport(std::uint32_t index) override;
        virtual void SetFullViewports() override;

        virtual void SetScissorRect(
            std::uint32_t index, 
            std::uint32_t x, std::uint32_t y, 
            std::uint32_t width, std::uint32_t height) override;
        virtual void SetFullScissorRect(std::uint32_t index) override;
        virtual void SetFullScissorRects() override;

        void _RegisterResourceSetUsage(VkPipelineBindPoint bindPoint);
        void _FlushNewResourceSets(VkPipelineBindPoint bindPoint);
        void PreDrawCommand();
        virtual void Draw(
            std::uint32_t vertexCount, std::uint32_t instanceCount,
            std::uint32_t vertexStart, std::uint32_t instanceStart) override;
        
        virtual void DrawIndexed(
            std::uint32_t indexCount, std::uint32_t instanceCount, 
            std::uint32_t indexStart, std::uint32_t vertexOffset, 
            std::uint32_t instanceStart) override;
        
        virtual void DrawIndirect(
            const sp<Buffer>& indirectBuffer, 
            std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride) override;

        virtual void DrawIndexedIndirect(
            const sp<Buffer>& indirectBuffer, 
            std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride) override;

        void PreDispatchCommand();
        virtual void Dispatch(std::uint32_t groupCountX, std::uint32_t groupCountY, std::uint32_t groupCountZ) override;

        virtual void DispatchIndirect(const sp<Buffer>& indirectBuffer, std::uint32_t offset) override;

        virtual void ResolveTexture(const sp<Texture>& source, const sp<Texture>& destination) override;

        virtual void CopyBuffer(
            const sp<Buffer>& source, std::uint32_t sourceOffset,
            const sp<Buffer>& destination, std::uint32_t destinationOffset, 
            std::uint32_t sizeInBytes) override;
                
        virtual void CopyBufferToTexture(
            const sp<Buffer>& source,
            const Texture::Description& sourceDescription,
            std::uint32_t srcX, std::uint32_t srcY, std::uint32_t srcZ,
            std::uint32_t srcMipLevel,
            std::uint32_t srcBaseArrayLayer,
            const sp<Texture>& destination,
            std::uint32_t dstX, std::uint32_t dstY, std::uint32_t dstZ,
            std::uint32_t dstMipLevel,
            std::uint32_t dstBaseArrayLayer,
            std::uint32_t width, std::uint32_t height, std::uint32_t depth,
            std::uint32_t layerCount
        ) override;

        virtual void CopyTextureToBuffer(
            const sp<Texture>& source,
            std::uint32_t srcX, std::uint32_t srcY, std::uint32_t srcZ,
            std::uint32_t srcMipLevel,
            std::uint32_t srcBaseArrayLayer,
            const sp<Buffer>& destination,
            const Texture::Description& destinationDescription,
            std::uint32_t dstX, std::uint32_t dstY, std::uint32_t dstZ,
            std::uint32_t dstMipLevel,
            std::uint32_t dstBaseArrayLayer,
            std::uint32_t width, std::uint32_t height, std::uint32_t depth,
            std::uint32_t layerCount
        ) override;

        virtual void CopyTexture(
            const sp<Texture>& source,
            std::uint32_t srcX, std::uint32_t srcY, std::uint32_t srcZ,
            std::uint32_t srcMipLevel,
            std::uint32_t srcBaseArrayLayer,
            const sp<Texture>& destination,
            std::uint32_t dstX, std::uint32_t dstY, std::uint32_t dstZ,
            std::uint32_t dstMipLevel,
            std::uint32_t dstBaseArrayLayer,
            std::uint32_t width, std::uint32_t height, std::uint32_t depth,
            std::uint32_t layerCount) override;

        virtual void GenerateMipmaps(const sp<Texture>& texture) override;

        virtual void PushDebugGroup(const std::string& name) override;

        virtual void PopDebugGroup() override;

        virtual void InsertDebugMarker(const std::string& name) override;

        
        virtual void Barrier(const alloy::BarrierDescription&) override;

    };
    

} // namespace Veldrid


