#pragma once

#include "veldrid/common/RefCnt.hpp"
#include "veldrid/common/Macros.h"
#include "veldrid/DeviceResource.hpp"
#include "veldrid/BindableResource.hpp"

#include <cstdint>

namespace Veldrid
{
    class GraphicsDevice;
    class BufferRange;
    
    class Buffer : public DeviceResource{
        friend class BufferRange;
    public:
        struct Description
        {
            
            std::uint32_t sizeInBytes;
            std::uint32_t structureByteStride;
        
            union Usage {
                struct {
                    /// <summary>
                    /// Indicates that a <see cref="DeviceBuffer"/> can be used as the source of vertex data for drawing commands.
                    /// This flag enables the use of a Buffer in the <see cref="CommandList.SetVertexBuffer(uint, DeviceBuffer)"/> method.
                    /// </summary>
                    std::uint8_t vertexBuffer : 1;
                    /// <summary>
                    /// Indicates that a <see cref="DeviceBuffer"/> can be used as the source of index data for drawing commands.
                    /// This flag enables the use of a Buffer in the <see cref="CommandList.SetIndexBuffer(DeviceBuffer, IndexFormat)" /> method.
                    /// </summary>
                    std::uint8_t indexBuffer : 1;
                    /// <summary>
                    /// Indicates that a <see cref="DeviceBuffer"/> can be used as a uniform Buffer.
                    /// This flag enables the use of a Buffer in a <see cref="ResourceSet"/> as a uniform Buffer.
                    /// </summary>
                    std::uint8_t uniformBuffer : 1;
                    /// <summary>
                    /// Indicates that a <see cref="DeviceBuffer"/> can be used as a read-only structured Buffer.
                    /// This flag enables the use of a Buffer in a <see cref="ResourceSet"/> as a read-only structured Buffer.
                    /// This flag can only be combined with <see cref="Dynamic"/>.
                    /// </summary>
                    std::uint8_t structuredBufferReadOnly : 1;
                    /// <summary>
                    /// Indicates that a <see cref="DeviceBuffer"/> can be used as a read-write structured Buffer.
                    /// This flag enables the use of a Buffer in a <see cref="ResourceSet"/> as a read-write structured Buffer.
                    /// This flag cannot be combined with any other flag.
                    /// </summary>
                    std::uint8_t structuredBufferReadWrite : 1;
                    /// <summary>
                    /// Indicates that a <see cref="DeviceBuffer"/> can be used as the source of indirect drawing information.
                    /// This flag enables the use of a Buffer in the *Indirect methods of <see cref="CommandList"/>.
                    /// This flag cannot be combined with <see cref="Dynamic"/>.
                    /// </summary>
                    std::uint8_t indirectBuffer : 1;
                    /// <summary>
                    /// Indicates that a <see cref="DeviceBuffer"/> will be updated with new data very frequently. Dynamic Buffers can be
                    /// mapped with <see cref="MapMode.Write"/>. This flag cannot be combined with <see cref="StructuredBufferReadWrite"/>
                    /// or <see cref="IndirectBuffer"/>.
                    /// </summary>
                    //std::uint8_t dynamic : 1;
                    /// <summary>
                    /// Indicates that a <see cref="DeviceBuffer"/> will be used as a staging Buffer. Staging Buffers can be used to transfer data
                    /// to-and-from the CPU using <see cref="GraphicsDevice.Map(MappableResource, MapMode)"/>. Staging Buffers can use all
                    /// <see cref="MapMode"/> values.
                    /// This flag cannot be combined with any other flag.
                    /// </summary>
                    //std::uint8_t staging : 1;
                };

                std::uint8_t value;
            } usage;

            enum class HostAccess{
                None,

                //Equivalent to DX12 READ_BACK heap
                //Typical usage: GPU write once, host read once
                PreferRead,

                //Equivalent to DX12 UPLOAD heap
                //Typical usage: Host write once, GPU read once
                PreferWrite
            } hostAccess;

            bool isRawBuffer;
        };
          

    protected:
        Buffer(
            const sp<GraphicsDevice>& dev,
            const Description& desc
        ) : 
            DeviceResource(dev),
            description(desc)
        {};

    protected:
        Description description;

    public:
        const Description& GetDesc() {return description;}

        virtual void* MapToCPU() = 0;

        virtual void UnMap() = 0;
    };


    class BufferRange : public IBindableResource{

        sp<Buffer> _buffer;
        std::uint32_t _offset;
        std::uint32_t _size;

        BufferRange(
            const sp<Buffer>& buffer,
            std::uint32_t offsetInBytes,
            std::uint32_t sizeInBytes
        )
            : _buffer(buffer)
            , _offset(offsetInBytes)
            , _size(sizeInBytes)
        {}
    public:
        ~BufferRange() override {}

        static sp<BufferRange> Make(
            const sp<Buffer>& buffer,
            std::uint32_t offsetInBytes,
            std::uint32_t sizeInBytes
        ){
            auto range = new BufferRange{ buffer, offsetInBytes, sizeInBytes};
            return sp{range};
        }

        static sp<BufferRange> Make(
            const sp<Buffer>& buffer
        ) {
            auto range = new BufferRange{ buffer, 0, buffer->GetDesc().sizeInBytes};
            return sp{ range };
        }

        virtual ResourceKind GetResourceKind() const override { return ResourceKind::UniformBuffer; }

        Buffer* GetBufferObject() const {return _buffer.get();}
        std::uint32_t GetSizeInBytes() const { return _size; }
        std::uint32_t GetOffsetInBytes() const { return _offset; }

    };

} // namespace Veldrid
