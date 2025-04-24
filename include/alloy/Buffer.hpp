#pragma once

#include "alloy/common/RefCnt.hpp"
#include "alloy/common/Macros.h"
#include "alloy/BindableResource.hpp"
#include "alloy/Types.hpp"

#include <cstdint>

namespace alloy
{
    class IGraphicsDevice;
    class BufferRange;
    
    class IBuffer : public common::RefCntBase {
        friend class BufferRange;
    public:
        struct Description
        {
            
            std::uint32_t sizeInBytes;
            std::uint32_t structureByteStride;
        
            struct Usage {
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
                
            } usage;

            HostAccess hostAccess;

            bool isRawBuffer;
        };
          

    protected:
        IBuffer( const Description& desc ) : description(desc) { };

    protected:
        Description description;

    public:
        const Description& GetDesc() {return description;}

        virtual void* MapToCPU() = 0;

        virtual void UnMap() = 0;

        virtual uint64_t GetNativeHandle() const {return 0;}
        
        virtual void SetDebugName(const std::string& ) = 0;
    };


    class BufferRange : public IBindableResource{

    public:
        struct Shape {
            std::uint64_t offsetInElements;
            std::uint64_t elementCount;
            std::uint32_t elementSizeInBytes;

            std::uint64_t GetOffsetInBytes() const {return offsetInElements * elementSizeInBytes;}
            std::uint64_t GetSizeInBytes() const {return elementCount * elementSizeInBytes;}
        };
    private:

        common::sp<IBuffer> _buffer;
        Shape _shape;
        ResourceKind _kind;

        BufferRange(
            const common::sp<IBuffer>& buffer,
            const Shape& shape,
            ResourceKind kind
        )
            : _buffer(buffer)
            , _shape(shape)
            , _kind(kind)
        {}
    public:
        ~BufferRange() override {}

        static common::sp<BufferRange> MakeByteBuffer(
            const common::sp<IBuffer>& buffer,
            std::uint32_t offsetInBytes,
            std::uint32_t sizeInBytes
        ){
            Shape shape {
                .offsetInElements = offsetInBytes,
                .elementCount = sizeInBytes,
                .elementSizeInBytes = 1,
            };
            auto range = new BufferRange{
                buffer, shape, ResourceKind::UniformBuffer
            };
            return common::sp{range};
        }

        static common::sp<BufferRange> MakeByteBuffer(
            const common::sp<IBuffer>& buffer
        ) {
            return MakeByteBuffer(buffer, 0, buffer->GetDesc().sizeInBytes);
        }

        static common::sp<BufferRange> MakeStructuredBuffer(
            const common::sp<IBuffer>& buffer,
            const Shape& shape
        ){
            auto range = new BufferRange{ buffer, shape, ResourceKind::StorageBuffer};
            return common::sp{range};
        }

        template<typename T>
        static common::sp<BufferRange> MakeStructuredBuffer(
            const common::sp<IBuffer>& buffer,
            std::uint32_t offsetInElements,
            std::uint32_t sizeInElements
        ) {
            Shape shape {};
            shape.elementCount = sizeInElements;
            shape.elementSizeInBytes = sizeof(T);
            shape.offsetInElements = offsetInElements;
            return MakeStructuredBuffer(buffer, shape); }

        virtual ResourceKind GetResourceKind() const override { return _kind; }

        IBuffer* GetBufferObject() const {return _buffer.get();}
        
        const Shape& GetShape() const {return _shape;}

        void* MapToCPU() {
            auto ptr = _buffer->MapToCPU();
            if(!ptr) {
                return nullptr;
            }

            return (void*)(((size_t)ptr) + _shape.GetOffsetInBytes());
        }

        void UnMap() {
            _buffer->UnMap();
        }

    };

} // namespace alloy
