#pragma once

#include "alloy/Buffer.hpp"
#include "alloy/Sampler.hpp"
#include "alloy/Texture.hpp"

#include <cstdint>
#include <span>
#include <variant>

namespace alloy
{
    struct ResourceDescriptorIndex {
        static constexpr std::uint32_t Invalid = 0xffffffffu;

        std::uint32_t value = Invalid;

        constexpr ResourceDescriptorIndex() = default;
        explicit constexpr ResourceDescriptorIndex(std::uint32_t value) : value(value) {}

        constexpr bool IsValid() const { return value != Invalid; }

        constexpr ResourceDescriptorIndex& operator+=(std::uint32_t offset) {
            value += offset;
            return *this;
        }

        constexpr ResourceDescriptorIndex& operator-=(std::uint32_t offset) {
            value -= offset;
            return *this;
        }
    };

    struct SamplerDescriptorIndex {
        static constexpr std::uint32_t Invalid = 0xffffffffu;

        std::uint32_t value = Invalid;

        constexpr SamplerDescriptorIndex() = default;
        explicit constexpr SamplerDescriptorIndex(std::uint32_t value) : value(value) {}

        constexpr bool IsValid() const { return value != Invalid; }

        constexpr SamplerDescriptorIndex& operator+=(std::uint32_t offset) {
            value += offset;
            return *this;
        }

        constexpr SamplerDescriptorIndex& operator-=(std::uint32_t offset) {
            value -= offset;
            return *this;
        }
    };

    constexpr ResourceDescriptorIndex operator+(
        ResourceDescriptorIndex index,
        std::uint32_t offset) {
        index += offset;
        return index;
    }

    constexpr ResourceDescriptorIndex operator-(
        ResourceDescriptorIndex index,
        std::uint32_t offset) {
        index -= offset;
        return index;
    }

    constexpr SamplerDescriptorIndex operator+(
        SamplerDescriptorIndex index,
        std::uint32_t offset) {
        index += offset;
        return index;
    }

    constexpr SamplerDescriptorIndex operator-(
        SamplerDescriptorIndex index,
        std::uint32_t offset) {
        index -= offset;
        return index;
    }

    // Maps to IBindableResource::ResourceKind::UniformBuffer.
    // The BufferRange describes the buffer object and byte range used for the descriptor.
    struct UniformBufferDescriptor {
        common::sp<BufferRange> buffer;
    };

    // Maps to IBindableResource::ResourceKind::StorageBuffer with read-only descriptor access.
    // The read/write interpretation is not carried by BufferRange, so T2 writes spell it out here.
    struct ReadOnlyStorageBufferDescriptor {
        common::sp<BufferRange> buffer;
    };

    // Maps to IBindableResource::ResourceKind::StorageBuffer with read-write descriptor access.
    // Backends use this to create a UAV/storage-buffer descriptor instead of an SRV/read-only one.
    struct ReadWriteStorageBufferDescriptor {
        common::sp<BufferRange> buffer;
    };

    // Maps to IBindableResource::ResourceKind::Texture with sampled/read-only descriptor access.
    // The ITextureView selects the texture subresource range exposed to the shader.
    struct SampledTextureDescriptor {
        common::sp<ITextureView> texture;
    };

    // Maps to IBindableResource::ResourceKind::Texture with storage/read-write descriptor access.
    // The writable interpretation is not carried by ITextureView, so T2 writes spell it out here.
    struct StorageTextureDescriptor {
        common::sp<ITextureView> texture;
    };

    using ResourceDescriptorWrite = std::variant<
        UniformBufferDescriptor,
        ReadOnlyStorageBufferDescriptor,
        ReadWriteStorageBufferDescriptor,
        SampledTextureDescriptor,
        StorageTextureDescriptor>;

    // Maps to IBindableResource::ResourceKind::Sampler. Samplers use a separate heap, so no
    // descriptor-type variant is needed for sampler writes.
    using SamplerDescriptorWrite = common::sp<ISampler>;

    struct ResourceDescriptorHeapDescription {
        std::uint32_t capacity;
    };

    struct SamplerDescriptorHeapDescription {
        std::uint32_t capacity;
    };

    class IResourceDescriptorHeap : public common::RefCntBase {
    public:
        using Description = ResourceDescriptorHeapDescription;

        virtual const Description& GetDesc() const = 0;

        virtual void Write(
            ResourceDescriptorIndex index,
            const ResourceDescriptorWrite& write) = 0;
        virtual void WriteRange(
            ResourceDescriptorIndex firstIndex,
            std::span<const ResourceDescriptorWrite> writes) = 0;
        virtual void Clear(ResourceDescriptorIndex index) = 0;
        virtual void ClearRange(ResourceDescriptorIndex firstIndex, std::uint32_t count) = 0;

        virtual void* GetNativeHandle() const { return nullptr; }
    };

    class ISamplerDescriptorHeap : public common::RefCntBase {
    public:
        using Description = SamplerDescriptorHeapDescription;

        virtual const Description& GetDesc() const = 0;

        virtual void Write(
            SamplerDescriptorIndex index,
            const SamplerDescriptorWrite& sampler) = 0;
        virtual void WriteRange(
            SamplerDescriptorIndex firstIndex,
            std::span<const SamplerDescriptorWrite> samplers) = 0;
        virtual void Clear(SamplerDescriptorIndex index) = 0;
        virtual void ClearRange(SamplerDescriptorIndex firstIndex, std::uint32_t count) = 0;

        virtual void* GetNativeHandle() const { return nullptr; }
    };
} // namespace alloy
