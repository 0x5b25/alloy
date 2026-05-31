#pragma once

#include "alloy/BindableResource.hpp"

#import <Metal/Metal.h>

#include <metal_irconverter/metal_irconverter.h>

#include <vector>
#include <span>

namespace alloy::mtl {

    class MetalDevice;
    class MetalResourceSet;
    class MetalMutableResourceSet;
    class MetalResourceSetBase;

    //Implement DX12-like root signature and descriptor tables
    class MetalResourceLayout : public IResourceLayout {

        friend class MetalResourceSet;
        friend class MetalResourceSetBase;
        friend class MetalMutableResourceSet;
    public:
        struct PushConstantInfo {
            uint32_t bindingSlot;
            uint32_t bindingSpace;
            uint32_t sizeInDwords;
            uint32_t offsetInDwords;
        };

        struct SlotLocation {
            uint32_t heapIndex;
            uint32_t startingDescriptorIndex;
            uint32_t linearResourceOffset;
            bool valid = false;
        };

    private:
        common::sp<MetalDevice> _dev;

        IRRootSignature* _rootSig;
        uint32_t _descHeapCount;
        std::vector<PushConstantInfo> _pushConstants;
        std::vector<SlotLocation> _slotLocations;

        uint64_t _rootSigSizeInBytes;

        struct _FixedSizeCtorTag {};
        struct _T2BindlessCtorTag {};

        static constexpr auto kFixedSize = _FixedSizeCtorTag{};
        static constexpr auto kT2Bindless = _T2BindlessCtorTag{};

        MetalResourceLayout(
            _FixedSizeCtorTag,
            const common::sp<MetalDevice>& dev,
            const IResourceLayout::Description& desc);


        MetalResourceLayout(
            _T2BindlessCtorTag,
            const common::sp<MetalDevice>& dev,
            const IResourceLayout::Description& desc);


    public:
        virtual ~MetalResourceLayout() override;


        static common::sp<MetalResourceLayout> Make (
            const common::sp<MetalDevice>& dev,
            const IResourceLayout::Description& desc
        ) {
            MetalResourceLayout* pLayout;

            if(desc.useGlobalHeaps) {
                pLayout = new MetalResourceLayout(kT2Bindless, dev, desc);
            } else {
                pLayout = new MetalResourceLayout(kFixedSize, dev, desc);
            }

            return common::sp(pLayout);
        }


        const IRRootSignature* GetHandle() const {return _rootSig;}

        uint32_t GetDescHeapCount() const { return _descHeapCount; }
        const std::vector<PushConstantInfo>& GetPushConstants() const { return _pushConstants; }
        const SlotLocation& GetSlotLocation(uint32_t layoutSlot) const {
            return _slotLocations.at(layoutSlot);
        }

        uint64_t GetRootSigSizeInBytes() const { return _rootSigSizeInBytes; }
    };

    // Descriptor heap is implemented as an argument buffer using the explicit
    // layout binding model expected by MetalShaderConverter.
    class MetalResourceSetBase {
    protected:
        common::sp<MetalDevice> _dev;
        common::sp<MetalResourceLayout> _layout;
        id<MTLBuffer> _argBuf;

        uint64_t _shaderResHeapGPUVA;
        uint64_t _samplerHeapGPUVA;
        uint64_t _combinedResHeapSize;
        uint64_t _samplerHeapSize;
        std::vector<common::sp<IBindableResource>> _boundResources;

        MetalResourceSetBase(
            const common::sp<MetalDevice>& dev,
            const common::sp<MetalResourceLayout>& layout);

        ~MetalResourceSetBase();

        void AllocateArgumentBuffer();
        void UpdateInternal(const std::span<const IMutableResourceSet::WriteBinding>& writes);

    public:
        const MetalResourceLayout& GetLayout() const { return *_layout; }

        std::vector<id<MTLResource>> GetUseResources() const;

        uint64_t GetShaderResHeapGPUVA() const { return _shaderResHeapGPUVA; }
        uint64_t GetSamplerHeapGPUVA() const { return _samplerHeapGPUVA; }

        id<MTLBuffer> GetHandle() const { return _argBuf; }


        IBindableResource* GetBoundResource(
            uint32_t layoutSlot,
            uint32_t firstArrayElement
        ) {
            auto linearBase = _layout->GetSlotLocation(layoutSlot).linearResourceOffset;

            return _boundResources[linearBase + firstArrayElement].get();
        }
    };

    class MetalResourceSet : public IResourceSet, public MetalResourceSetBase {
        MetalResourceSet(
            const common::sp<MetalDevice>& dev,
            const IResourceSet::Description& desc);

    public:
        virtual ~MetalResourceSet() override;

        static common::sp<MetalResourceSet> Make(
            const common::sp<MetalDevice>& dev,
            const IResourceSet::Description& desc);

        virtual const IResourceLayout& GetLayout() const override {
            return MetalResourceSetBase::GetLayout();
        }

        virtual void* GetNativeHandle() const override { return GetHandle(); }


        virtual IBindableResource* GetBoundResource(
            uint32_t layoutSlot,
            uint32_t firstArrayElement
        ) override {
            return MetalResourceSetBase::GetBoundResource(
                layoutSlot,
                firstArrayElement);
        }
    };

    class MetalMutableResourceSet
        : public IMutableResourceSet
        , public MetalResourceSetBase {
        MetalMutableResourceSet(
            const common::sp<MetalDevice>& dev,
            const IMutableResourceSet::Description& desc);

    public:
        virtual ~MetalMutableResourceSet() override;

        static common::sp<IMutableResourceSet> Make(
            const common::sp<MetalDevice>& dev,
            const IMutableResourceSet::Description& desc);

        virtual const IResourceLayout& GetLayout() const override {
            return MetalResourceSetBase::GetLayout();
        }

        void Update(const std::span<const WriteBinding>& writes) override;

        virtual void* GetNativeHandle() const override { return GetHandle(); }

        virtual IBindableResource* GetBoundResource(
            uint32_t layoutSlot,
            uint32_t firstArrayElement
        ) override {
            return MetalResourceSetBase::GetBoundResource(
                layoutSlot,
                firstArrayElement);
        }
    };

}
