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
    };
    
    class DXCResourceSet : public IResourceSet{ //Actually d3d12 descriptor heap?
    public:
        //using ElementVisitor = std::function<void(VulkanResourceLayout*)>;
    private:
//
    //    _DescriptorSet _descSet;
        common::sp<DXCDevice> dev;
//
    //    std::unordered_set<VulkanTexture*> _texReadOnly, _texRW;
//
        
        std::vector<ID3D12DescriptorHeap*> _descHeap;

        DXCResourceSet(
            const common::sp<DXCDevice>& dev,
            const Description& desc
        ) 
            : IResourceSet(desc)
            , dev(dev)
        {}

    public:
        virtual ~DXCResourceSet() override;

        static common::sp<IResourceSet> Make(
            const common::sp<DXCDevice>& dev,
            const Description& desc
        );
//
    //    //const VkDescriptorSet& GetHandle() const { return _descSet.GetHandle(); }
//
//
    //    //void TransitionImageLayoutsIfNeeded(VkCommandBuffer cb);
    //    void VisitElements(ElementVisitor visitor);
        const std::vector<ID3D12DescriptorHeap*>& GetHeaps() const {return _descHeap;}
        virtual void* GetNativeHandle() const override {return (void*)_descHeap.data(); }
    };
}