#pragma once

//3rd-party headers
#include <directx/d3d12.h>

//veldrid public headers
#include "veldrid/common/RefCnt.hpp"

#include "veldrid/BindableResource.hpp"
#include "veldrid/Pipeline.hpp"
#include "veldrid/GraphicsDevice.hpp"
#include "veldrid/SyncObjects.hpp"
#include "veldrid/Buffer.hpp"
#include "veldrid/SwapChain.hpp"

//standard library headers
#include <vector>

//backend specific headers
#include "directx/d3d12.h"

//platform specific headers

#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers


namespace Veldrid{

    class DXCDevice;
    class DXCTexture;

    class DXCResourceLayout : public ResourceLayout{
    
        enum {
            MAX_ROOT_SIGNATURE_SIZE_DW = 64
        };

        Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSig;

    
        //VkDescriptorSetLayout _dsl;
    
        std::uint32_t _dynamicBufferCount;
        //DescriptorResourceCounts _drcs;
    
        DXCResourceLayout(
            const sp<GraphicsDevice>& dev,
            const Description& desc
        ) : ResourceLayout(dev, desc){}
    
    public:
        virtual ~DXCResourceLayout() override {}
    
        static sp<ResourceLayout> Make(
            const sp<DXCDevice>& dev,
            const Description& desc
        );

        void* GetNativeHandle() const override {return _rootSig.Get(); }
        ID3D12RootSignature* GetHandle() const {return _rootSig.Get(); }
    
        //const VkDescriptorSetLayout& GetHandle() const {return _dsl;}
        //std::uint32_t GetDynamicBufferCount() const {return _dynamicBufferCount;}
        //const DescriptorResourceCounts& GetResourceCounts() const {return _drcs;}
    };
    
    class DXCResourceSet : public ResourceSet{ //Actually d3d12 descriptor heap?
    public:
        //using ElementVisitor = std::function<void(VulkanResourceLayout*)>;
    private:
//
    //    _DescriptorSet _descSet;
//
    //    std::unordered_set<VulkanTexture*> _texReadOnly, _texRW;
//
        
        std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> _descHeap;

        DXCResourceSet(
            const sp<GraphicsDevice>& dev,
            const Description& desc
        ) 
            : ResourceSet(dev, desc)
        {}

    public:
        virtual ~DXCResourceSet() override {}

        static sp<ResourceSet> Make(
            const sp<DXCDevice>& dev,
            const Description& desc
        );
//
    //    //const VkDescriptorSet& GetHandle() const { return _descSet.GetHandle(); }
//
//
    //    //void TransitionImageLayoutsIfNeeded(VkCommandBuffer cb);
    //    void VisitElements(ElementVisitor visitor);
    };
}