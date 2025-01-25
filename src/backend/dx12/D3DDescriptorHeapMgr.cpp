#include "D3DDescriptorHeapMgr.hpp"

#include "alloy/common/Common.hpp"

#include "DXCDevice.hpp"

namespace alloy::dxc {

    

    _DescriptorHeapMgr::_DescriptorHeapMgr(
        ID3D12Device* dev,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        uint32_t maxDescCnt
    )
        : _dev(dev)
        , _type(type)
        , _maxDescCnt(maxDescCnt)
    {
        assert(dev != nullptr);
        _incrSize = _dev->GetDescriptorHandleIncrementSize(type);

        auto firstPool = _CreatePool(_maxDescCnt);
        auto firstPoolContainer = new Container(firstPool, this);
        _currentPool.reset(firstPoolContainer);
    }

    _DescriptorHeapMgr::~_DescriptorHeapMgr(){
       
        //All dirty pools should be recycled by now.
        assert(_dirtyPools.empty());
        //Retire current pool
        _currentPool = nullptr;

        while (!_freePools.empty()) {
            auto pool = _freePools.front();
            _freePools.pop();
            pool->Release();
        }
    }


    void _DescriptorHeapMgr::_ReleaseContainer(Container* container){
        //std::scoped_lock l{_m_pool};

        //Mainly for debug purposes
        //assert(_dirtyPools.find(container) != _dirtyPools.end());
        //may not be in dirty pool list during manager destruction
        _dirtyPools.erase(container);

        //VK_CHECK(vkResetDescriptorPool(_dev, container->pool, 0));
        _freePools.push(container->pool);
    }
    ID3D12DescriptorHeap* _DescriptorHeapMgr::_GetOnePool(){
        //Should only be called from allocate
        //std::scoped_lock l{_m_pool};

        ID3D12DescriptorHeap* rawPool;
        if(!_freePools.empty()){
            rawPool = _freePools.front();
            _freePools.pop();
        } else {
            rawPool = _CreatePool(_maxDescCnt);
        }

        return rawPool;
    }


    ID3D12DescriptorHeap* _DescriptorHeapMgr::_CreatePool(uint32_t maxDescCnt)
    {
        ID3D12DescriptorHeap* pHeap = nullptr;

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc {};
        heapDesc.NumDescriptors = maxDescCnt;
        heapDesc.Type = _type;
        auto hr = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pHeap));
        if (FAILED(hr))
        {
            //Running = false;
        }

        return pHeap;
    }

    _DescriptorSet _DescriptorHeapMgr::Allocate(const std::vector<common::sp<ITextureView>>& res) {

        common::sp<Container> targetPool {nullptr};
        _DescriptorSet allocated {};

        {
            //Critical section guard
            std::scoped_lock l{_m_pool};

            //Create a dedicated pool for extra large allocations
            if(res.size() > _maxDescCnt) {
                //Create and wrap the raw pool
                auto newPool = _CreatePool(res.size());
                auto container = new _DescriptorHeapMgr::Container(newPool, this);
                targetPool.reset(container);
            } else {
                auto freeSlots = _maxDescCnt - _currentPool->nextFreeSlot;
                //Make new pool if not enough free slots.
                if(freeSlots < res.size()) {
                    //Add to dirty pools
                    _dirtyPools.insert(_currentPool.get());
                    //Create and wrap the raw pool
                    auto newPool = _GetOnePool();
                    auto container = new _DescriptorHeapMgr::Container(newPool, this);
                    //change current pool to new pool
                    _currentPool.reset(container);
                }
                targetPool = _currentPool;
            }

            //Try to allocate from current pool.
            auto hFreeSlot = targetPool->pool->GetCPUDescriptorHandleForHeapStart();
            hFreeSlot.ptr += _incrSize * targetPool->nextFreeSlot;

            
            auto hCurrSlot = hFreeSlot;
            for(auto& texView : res) {

                auto* dxcTex = PtrCast<DXCTexture>(texView->GetTextureObject().get());
                auto& texDesc = dxcTex->GetDesc();
                auto& viewDesc = texView->GetDesc();

                switch (_type)
                {
                case D3D12_DESCRIPTOR_HEAP_TYPE_RTV: {
                    
                    D3D12_RENDER_TARGET_VIEW_DESC desc {};

                    switch(texDesc.type) {
                        case ITexture::Description::Type::Texture1D : {
                            if(texDesc.arrayLayers > 1) {
                                desc.Texture1DArray.ArraySize = viewDesc.arrayLayers;
                                desc.Texture1DArray.FirstArraySlice = viewDesc.baseArrayLayer;
                                desc.Texture1DArray.MipSlice = viewDesc.baseMipLevel;
                                desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
                            } else {
                                desc.Texture1D.MipSlice = viewDesc.baseMipLevel;
                                desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
                            }
                        }break;

                        case ITexture::Description::Type::Texture2D : {
                            if(texDesc.arrayLayers > 1) {
                                desc.Texture2DArray.ArraySize = viewDesc.arrayLayers;
                                desc.Texture2DArray.FirstArraySlice = viewDesc.baseArrayLayer;
                                desc.Texture2DArray.PlaneSlice = 0;
                                desc.Texture2DArray.MipSlice = viewDesc.baseMipLevel;
                                desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                            } else {
                                desc.Texture2D.PlaneSlice = 0;
                                desc.Texture2D.MipSlice = viewDesc.baseMipLevel;
                                desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                            }
                        }break;

                        case ITexture::Description::Type::Texture3D : {
                            if(texDesc.arrayLayers > 1) {
                                ///#TODO: handle tex array & texture type mismatch
                            } else {
                                desc.Texture3D.FirstWSlice = 0;
                                desc.Texture3D.WSize = -1;
                                desc.Texture3D.MipSlice = viewDesc.baseMipLevel;
                                desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
                            }
                        }break;
                    }

                    _dev->CreateRenderTargetView(dxcTex->GetHandle(), &desc, hCurrSlot);
                }break;

            
                case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:{
                    D3D12_DEPTH_STENCIL_VIEW_DESC desc {};

                    switch(texDesc.type) {
                        case ITexture::Description::Type::Texture1D : {
                            if(texDesc.arrayLayers > 1) {
                                desc.Texture1DArray.ArraySize = viewDesc.arrayLayers;
                                desc.Texture1DArray.FirstArraySlice = viewDesc.baseArrayLayer;
                                desc.Texture1DArray.MipSlice = viewDesc.baseMipLevel;
                                desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
                            } else {
                                desc.Texture1D.MipSlice = viewDesc.baseMipLevel;
                                desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
                            }
                        }break;

                        case ITexture::Description::Type::Texture2D : {
                            if(texDesc.arrayLayers > 1) {
                                desc.Texture2DArray.ArraySize = viewDesc.arrayLayers;
                                desc.Texture2DArray.FirstArraySlice = viewDesc.baseArrayLayer;
                                desc.Texture2DArray.MipSlice = viewDesc.baseMipLevel;
                                desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                            } else {
                                desc.Texture2D.MipSlice = viewDesc.baseMipLevel;
                                desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                            }
                        }break;

                        default : {
                            //#TODO: put in error state
                        }break;
                    }

                    _dev->CreateDepthStencilView(dxcTex->GetHandle(), &desc, hCurrSlot);
                } break;

                default:
                    break;
                }

                hCurrSlot.ptr += _incrSize;
            }

            allocated = _DescriptorSet(
                _currentPool, hFreeSlot, res.size()
            );
        }
        //Release the mutex to allow old container to do its clean-ups
        
        return allocated;
    }
}
