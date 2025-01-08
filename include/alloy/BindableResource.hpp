#pragma once

#include "alloy/common/RefCnt.hpp"
#include "alloy/common/BitFlags.hpp"
#include "alloy/Shader.hpp"

#include <cstdint>
#include <vector>
#include <string>

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

            MAX_VALUE
        };

        virtual ~IBindableResource() = default;

        virtual ResourceKind GetResourceKind() const = 0;
        
    };

    class IResourceLayout : public common::RefCntBase{

    public:
        struct Description{

            struct ElementDescription{
                uint32_t  bindingSlot;
                uint32_t  bindingSpace;
                std::string name;
                IBindableResource::ResourceKind kind;

                alloy::common::BitFlags<IShader::Stage> stages;

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
                        //std::uint8_t dynamicBinding : 1;
                        
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

        IResourceLayout(
            const Description& desc
        )
        : description(desc){}

    public:
        const Description& GetDesc() const {return description;}

        
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

    protected:
        Description description;

        IResourceSet(
            const Description& desc
        ) 
            : description(desc)
        {}

    public:
        const Description& GetDesc() const {return description;}

        virtual void* GetNativeHandle() const {return nullptr; }
    };
 
} // namespace alloy
