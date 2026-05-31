#pragma once

#include "alloy/common/RefCnt.hpp"
#include "alloy/common/BitFlags.hpp"
#include "alloy/Shader.hpp"

#include <cstdint>
#include <vector>
#include <string>
#include <span>

namespace alloy
{
    class IBindableResource : public common::RefCntBase{

    public:
        enum class ResourceKind {
            /// <summary>
            /// A <see cref="DeviceBuffer"/> accessed as a uniform buffer. A subset of a buffer can be bound using a
            /// <see cref="DeviceBufferRange"/>.
            /// </summary>
            UniformBuffer,
            /// <summary>
            /// A <see cref="DeviceBuffer"/> accessed as a storage buffer. A subset of a buffer can be bound using a
            /// <see cref="DeviceBufferRange"/>.
            /// </summary>
            StorageBuffer,
            /// <summary>
            /// A <see cref="Texture"/>, accessed through a Texture or <see cref="TextureView"/>.
            /// <remarks>Binding a <see cref="Texture"/> to a resource slot expecting a TextureReadWrite is equivalent to binding a
            /// <see cref="TextureView"/> that covers the full mip and array layer range, with the original Texture's
            /// <see cref="PixelFormat"/>.</remarks>
            /// </summary>
            Texture,
            /// <summary>
            /// A <see cref="alloy.Sampler"/>.
            /// </summary>
            Sampler,

            ALLOY_BITFLAG_MAX
        };

        virtual ~IBindableResource() = default;

        virtual ResourceKind GetResourceKind() const = 0;
        
    };

    class IResourceLayout : public common::RefCntBase{

    public:
        struct PushConstantDescription {
            uint32_t bindingSlot;
            uint32_t bindingSpace;
            uint32_t sizeInDwords;
        };

        struct ShaderResourceDescription {
            /* Alloy follows DX12 resource binding model
             * 
             * - each slot can only hold 1 resource, arrays will automatically
             *    occupy subsequent slots
             * - Every kind has it's own slot space, which means 
             *    {slot = 0, kind = UniformBuffer} and {slot = 0, kind = Texture}
             *    are different bindings that won't overlap
             */
            uint32_t  bindingSlot;
            uint32_t  bindingSpace;
            uint32_t  bindingCount;
            IBindableResource::ResourceKind kind;
            alloy::common::BitFlags<IShader::Stage> stages;
            struct Options {
                std::uint32_t writable : 1;
            } options {};
        };

        struct Description{
            std::vector<PushConstantDescription> pushConstants;
            std::vector<ShaderResourceDescription> shaderResources;

            // Create a T2 bindless layout. Check resourceBindingModel first.
            //
            // If this is set, shaderResources is ignored. We only support
            // full bindless mode.
            bool useGlobalHeaps = false;
        };

        struct HeapRangeLocation {
            enum {
                ResourceHeap, SamplerHeap
            } heapType;

            uint32_t offsetFromRangeBase;
        };

    protected:
        Description description;

        IResourceLayout(
            const Description& desc
        )
        : description(desc){}

    public:
        virtual const Description& GetDesc() const {return description;}

        
        virtual void* GetNativeHandle() const {return nullptr; }

    };

    class IResourceSet : public common::RefCntBase{

    
    public:
        struct Description{
            
            // The <see cref="ResourceLayout"/> describing the number and kind of resources used.
            common::sp<IResourceLayout> layout;
            
            // An array of <see cref="BindableResource"/> objects.
            // The number and type of resources must match those specified in the <see cref="ResourceLayout"/>.
            std::vector<common::sp<IBindableResource>> boundResources;
        };

    //protected:
    //    Description description;
    //
    //    IResourceSet(
    //        const Description& desc
    //    ) 
    //        : description(desc)
    //    {}

    public:
        virtual const IResourceLayout& GetLayout() const = 0;

        virtual IBindableResource* GetBoundResource(
            uint32_t layoutSlot,
            uint32_t firstArrayElement
        ) = 0;

        virtual void* GetNativeHandle() const {return nullptr; }
    };

    class IMutableResourceSet : public common::RefCntBase {

    public:
        struct WriteBinding {
            uint32_t layoutSlot;
            uint32_t firstArrayElement;
            std::vector<common::sp<IBindableResource>> resources;
        };

        struct Description {
            // The <see cref="ResourceLayout"/> describing the fixed resource capacity.
            common::sp<IResourceLayout> layout;

            // Sparse, slot-addressed writes applied during creation.
            // layoutSlot indexes IResourceLayout::Description::shaderResources.
            std::vector<WriteBinding> initialWrites;
        };

    //protected:
    //
    //    IMutableResourceSet(
    //        const Description& desc
    //    )
    //        : description(desc)
    //    {}

    public:
        //const Description& GetDesc() const {return description;}
        virtual const IResourceLayout& GetLayout() const = 0;

        virtual IBindableResource* GetBoundResource(
            uint32_t layoutSlot,
            uint32_t firstArrayElement
        ) = 0;

        virtual void Update(const std::span<const WriteBinding>& writes) = 0;

        virtual void* GetNativeHandle() const {return nullptr; }
    };
 
} // namespace alloy
