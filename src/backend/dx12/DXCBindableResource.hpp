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

//platform specific headers

#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers


namespace Veldrid{

    class DXCDevice;
    class DXCTexture;

    //class VulkanResourceLayout : public ResourceLayout{
//
//
    //    VkDescriptorSetLayout _dsl;
//
    //    std::uint32_t _dynamicBufferCount;
    //    DescriptorResourceCounts _drcs;
//
    //    VulkanResourceLayout(
    //        const sp<GraphicsDevice>& dev,
    //        const Description& desc
    //    ) : ResourceLayout(dev, desc){}
//
    //public:
    //    ~VulkanResourceLayout();
//
    //    static sp<ResourceLayout> Make(
    //        const sp<VulkanDevice>& dev,
    //        const Description& desc
    //    );
//
    //    const VkDescriptorSetLayout& GetHandle() const {return _dsl;}
    //    std::uint32_t GetDynamicBufferCount() const {return _dynamicBufferCount;}
    //    const DescriptorResourceCounts& GetResourceCounts() const {return _drcs;}
    //};
//
    //class DXCResourceSet : public ResourceSet{ //Actually d3d12 descriptor heap?
    //public:
    //    using ElementVisitor = std::function<void(VulkanResourceLayout*)>;
    //private:
//
    //    _DescriptorSet _descSet;
//
    //    std::unordered_set<VulkanTexture*> _texReadOnly, _texRW;
//
    //    DXCResourceSet(
    //        const sp<GraphicsDevice>& dev,
    //        _DescriptorSet&& set,
    //        const Description& desc
    //    ) 
    //        : ResourceSet(dev, desc)
    //        , _descSet(std::move(set))
    //    
    //    {}
//
    //public:
    //    ~DXCResourceSet();
//
    //    static sp<ResourceSet> Make(
    //        const sp<DXCDevice>& dev,
    //        const Description& desc
    //    );
//
    //    //const VkDescriptorSet& GetHandle() const { return _descSet.GetHandle(); }
//
//
    //    //void TransitionImageLayoutsIfNeeded(VkCommandBuffer cb);
    //    void VisitElements(ElementVisitor visitor);
    //};
}