#pragma once

#include "alloy/common/RefCnt.hpp"
#include "alloy/BindableResource.hpp"
#include "alloy/CommandList.hpp"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <unordered_set>
#include <unordered_map>

namespace alloy::mtl{

class MetalDevice;
class MetalCmdQ;
class MetalCommandList;
class MetalGfxPipeline;
class MetalFrameBufferBase;


class CmdEncoderImplBase {
    
protected:
    std::unordered_set<common::sp<common::RefCntBase>> resources;

    //T* _Self() const {return static_cast<T*>(this);}

    MetalCommandList* _cmdBuf;

    CmdEncoderImplBase(MetalCommandList* cmdBuf) : _cmdBuf(cmdBuf){}
    

    //virtual id<MTLCommandEncoder> GetHandle() = 0;
    
public:
    virtual ~CmdEncoderImplBase() {};
    
    virtual void EndPass() {}

  //void RegisterObjInUse(const common::sp<common::RefCntBase> &obj);
  // void WaitForFence(const common::sp<IFence>& fence);
  // void UpdateFence(const common::sp<IFence>& fence);
};

/*
class BaseCommandEncoderImpl : public CmdEncoderImplBase, public IBaseCommandEncoder {
    using Super = CmdEncoderImplBase;

    id<MTLCommandEncoder> _mtlEnc;

public:
    virtual id<MTLCommandEncoder> GetHandle() override {return _mtlEnc;}

    BaseCommandEncoderImpl(MetalCommandList* cmdBuf) : CmdEncoderImplBase(cmdBuf) {}
    
    virtual ~BaseCommandEncoderImpl() override;
    
    
    virtual void EndEncoding() override {
        assert(_mtlEnc != nullptr);
        [_mtlEnc endEncoding];
        [_mtlEnc release];
        _mtlEnc = nullptr;
    }
    
    
    
    virtual void PushDebugGroup(const std::string& label) override;
    virtual void PopDebugGropu() override;

};
*/

class MetalRenderCmdEnc : public IRenderCommandEncoder, public CmdEncoderImplBase {
    
    id<MTLRenderCommandEncoder> _mtlEnc;

    //MetalGfxPipeline* _currentPipeline;

    //Metal framebuffers won't 
    RenderPassAction _actions;
    
    struct RenderPassRegistry{
        common::sp<MetalGfxPipeline> boundPipeline;
        std::unordered_map<std::uint32_t, common::sp<BufferRange> > boundVertexBuffers;
        common::sp<BufferRange> boundIndexBuffer; IndexFormat boundIndexBufferFormat;
        //common::sp<IResourceSet> boundArgBuffers[31];
        std::vector<uint8_t> argBuffer;
    };

    ///#TODO: Confirm can we have multiple draw calls in single render pass
    RenderPassRegistry _drawResources;
    
public:


    //id<MTLCommandEncoder> GetHandle() override {return _mtlEnc;}
    
    MetalRenderCmdEnc(
        
        MetalCommandList* cmdBuf,
        id<MTLRenderCommandEncoder>  enc,
        const RenderPassAction& action
        //RenderPassRegistry& registry
    ) ;

    virtual ~MetalRenderCmdEnc() override;

    virtual void EndPass() override {
        assert(_mtlEnc != nullptr);
        @autoreleasepool {
            [_mtlEnc endEncoding];
            [_mtlEnc release];
        }
        _mtlEnc = nullptr;
    }
    
    virtual void SetPipeline(const common::sp<IGfxPipeline>&) override;

    // Sets the active <see cref="DeviceBuffer"/> for the given index.
    // When drawing, the bound <see cref="DeviceBuffer"/> objects must be compatible with the bound <see cref="Pipeline"/>.
    // The given buffer must be non-null. It is not necessary to un-bind vertex buffers for Pipelines which will not
    // use them. All extra vertex buffers are simply ignored.
    virtual void SetVertexBuffer(
        std::uint32_t index, const common::sp<BufferRange>& buffer) override;

    // Sets the active <see cref="DeviceBuffer"/>.{}
    // When drawing, an <see cref="DeviceBuffer"/> must be bound.
    virtual void SetIndexBuffer(
        const common::sp<BufferRange>& buffer, IndexFormat format) override;

    
    // Sets the active <see cref="ResourceSet"/> for the given index. This ResourceSet is only active for the graphics
    // Pipeline.
    // <param name="slot">The resource slot.</param>
    // <param name="rs">The new <see cref="ResourceSet"/>.</param>
    // <param name="dynamicOffsets">An array containing the offsets to apply to the dynamic
    // buffers contained in the <see cref="ResourceSet"/>. The number of elements in this array must be equal to the number
    // of dynamic buffers (<see cref="ResourceLayoutElementOptions.DynamicBinding"/>) contained in the
    // <see cref="ResourceSet"/>. These offsets are applied in the order that dynamic buffer
    // elements appear in the <see cref="ResourceSet"/>. Each of these offsets must be a multiple of either
    // <see cref="GraphicsDevice.UniformBufferMinOffsetAlignment"/> or
    // <see cref="GraphicsDevice.StructuredBufferMinOffsetAlignment"/>, depending on the kind of resource.</param>
    virtual void SetGraphicsResourceSet(
        //std::uint32_t slot,
        const common::sp<IResourceSet>& rs
        //const std::vector<std::uint32_t>& dynamicOffsets
        ) override;
    
    
    virtual void SetPushConstants(
        std::uint32_t pushConstantIndex,
        const std::span<uint32_t>& data,
        std::uint32_t destOffsetIn32BitValues) override;

    //Subsituted by BeginWithRenderEncoder and EndEncoding
    //virtual void BeginRenderPass(const sp<Framebuffer>& fb) = 0;
    //virtual void EndRenderPass() = 0;

    //Subsituted by load actions of renderpass
    // Clears the color target at the given index of the active <see cref="Framebuffer"/>.
    // The index given must be less than the number of color attachments in the active <see cref="Framebuffer"/>.
    //virtual void ClearColorTarget(
    //    std::uint32_t slot,
    //    float r, float g, float b, float a) = 0;

    // Clears the depth-stencil target of the active <see cref="Framebuffer"/>.
    // The active <see cref="Framebuffer"/> must have a depth attachment.
    // <param name="depth">The value to clear the depth buffer to.</param>
    // <param name="stencil">The value to clear the stencil buffer to.</param>
    //virtual void ClearDepthStencil(float depth, std::uint8_t stencil = 0) = 0;


    // Sets the active <see cref="Viewport"/> at the given index.
    // The index given must be less than the number of color attachments in the active <see cref="Framebuffer"/>.
    // <param name="index">The color target index.</param>
    // <param name="viewport">The new <see cref="Viewport"/>.</param>
    virtual void SetViewports(const std::span<Viewport>& viewport) override;
    virtual void SetFullViewports() override;
    // This at least should be inside a renderpass, therefore a framebuffer exists,
    // then we can get fb sizes
    //virtual void SetFullViewport(std::uint32_t index) = 0;
    //virtual void SetFullViewports() = 0;

    // Sets the active scissor rectangle at the given index.
    // The index given must be less than the number of color attachments in the active <see cref="Framebuffer"/>.
    // <param name="index">The color target index.</param>
    // <param name="x">The X value of the scissor rectangle.</param>
    // <param name="y">The Y value of the scissor rectangle.</param>
    // <param name="width">The width of the scissor rectangle.</param>
    // <param name="height">The height of the scissor rectangle.</param>
    virtual void SetScissorRects(const std::span<Rect>&  rects) override;
    virtual void SetFullScissorRects() override;
    // This at least should be inside a renderpass, therefore a framebuffer exists,
    // then we can get fb sizes
    //virtual void SetFullScissorRect(std::uint32_t index) = 0;
    //virtual void SetFullScissorRects() = 0;

    // Draws primitives from the currently-bound state in this CommandList.
    // An index Buffer is not used.
    // <param name="vertexCount">The number of vertices.</param>
    // <param name="instanceCount">The number of instances.</param>
    // <param name="vertexStart">The first vertex to use when drawing.</param>
    // <param name="instanceStart">The starting instance value.</param>
    virtual void Draw(
        std::uint32_t vertexCount, std::uint32_t instanceCount,
        std::uint32_t vertexStart, std::uint32_t instanceStart) override;
    void Draw(std::uint32_t vertexCount){Draw(vertexCount, 1, 0, 0);}
    
    // Draws indexed primitives from the currently-bound state in this <see cref="CommandList"/>.
    // <param name="indexCount">The number of indices.</param>
    // <param name="instanceCount">The number of instances.</param>
    // <param name="indexStart">The number of indices to skip in the active index buffer.</param>
    // <param name="vertexOffset">The base vertex value, which is added to each index value read from the index buffer.</param>
    // <param name="instanceStart">The starting instance value.</param>
    virtual void DrawIndexed(
        std::uint32_t indexCount, std::uint32_t instanceCount,
        std::uint32_t indexStart, std::uint32_t vertexOffset,
        std::uint32_t instanceStart) override;
    void DrawIndexed(std::uint32_t indexCount){DrawIndexed(indexCount, 1, 0, 0, 0);}
    
    // Issues indirect draw commands based on the information contained in the given indirect <see cref="DeviceBuffer"/>.
    // The information stored in the indirect Buffer should conform to the structure of <see cref="IndirectDrawArguments"/>.
    // <param name="indirectBuffer">The indirect Buffer to read from. Must have been created with the
    // <see cref="BufferUsage.IndirectBuffer"/> flag.</param>
    // <param name="offset">An offset, in bytes, from the start of the indirect buffer from which the draw commands will be
    // read. This value must be a multiple of 4.</param>
    // <param name="drawCount">The number of draw commands to read and issue from the indirect Buffer.</param>
    // <param name="stride">The stride, in bytes, between consecutive draw commands in the indirect Buffer. This value must
    // be a multiple of four, and must be larger than the size of <see cref="IndirectDrawArguments"/>.</param>
    //virtual void DrawIndirect(
    //    const common::sp<BufferRange>& indirectBuffer,
    //    std::uint32_t drawCount, std::uint32_t stride) = 0;

    // Issues indirect, indexed draw commands based on the information contained in the given indirect <see cref="DeviceBuffer"/>.
    // The information stored in the indirect Buffer should conform to the structure of
    // <see cref="IndirectDrawIndexedArguments"/>.
    // <param name="indirectBuffer">The indirect Buffer to read from. Must have been created with the
    // <see cref="BufferUsage.IndirectBuffer"/> flag.</param>
    // <param name="offset">An offset, in bytes, from the start of the indirect buffer from which the draw commands will be
    // read. This value must be a multiple of 4.</param>
    // <param name="drawCount">The number of draw commands to read and issue from the indirect Buffer.</param>
    // <param name="stride">The stride, in bytes, between consecutive draw commands in the indirect Buffer. This value must
    // be a multiple of four, and must be larger than the size of <see cref="IndirectDrawIndexedArguments"/>.</param>
    //virtual void DrawIndexedIndirect(
    //    const common::sp<BufferRange>& indirectBuffer,
    //    std::uint32_t drawCount, std::uint32_t stride) = 0;


    virtual void WaitForFenceBeforeStages(const common::sp<IFence>&, const PipelineStages&) override;
    virtual void UpdateFenceAfterStages(const common::sp<IFence>&, const PipelineStages&) override;

};


    class MetalComputeCmdEnc : public IComputeCommandEncoder, public CmdEncoderImplBase {
        id<MTLComputeCommandEncoder> _mtlEnc;
    
    public:
        
        MetalComputeCmdEnc(
            MetalCommandList* cmdBuf,
            id<MTLComputeCommandEncoder>  enc
        )
            : CmdEncoderImplBase(cmdBuf)
            , _mtlEnc(enc)
        {};
        
        virtual ~MetalComputeCmdEnc() {}
        
        virtual void EndPass() override {}
        
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
        virtual void Dispatch(std::uint32_t groupCountX, std::uint32_t groupCountY, std::uint32_t groupCountZ) override;

        ///:TODO: Needs further confirmation about indirect commands
        #if 0
        /// Issues an indirect compute dispatch command based on the information contained in the given indirect
        /// <see cref="DeviceBuffer"/>. The information stored in the indirect Buffer should conform to the structure of
        /// <see cref="IndirectDispatchArguments"/>.
        /// <param name="indirectBuffer">The indirect Buffer to read from. Must have been created with the
        /// <see cref="BufferUsage.IndirectBuffer"/> flag.</param>
        /// <param name="offset">An offset, in bytes, from the start of the indirect buffer from which the draw commands will be
        /// read. This value must be a multiple of 4.</param>
        virtual void DispatchIndirect(const sp<Buffer>& indirectBuffer, std::uint32_t offset) = 0;
        #endif

        
        virtual void WaitForFenceBeforeStages(const common::sp<IFence>&, const PipelineStages&) override {}
        virtual void UpdateFenceAfterStages(const common::sp<IFence>&, const PipelineStages&) override {}

    };


    class MetalTransferCmdEnc : public CmdEncoderImplBase, public ITransferCommandEncoder {

        id<MTLBlitCommandEncoder> _mtlEnc;

    public:

        MetalTransferCmdEnc(
            MetalCommandList* cmdBuf,
            id<MTLBlitCommandEncoder> enc
        ) : CmdEncoderImplBase(cmdBuf)
          , _mtlEnc(enc)
        {}

        virtual ~MetalTransferCmdEnc() override {}

        virtual void EndPass() override {
            assert(_mtlEnc != nullptr);
            @autoreleasepool {
                [_mtlEnc endEncoding];
                [_mtlEnc release];
            }
            _mtlEnc = nullptr;
        }

        /// <summary>
        /// Copies a region from the source <see cref="DeviceBuffer"/> to another region in the destination <see cref="DeviceBuffer"/>.
        /// </summary>
        /// <param name="source">The source <see cref="DeviceBuffer"/> from which data will be copied.</param>
        /// <param name="sourceOffset">An offset into <paramref name="source"/> at which the copy region begins.</param>
        /// <param name="destination">The destination <see cref="DeviceBuffer"/> into which data will be copied.</param>
        /// <param name="destinationOffset">An offset into <paramref name="destination"/> at which the data will be copied.
        /// </param>
        /// <param name="sizeInBytes">The number of bytes to copy.</param>
        virtual void CopyBuffer(
            const common::sp<BufferRange>& source,
            const common::sp<BufferRange>& destination,
            std::uint32_t sizeInBytes) override;
                
        //TODO: should we adjust to resource views?
        virtual void CopyBufferToTexture(
            const common::sp<BufferRange>& source,
            std::uint32_t sourceBytesPerRow,
            std::uint32_t sourceBytesPerImage,
            const common::sp<ITexture>& destination,
            const Point3D& dstOrigin,
            std::uint32_t dstMipLevel,
            std::uint32_t dstBaseArrayLayer,
            const Size3D& copySize
        ) override;

        virtual void CopyTextureToBuffer(
            const common::sp<ITexture>& source,
            const Point3D& srcOrigin,
            std::uint32_t srcMipLevel,
            std::uint32_t srcBaseArrayLayer,
            const common::sp<BufferRange>& destination,
            std::uint32_t dstBytesPerRow,
            std::uint32_t dstBytesPerImage,
            const Size3D& copySize
        ) override;
        /// Copies a region from one <see cref="Texture"/> into another.
        /// <param name="source">The source <see cref="Texture"/> from which data is copied.</param>
        /// <param name="srcX">The X coordinate of the source copy region.</param>
        /// <param name="srcY">The Y coordinate of the source copy region.</param>
        /// <param name="srcZ">The Z coordinate of the source copy region.</param>
        /// <param name="srcMipLevel">The mip level to copy from the source Texture.</param>
        /// <param name="srcBaseArrayLayer">The starting array layer to copy from the source Texture.</param>
        /// <param name="destination">The destination <see cref="Texture"/> into which data is copied.</param>
        /// <param name="dstX">The X coordinate of the destination copy region.</param>
        /// <param name="dstY">The Y coordinate of the destination copy region.</param>
        /// <param name="dstZ">The Z coordinate of the destination copy region.</param>
        /// <param name="dstMipLevel">The mip level to copy the data into.</param>
        /// <param name="dstBaseArrayLayer">The starting array layer to copy data into.</param>
        /// <param name="width">The width in texels of the copy region.</param>
        /// <param name="height">The height in texels of the copy region.</param>
        /// <param name="depth">The depth in texels of the copy region.</param>
        /// <param name="layerCount">The number of array layers to copy.</param>
        virtual void CopyTexture(
            const common::sp<ITexture>& source,
            const Point3D& srcOrigin,
            std::uint32_t srcMipLevel,
            std::uint32_t srcBaseArrayLayer,
            const common::sp<ITexture>& destination,
            const Point3D& dstOrigin,
            std::uint32_t dstMipLevel,
            std::uint32_t dstBaseArrayLayer,
            const Size3D& copySize) override;

        /// <summary>
        /// Copies all subresources from one <see cref="Texture"/> to another.
        /// </summary>
        /// <param name="source">The source of Texture data.</param>
        /// <param name="destination">The destination of Texture data.</param>
/*        void CopyTexture(const common::sp<Texture>& source, const common::sp<Texture>& destination)
        {
            auto& desc = source->GetDesc();

            std::uint32_t effectiveSrcArrayLayers = (desc.usage.cubemap) != 0
                ? desc.arrayLayers * 6
                : desc.arrayLayers;
#if VALIDATE_USAGE
            std::uint32_t effectiveDstArrayLayers = (destination.Usage & TextureUsage.Cubemap) != 0
                ? destination.ArrayLayers * 6
                : destination.ArrayLayers;
            if (effectiveSrcArrayLayers != effectiveDstArrayLayers || source.MipLevels != destination.MipLevels
                || source.SampleCount != destination.SampleCount || source.Width != destination.Width
                || source.Height != destination.Height || source.Depth != destination.Depth
                || source.Format != destination.Format)
            {
                throw new VeldridException("Source and destination Textures are not compatible to be copied.");
            }
#endif

            for (std::uint32_t level = 0; level < source->GetDesc().mipLevels; level++)
            {
                std::uint32_t mipWidth, mipHeight, mipDepth;
                
                Helpers::GetMipDimensions(source->GetDesc(), level, mipWidth, mipHeight, mipDepth);
                CopyTexture(
                    source, 0, 0, 0, level, 0,
                    destination, 0, 0, 0, level, 0,
                    mipWidth, mipHeight, mipDepth,
                    effectiveSrcArrayLayers);
            }
        }*/

/*
        void CopyTexture(
            const common::sp<Texture>& source, const common::sp<Texture>& destination,
            std::uint32_t mipLevel, std::uint32_t arrayLayer)
        {
#if VALIDATE_USAGE
            std::uint32_t effectiveSrcArrayLayers = (source.Usage & TextureUsage.Cubemap) != 0
                ? source.ArrayLayers * 6
                : source.ArrayLayers;
            std::uint32_t effectiveDstArrayLayers = (destination.Usage & TextureUsage.Cubemap) != 0
                ? destination.ArrayLayers * 6
                : destination.ArrayLayers;
            if (source.SampleCount != destination.SampleCount || source.Width != destination.Width
                || source.Height != destination.Height || source.Depth != destination.Depth
                || source.Format != destination.Format)
            {
                throw new VeldridException("Source and destination Textures are not compatible to be copied.");
            }
            if (mipLevel >= source.MipLevels || mipLevel >= destination.MipLevels || arrayLayer >= effectiveSrcArrayLayers || arrayLayer >= effectiveDstArrayLayers)
            {
                throw new VeldridException(
                    $"{nameof(mipLevel)} and {nameof(arrayLayer)} must be less than the given Textures' mip level count and array layer count.");
            }
#endif
            std::uint32_t width, height, depth;
            Helpers::GetMipDimensions(source->GetDesc(), mipLevel, width, height, depth);
            CopyTexture(
                source, 0, 0, 0, mipLevel, arrayLayer,
                destination, 0, 0, 0, mipLevel, arrayLayer,
                width, height, depth,
                1);
        }*/

        // Generates mipmaps for the given <see cref="Texture"/>. The largest mipmap is used to generate all of the lower mipmap
        // levels contained in the Texture. The previous contents of all lower mipmap levels are overwritten by this operation.
        // The target Texture must have been created with <see cref="TextureUsage"/>.<see cref="TextureUsage.GenerateMipmaps"/>.
        // <param name="texture">The <see cref="Texture"/> to generate mipmaps for. This Texture must have been created with
        // <see cref="TextureUsage"/>.<see cref="TextureUsage.GenerateMipmaps"/>.</param>
        virtual void GenerateMipmaps(const common::sp<ITexture>& texture) override;
    
        
        /// Resolves a multisampled source <see cref="Texture"/> into a non-multisampled destination <see cref="Texture"/>.
        /// <param name="source">The source of the resolve operation. Must be a multisampled <see cref="Texture"/>
        /// (<see cref="Texture.SampleCount"/> > 1).</param>
        /// <param name="destination">The destination of the resolve operation. Must be a non-multisampled <see cref="Texture"/>
        /// (<see cref="Texture.SampleCount"/> == 1).</param>
        virtual void ResolveTexture(const common::sp<ITexture>& source, const common::sp<ITexture>& destination) override;
        
        
        virtual void WaitForFence(const common::sp<IFence>&) override {}
        virtual void UpdateFence(const common::sp<IFence>&) override {}

    };



class MetalCommandList : public ICommandList{
    
    common::sp<MetalDevice> _dev;
    id<MTLCommandBuffer> _cmdBuf;
    
    CmdEncoderImplBase* _currEncoder;
    
    std::vector<CmdEncoderImplBase*> _passes;
    std::unordered_set<common::sp<common::RefCntBase>> _objsInUse;
    
public:
    MetalCommandList(const common::sp<MetalDevice>&,
                     id<MTLCommandBuffer>);
    
    virtual ~MetalCommandList() override;


    void RegisterObjInUse(const common::sp<common::RefCntBase>& obj) {_objsInUse.emplace(obj);}
    
    virtual void Begin() override {}
    virtual void End() override {}
    
    virtual IRenderCommandEncoder&   BeginRenderPass(const RenderPassAction&) override;
    virtual IComputeCommandEncoder&  BeginComputePass() override;
    virtual ITransferCommandEncoder& BeginTransferPass() override;
    //virtual IBaseCommandEncoder*     BeginWithBasicEncoder() override;

    virtual void EndPass() override {
        assert(_currEncoder != nullptr);
        _currEncoder->EndPass();
        _currEncoder = nullptr;
    }
    

    //virtual void BeginRenderPass(const sp<Framebuffer>& fb) = 0;
    //virtual void EndRenderPass() = 0;

    virtual void Barrier(const std::vector<alloy::BarrierDescription>& barriers) override {}
    virtual void TransitionTextureToDefaultLayout(
        const std::vector<common::sp<ITexture>>& textures
    ) override {}

    // Pushes a debug group at the current position in the <see cref="CommandList"/>. This allows subsequent commands to be
    // categorized and filtered when viewed in external debugging tools. This method can be called multiple times in order
    // to create nested debug groupings. Each call to PushDebugGroup must be followed by a matching call to
    // <see cref="PopDebugGroup"/>.
    // <param name="name">The name of the group. This is an opaque identifier used for display by graphics debuggers.</param>
    virtual void PushDebugGroup(const std::string& name) override {}

    // Pops the current debug group. This method must only be called after <see cref="PushDebugGroup(string)"/> has been
    // called on this instance.
    virtual void PopDebugGroup() override {}

    // Inserts a debug marker into the CommandList at the current position. This is used by graphics debuggers to identify
    // points of interest in a command stream.
    // <param name="name">The name of the marker. This is an opaque identifier used for display by graphics debuggers.</param>
    virtual void InsertDebugMarker(const std::string& name) override {}

    //virtual void EncodeWaitForEvent(const common::sp<IEvent>& event, uint64_t expectedValue) override;
    //virtual void EncodeSignalEvent(const common::sp<IEvent>& event, uint64_t value) override;
    

    //virtual void Present(const common::sp<RenderTarget>& ) override;
    
    id<MTLCommandBuffer> GetHandle() const {return _cmdBuf;}
};

}
