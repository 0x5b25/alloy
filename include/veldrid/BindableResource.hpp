#pragma once

#include "veldrid/common/RefCnt.hpp"
#include "veldrid/common/BitFlags.hpp"
#include "veldrid/DeviceResource.hpp"
#include "veldrid/Shader.hpp"

#include <cstdint>
#include <vector>
#include <string>

namespace Veldrid
{
    class IBindableResource : public RefCntBase{

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
            /// A <see cref="Veldrid.Sampler"/>.
            /// </summary>
            Sampler,

            MAX_VALUE
        };

        virtual ~IBindableResource() = default;

        virtual ResourceKind GetResourceKind() const = 0;
        
    };

    class ResourceLayout : public DeviceResource{

    public:
        struct Description{

            struct ElementDescription{
                uint32_t  bindingSlot;
                std::string name;
                IBindableResource::ResourceKind kind;

                alloy::BitFlags<Shader::Stage> stages;

                union Options
                {
                    struct {
                        /// <summary>
                        /// No special options.
                        /// </summary>
                        //None,
                        /// <summary>
                        /// Can be applied to a buffer type resource (<see cref="ResourceKind.StructuredBufferReadOnly"/>,
                        /// <see cref="ResourceKind.StructuredBufferReadWrite"/>, or <see cref="ResourceKind.UniformBuffer"/>), allowing it to be
                        /// bound with a dynamic offset using <see cref="CommandList.SetGraphicsResourceSet(uint, ResourceSet, uint[])"/>.
                        /// Offsets specified this way must be a multiple of <see cref="GraphicsDevice.UniformBufferMinOffsetAlignment"/> or
                        /// <see cref="GraphicsDevice.StructuredBufferMinOffsetAlignment"/>.
                        /// </summary>
                        std::uint8_t dynamicBinding : 1;
                        
                        //Resource is writable by shader
                        // can only applied to storage buffers, texture storages
                        std::uint8_t writable : 1;
                    };
                    std::uint8_t value;
                } options;


            };

            std::vector<ElementDescription> elements;

        };

    protected:
        Description description;

        ResourceLayout(
            const sp<GraphicsDevice>& dev,
            const Description& desc
        )
        : DeviceResource(dev)
        , description(desc){}

    public:
        const Description& GetDesc() const {return description;}

        
        virtual void* GetNativeHandle() const {return nullptr; }

    };

    class ResourceSet : public DeviceResource{

    
    public:
        struct Description{
            
            // The <see cref="ResourceLayout"/> describing the number and kind of resources used.
            sp<ResourceLayout> layout;
            
            // An array of <see cref="BindableResource"/> objects.
            // The number and type of resources must match those specified in the <see cref="ResourceLayout"/>.
            std::vector<sp<IBindableResource>> boundResources;
        };

    protected:
        Description description;

        ResourceSet(
            const sp<GraphicsDevice>& dev,
            const Description& desc
        ) 
            : DeviceResource(dev)
            , description(desc)
        {}

    public:
        const Description& GetDesc() const {return description;}

        virtual void* GetNativeHandle() const {return nullptr; }
    };
 
} // namespace Veldrid
