#pragma once

//3rd-party headers

//alloy public headers
#include "alloy/common/RefCnt.hpp"

#include "alloy/BindableResource.hpp"
#include "alloy/Pipeline.hpp"
#include "alloy/GraphicsDevice.hpp"
#include "alloy/SyncObjects.hpp"
#include "alloy/Buffer.hpp"
#include "alloy/SwapChain.hpp"

//standard library headers
#include <vector>
#include <optional>
#include <span>

//backend specific headers
#include <d3d12.h>

//platform specific headers

#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers


namespace alloy::dxc {

    class DXCDevice;
    class DXCTexture;

    class DXCResourceLayout : public IResourceLayout{
    
        enum {
            MAX_ROOT_SIGNATURE_SIZE_DW = 64
        };

        
        common::sp<DXCDevice> dev;

        Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSig;

    
        //VkDescriptorSetLayout _dsl;
    
        std::uint32_t _dynamicBufferCount;
        //DescriptorResourceCounts _drcs;

        std::uint32_t _rootConstantCount;
        uint32_t _heapCount;

    public:
        struct SlotLocation {
            uint32_t heapIndex;
            uint32_t startingDescriptorIndex;
            uint32_t linearResourceOffset;
            bool valid = false;
        };

    private:
        std::vector<SlotLocation> _slotLocations;
    
        DXCResourceLayout(
            const common::sp<DXCDevice>& dev,
            const Description& desc
        ) : IResourceLayout(desc)
            , dev(dev)
        {}
    
    public:
        virtual ~DXCResourceLayout() override {}
    
        static common::sp<IResourceLayout> Make(
            const common::sp<DXCDevice>& dev,
            const Description& desc
        );

        void* GetNativeHandle() const override {return _rootSig.Get(); }
        ID3D12RootSignature* GetHandle() const {return _rootSig.Get(); }
    
        //const VkDescriptorSetLayout& GetHandle() const {return _dsl;}
        //std::uint32_t GetDynamicBufferCount() const {return _dynamicBufferCount;}
        //const DescriptorResourceCounts& GetResourceCounts() const {return _drcs;}

        std::uint32_t GetHeapCount() const { return _heapCount; }
        const SlotLocation& GetSlotLocation(uint32_t layoutSlot) const {
            return _slotLocations.at(layoutSlot);
        }
    };

    class DXCResourceSetBase {
    protected:
        common::sp<DXCDevice> dev;
        common::sp<DXCResourceLayout> _layout;
        std::vector<ID3D12DescriptorHeap*> _descHeap;

        // Linear array to track resources written in.
        std::vector<common::sp<IBindableResource>> _boundResources;

        DXCResourceSetBase( const common::sp<DXCDevice>& dev,
                            const common::sp<DXCResourceLayout>& layout
        );

        void AllocateDescriptorHeaps();
        void UpdateInternal(const std::span<const IMutableResourceSet::WriteBinding>& writes);

    public:
        ~DXCResourceSetBase();

        const DXCResourceLayout& GetLayout() const { return *_layout; }
        const std::vector<common::sp<IBindableResource>>& GetBoundResources() const {
            return _boundResources;
        }
        const std::vector<ID3D12DescriptorHeap*>& GetHeaps() const { return _descHeap; }
    };
    
    class DXCResourceSet : public IResourceSet, public DXCResourceSetBase {
    public:
        //using ElementVisitor = std::function<void(VulkanResourceLayout*)>;
    private:

        DXCResourceSet(
            const common::sp<DXCDevice>& dev,
            const Description& desc
        );

    public:
        virtual ~DXCResourceSet() override;

        virtual const IResourceLayout& GetLayout() const override {
            return DXCResourceSetBase::GetLayout();
        }

        static common::sp<IResourceSet> Make(
            const common::sp<DXCDevice>& dev,
            const Description& desc
        );
    };

    class DXCMutableResourceSet : public IMutableResourceSet, public DXCResourceSetBase {
        DXCMutableResourceSet(
            const common::sp<DXCDevice>& dev,
            const Description& desc
        );
    public:
        ~DXCMutableResourceSet() override;

        static common::sp<IMutableResourceSet> Make(
            const common::sp<DXCDevice>& dev,
            const Description& desc
        );

        
        virtual const IResourceLayout& GetLayout() const override {
            return DXCResourceSetBase::GetLayout();
        }

        void Update(const std::span<const WriteBinding>& writes) override;
    };
}
