#pragma once

#include "veldrid/Helpers.hpp"
#include "veldrid/DeviceResource.hpp"
//#include "veldrid/Pipeline.hpp"
//#include "veldrid/Buffer.hpp"
//#include "veldrid/BindableResource.hpp"
//#include "veldrid/Framebuffer.hpp"
#include "veldrid/Types.hpp"
#include "veldrid/ResourceBarrier.hpp"

#include <cstdint>
#include <vector>
#include <type_traits>
#include <variant>

namespace Veldrid
{
    class Buffer;
    class Pipeline;
    class FrameBuffer;
    class ResourceLayout;
    class ResourceSet;
    
    class CommandList : public DeviceResource{

    protected:
        CommandList(const sp<GraphicsDevice>& dev) : DeviceResource(dev){}

    public:

        virtual void Begin() = 0;
        virtual void End() = 0;

        virtual void SetPipeline(const sp<Pipeline>&) = 0;

        // Sets the active <see cref="DeviceBuffer"/> for the given index.
        // When drawing, the bound <see cref="DeviceBuffer"/> objects must be compatible with the bound <see cref="Pipeline"/>.
        // The given buffer must be non-null. It is not necessary to un-bind vertex buffers for Pipelines which will not
        // use them. All extra vertex buffers are simply ignored.
        virtual void SetVertexBuffer(
            std::uint32_t index, const sp<Buffer>& buffer, std::uint32_t offset = 0) = 0;
    
        // Sets the active <see cref="DeviceBuffer"/>.
        // When drawing, an <see cref="DeviceBuffer"/> must be bound.
        virtual void SetIndexBuffer(
            const sp<Buffer>& buffer, IndexFormat format, std::uint32_t offset = 0) = 0;

        
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
            std::uint32_t slot, 
            const sp<ResourceSet>& rs, 
            const std::vector<std::uint32_t>& dynamicOffsets) = 0;
            
        virtual void SetComputeResourceSet(
            std::uint32_t slot, 
            const sp<ResourceSet>& rs, 
            const std::vector<std::uint32_t>& dynamicOffsets) = 0;

        virtual void BeginRenderPass(const sp<Framebuffer>& fb) = 0;
        virtual void EndRenderPass() = 0;

        // Clears the color target at the given index of the active <see cref="Framebuffer"/>.
        // The index given must be less than the number of color attachments in the active <see cref="Framebuffer"/>.
        virtual void ClearColorTarget(
            std::uint32_t slot, 
            float r, float g, float b, float a) = 0;

        // Clears the depth-stencil target of the active <see cref="Framebuffer"/>.
        // The active <see cref="Framebuffer"/> must have a depth attachment.
        // <param name="depth">The value to clear the depth buffer to.</param>
        // <param name="stencil">The value to clear the stencil buffer to.</param>
        virtual void ClearDepthStencil(float depth, std::uint8_t stencil = 0) = 0;
    

        // Sets the active <see cref="Viewport"/> at the given index.
        // The index given must be less than the number of color attachments in the active <see cref="Framebuffer"/>.
        // <param name="index">The color target index.</param>
        // <param name="viewport">The new <see cref="Viewport"/>.</param>
        virtual void SetViewport(std::uint32_t index, const Viewport& viewport) = 0;
        // This at least should be inside a renderpass, therefore a framebuffer exists, 
        // then we can get fb sizes
        virtual void SetFullViewport(std::uint32_t index) = 0;
        virtual void SetFullViewports() = 0;

        // Sets the active scissor rectangle at the given index.
        // The index given must be less than the number of color attachments in the active <see cref="Framebuffer"/>.
        // <param name="index">The color target index.</param>
        // <param name="x">The X value of the scissor rectangle.</param>
        // <param name="y">The Y value of the scissor rectangle.</param>
        // <param name="width">The width of the scissor rectangle.</param>
        // <param name="height">The height of the scissor rectangle.</param>
        virtual void SetScissorRect(
            std::uint32_t index, 
            std::uint32_t x, std::uint32_t y, 
            std::uint32_t width, std::uint32_t height) = 0;
        // This at least should be inside a renderpass, therefore a framebuffer exists, 
        // then we can get fb sizes
        virtual void SetFullScissorRect(std::uint32_t index) = 0;
        virtual void SetFullScissorRects() = 0;

        // Draws primitives from the currently-bound state in this CommandList. 
        // An index Buffer is not used.
        // <param name="vertexCount">The number of vertices.</param>
        // <param name="instanceCount">The number of instances.</param>
        // <param name="vertexStart">The first vertex to use when drawing.</param>
        // <param name="instanceStart">The starting instance value.</param>
        virtual void Draw(
            std::uint32_t vertexCount, std::uint32_t instanceCount,
            std::uint32_t vertexStart, std::uint32_t instanceStart) = 0;
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
            std::uint32_t instanceStart) = 0;
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
        virtual void DrawIndirect(
            const sp<Buffer>& indirectBuffer, 
            std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride) = 0;

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
        virtual void DrawIndexedIndirect(
            const sp<Buffer>& indirectBuffer, 
            std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride) = 0;

        /// <summary>
        /// Dispatches a compute operation from the currently-bound compute state of this Pipeline.
        /// </summary>
        /// <param name="groupCountX">The X dimension of the compute thread groups that are dispatched.</param>
        /// <param name="groupCountY">The Y dimension of the compute thread groups that are dispatched.</param>
        /// <param name="groupCountZ">The Z dimension of the compute thread groups that are dispatched.</param>
        virtual void Dispatch(std::uint32_t groupCountX, std::uint32_t groupCountY, std::uint32_t groupCountZ) = 0;

        /// Issues an indirect compute dispatch command based on the information contained in the given indirect
        /// <see cref="DeviceBuffer"/>. The information stored in the indirect Buffer should conform to the structure of
        /// <see cref="IndirectDispatchArguments"/>.
        /// <param name="indirectBuffer">The indirect Buffer to read from. Must have been created with the
        /// <see cref="BufferUsage.IndirectBuffer"/> flag.</param>
        /// <param name="offset">An offset, in bytes, from the start of the indirect buffer from which the draw commands will be
        /// read. This value must be a multiple of 4.</param>
        virtual void DispatchIndirect(const sp<Buffer>& indirectBuffer, std::uint32_t offset) = 0;

        /// Resolves a multisampled source <see cref="Texture"/> into a non-multisampled destination <see cref="Texture"/>.
        /// <param name="source">The source of the resolve operation. Must be a multisampled <see cref="Texture"/>
        /// (<see cref="Texture.SampleCount"/> > 1).</param>
        /// <param name="destination">The destination of the resolve operation. Must be a non-multisampled <see cref="Texture"/>
        /// (<see cref="Texture.SampleCount"/> == 1).</param>
        virtual void ResolveTexture(const sp<Texture>& source, const sp<Texture>& destination) = 0;

        /// Updates a <see cref="DeviceBuffer"/> region with new data.
        /// <param name="buffer">The resource to update.</param>
        /// <param name="bufferOffsetInBytes">An offset, in bytes, from the beginning of the <see cref="DeviceBuffer"/>'s storage, at
        /// which new data will be uploaded.</param>
        /// <param name="source">A pointer to the start of the data to upload.</param>
        /// <param name="sizeInBytes">The total size of the uploaded data, in bytes.</param>
        //virtual void UpdateBuffer(
        //    const sp<Buffer>& buffer,
        //    std::uint32_t bufferOffsetInBytes,
        //    void* source,
        //    std::uint32_t sizeInBytes) = 0;
        //
        //template<typename T>
        //void UpdateBuffer(
        //    const sp<Buffer>& buffer,
        //    std::uint32_t bufferOffsetInBytes,
        //    const T& source
        //){
        //    UpdateBuffer(buffer, bufferOffsetInBytes,
        //        &source, sizeof(source));
        //}
        //
        //template<typename T>
        //void UpdateBuffer(
        //    const sp<Buffer>& buffer,
        //    std::uint32_t bufferOffsetInBytes,
        //    const std::vector<T>& source
        //){
        //    auto totalSize = sizeof(T) * source.size();
        //    UpdateBuffer(buffer, bufferOffsetInBytes,
        //        source.data(), totalSize);
        //}

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
            const sp<Buffer>& source, std::uint32_t sourceOffset,
            const sp<Buffer>& destination, std::uint32_t destinationOffset, 
            std::uint32_t sizeInBytes) = 0;
                

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
        ) = 0;

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
        ) = 0;
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
            const sp<Texture>& source,
            std::uint32_t srcX, std::uint32_t srcY, std::uint32_t srcZ,
            std::uint32_t srcMipLevel,
            std::uint32_t srcBaseArrayLayer,
            const sp<Texture>& destination,
            std::uint32_t dstX, std::uint32_t dstY, std::uint32_t dstZ,
            std::uint32_t dstMipLevel,
            std::uint32_t dstBaseArrayLayer,
            std::uint32_t width, std::uint32_t height, std::uint32_t depth,
            std::uint32_t layerCount) = 0;

        /// <summary>
        /// Copies all subresources from one <see cref="Texture"/> to another.
        /// </summary>
        /// <param name="source">The source of Texture data.</param>
        /// <param name="destination">The destination of Texture data.</param>
        void CopyTexture(const sp<Texture>& source, const sp<Texture>& destination)
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
        }


        void CopyTexture(
            const sp<Texture>& source, const sp<Texture>& destination,
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
        }

        // Generates mipmaps for the given <see cref="Texture"/>. The largest mipmap is used to generate all of the lower mipmap
        // levels contained in the Texture. The previous contents of all lower mipmap levels are overwritten by this operation.
        // The target Texture must have been created with <see cref="TextureUsage"/>.<see cref="TextureUsage.GenerateMipmaps"/>.
        // <param name="texture">The <see cref="Texture"/> to generate mipmaps for. This Texture must have been created with
        // <see cref="TextureUsage"/>.<see cref="TextureUsage.GenerateMipmaps"/>.</param>
        virtual void GenerateMipmaps(const sp<Texture>& texture) = 0;

        
        virtual void Barrier(const alloy::BarrierDescription&) = 0;

        // Pushes a debug group at the current position in the <see cref="CommandList"/>. This allows subsequent commands to be
        // categorized and filtered when viewed in external debugging tools. This method can be called multiple times in order
        // to create nested debug groupings. Each call to PushDebugGroup must be followed by a matching call to
        // <see cref="PopDebugGroup"/>.
        // <param name="name">The name of the group. This is an opaque identifier used for display by graphics debuggers.</param>
        virtual void PushDebugGroup(const std::string& name) = 0;

        // Pops the current debug group. This method must only be called after <see cref="PushDebugGroup(string)"/> has been
        // called on this instance.
        virtual void PopDebugGroup() = 0;

        // Inserts a debug marker into the CommandList at the current position. This is used by graphics debuggers to identify
        // points of interest in a command stream.
        // <param name="name">The name of the marker. This is an opaque identifier used for display by graphics debuggers.</param>
        virtual void InsertDebugMarker(const std::string& name) = 0;
    };

} // namespace Veldrid


