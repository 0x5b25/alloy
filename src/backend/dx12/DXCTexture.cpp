#include "DXCTexture.hpp"

#include "alloy/common/Common.hpp"

#include "DXCDevice.hpp"
#include "D3DTypeCvt.hpp"
#include "D3DCommon.hpp"

#include <dxgi1_4.h>
#include <dxgidebug.h>

#include <format>

namespace alloy::dxc {

    uint32_t DXCTexture::ComputeSubresource(uint32_t mipLevel, uint32_t mipLevelCount, uint32_t arrayLayer)
    {
        return ((arrayLayer * mipLevelCount) + mipLevel);
    };

    void DXCTexture::SetResource(ID3D12Resource* res) {
        _res = res;
        //_res->AddRef();
    }

    DXCTexture::~DXCTexture() {
        for(auto& timeline : timelines)
            timeline->RemoveResource(this);
        if(_allocation) {
            //Resource is managed by D3D12MA allocation
            //We don't need ot manually release it, the
            // allocation release will do that for us.
            _allocation->Release();
        } else {
            _res->Release();
        }
    }

    common::sp<ITexture> DXCTexture::Make(
        const common::sp<DXCDevice>& dev,
        const ITexture::Description& desc
    ) {
        D3D12_RESOURCE_DESC resourceDesc = {};
        switch (desc.type) {
        case ITexture::Description::Type::Texture1D: {
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
            resourceDesc.Width = desc.width;
            resourceDesc.Height = 1;
            resourceDesc.DepthOrArraySize = desc.arrayLayers;
        }break;
        case ITexture::Description::Type::Texture2D: {
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resourceDesc.Width = desc.width;
            resourceDesc.Height = desc.height;
            resourceDesc.DepthOrArraySize = desc.arrayLayers;
        }break;
        case ITexture::Description::Type::Texture3D: {
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
            resourceDesc.Width = desc.width;
            resourceDesc.Height = desc.height;
            resourceDesc.DepthOrArraySize = desc.depth;
        }break;
        }
        //For buffers Alignment must be 64KB (D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT) or 0, 
        //which is effectively 64KB.
        resourceDesc.Alignment = 0;
        resourceDesc.MipLevels = desc.mipLevels;
        resourceDesc.Format = VdToD3DPixelFormat(desc.format, desc.usage.depthStencil);
        //resourceDesc.SampleDesc.Count = 1;

        ///#TODO: enable high quality MSAA
        if ((desc.usage.depthStencil || desc.usage.renderTarget)
            && desc.type == ITexture::Description::Type::Texture2D) {
            switch(desc.sampleCount) {
                default:
                case SampleCount::x1 : resourceDesc.SampleDesc.Count = 1; break;
                case SampleCount::x2 : resourceDesc.SampleDesc.Count = 2; break;
                case SampleCount::x4 : resourceDesc.SampleDesc.Count = 4; break;
                case SampleCount::x8 : resourceDesc.SampleDesc.Count = 8; break;
                case SampleCount::x16: resourceDesc.SampleDesc.Count = 16; break;
                case SampleCount::x32: resourceDesc.SampleDesc.Count = 32; break;
            }

            //Query usable sample qualities
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS queryData {
                .Format = resourceDesc.Format,
                .SampleCount = resourceDesc.SampleDesc.Count
            };
            dev->GetDevice()->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS
                , &queryData, sizeof(queryData)
            );

            if(queryData.NumQualityLevels == 0) {
                std::string msg = std::format("Sample count {} is not supported for this format", resourceDesc.SampleDesc.Count);
                throw std::invalid_argument(msg);
            }

            assert(queryData.NumQualityLevels != 0);
            resourceDesc.SampleDesc.Quality = queryData.NumQualityLevels - 1;
        }
        else {
            resourceDesc.SampleDesc.Count = 1;
            resourceDesc.SampleDesc.Quality = 0;
        }
        //Depth stencil textures can't use row_major
        //There are also tiled resource support that needs 64K_** layout
        //resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        //if(desc.usage.sampled)         resourceDesc.Flags |= ;
        if (desc.usage.storage)         resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        if (desc.usage.renderTarget)    resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (desc.usage.depthStencil)    resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        //if(desc.usage.cubemap)         resourceDesc.Flags |= ;
        //if(desc.usage.generateMipmaps) resourceDesc.Flags |= ;

        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        D3D12_RESOURCE_STATES resourceState;

        if(desc.usage.shareable) {
            allocationDesc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;
            allocationDesc.ExtraHeapFlags |= D3D12_HEAP_FLAG_SHARED;
        }

        //D3D12_HEAP_TYPE cpuAccessableLFB = (D3D12_HEAP_TYPE)0;
        //Make sure that we default to a invalid heap
        static_assert(D3D12_HEAP_TYPE_DEFAULT != (D3D12_HEAP_TYPE)0);
#if 0
        if (desc.hostAccess != HostAccess::None) {

            if (dev->GetDevCaps().SupportUMA()) {
                //allocationDesc.HeapType = D3D12_HEAP_TYPE_CUSTOM;
                allocationDesc.CustomPool = dev->UMAPool();
            }
            else if (dev->GetDevCaps().SupportReBAR()) {
                allocationDesc.HeapType = D3D12_HEAP_TYPE_GPU_UPLOAD;
            }

            else {
                //Uh-oh, no host accessible texture for you!
                return nullptr;
            }

            ///#TODO DX12 is very restrictive on host visible textures.
            // we must ensure following:
            //    * Layout is D3D12_TEXTURE_LAYOUT_ROW_MAJOR
            //    * only D3D12_RESOURCE_DIMENSION_TEXTURE_2D is supported.
            //    * A single mip level.
            //    * A single array slice.
            //    * 64KB alignment.
            //    * Non-MSAA.
            //    * No D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL.
            //    * The format cannot be a YUV format.

            //allocationDesc.ExtraHeapFlags |=   D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER
            //                                 | D3D12_HEAP_FLAG_SHARED;

            //resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        }
        else {

    
        //switch (desc.hostAccess)
        //{        
        //case HostAccess::None:
        //    allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
        //    break;
        //case HostAccess::PreferRead:
        //    //allocationDesc.HeapType = cpuAccessableLFB;
        //    //resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        //    //resourceState = D3D12_RESOURCE_STATE_COPY_DEST;
        //    break;
        //case HostAccess::PreferWrite:
        //    //allocationDesc.HeapType = cpuAccessableLFB;
        //    //resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        //    //resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
        //    break;
        //default:
        //    //allocationDesc.HeapType = cpuAccessableLFB;
        //    //resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        //    //resourceState = D3D12_RESOURCE_STATE_COMMON;
        //    break;
        //}
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        }
#endif
        switch (desc.hostAccess) {
        case HostAccess::SystemMemoryPreferRead:
        case HostAccess::SystemMemoryPreferWrite:
        case HostAccess::PreferDeviceMemory:
            allocationDesc.CustomPool = dev->GetHostAccessablePool(desc.hostAccess);
            break;
        case HostAccess::None:
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            break;
        default:
            break;
        }

        resourceState = D3D12_RESOURCE_STATE_COMMON;

        D3D12MA::Allocation* allocation;
        HRESULT hr = dev->Allocator()->CreateResource(
            &allocationDesc,
            &resourceDesc,
            resourceState,
            NULL,
            &allocation,
             IID_NULL, NULL);

        if(FAILED(hr)){
            
            //Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug;
            //Microsoft::WRL::ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
            //if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
            //{
            //    ThrowIfFailed(dxgiDebug->QueryInterface(IID_PPV_ARGS(&dxgiInfoQueue)));
            //
            //    for (UINT64 index = 0; index < dxgiInfoQueue->GetNumStoredMessages(DXGI_DEBUG_ALL); index++)
            //    {
            //        SIZE_T length = 0;
            //        dxgiInfoQueue->GetMessage(DXGI_DEBUG_ALL, index, nullptr, &length);
            //        auto msg = (DXGI_INFO_QUEUE_MESSAGE*)malloc(sizeof(length));
            //        dxgiInfoQueue->GetMessage(DXGI_DEBUG_ALL, index, msg, &length);
            //        printf("Description: %s\n", msg->pDescription);
            //    }
            //}

            return nullptr;
        }

        auto tex = new DXCTexture{ dev, desc };
        tex->_allocation = allocation;
        tex->SetResource(allocation->GetResource());

        return common::sp(tex);
    }

    common::sp<DXCTexture> DXCTexture::WrapNative(
        const common::sp<DXCDevice>& dev,
        const ITexture::Description& desc,
        ID3D12Resource* nativeRes
    ) {
        auto tex = new DXCTexture{ dev, desc };
        tex->_allocation = nullptr;
        tex->SetResource(nativeRes);
        return common::sp(tex);
    }

    void DXCTexture::WriteSubresource(
        uint32_t mipLevel,
        uint32_t arrayLayer,
        Point3D dstOrigin,
        Size3D writeSize,
        const void* src,
        uint32_t srcRowPitch,
        uint32_t srcDepthPitch
    ) {
        auto subResIdx = ComputeSubresource(mipLevel, description.mipLevels, arrayLayer);

        D3D12_BOX dstBox {
            .left = dstOrigin.x,
            .top = dstOrigin.y,
            .front = dstOrigin.z,
            .right = dstOrigin.x + writeSize.width,
            .bottom = dstOrigin.y + writeSize.height,
            .back = dstOrigin.z + writeSize.depth,
        };
        ThrowIfFailed(GetHandle()->Map(subResIdx, nullptr, nullptr));
        ThrowIfFailed(GetHandle()->WriteToSubresource(subResIdx, 
                                                      &dstBox,
                                                      src,
                                                      srcRowPitch,
                                                      srcDepthPitch ));

        GetHandle()->Unmap(subResIdx, nullptr);
    }

    void DXCTexture::ReadSubresource(
        void* dst,
        uint32_t dstRowPitch,
        uint32_t dstDepthPitch,
        uint32_t mipLevel,
        uint32_t arrayLayer,
        Point3D srcOrigin,
        Size3D readSize
    ) {
        auto subResIdx = ComputeSubresource(mipLevel, description.mipLevels, arrayLayer);

        D3D12_BOX srcBox {
            .left = srcOrigin.x,
            .top = srcOrigin.y,
            .front = srcOrigin.z,
            .right = srcOrigin.x + readSize.width,
            .bottom = srcOrigin.y + readSize.height,
            .back = srcOrigin.z + readSize.depth,
        };

        GetHandle()->Map(subResIdx, nullptr, nullptr);

        ThrowIfFailed(GetHandle()->ReadFromSubresource(dst,
                                                       dstRowPitch,
                                                       dstDepthPitch,
                                                       subResIdx, 
                                                       &srcBox));

        GetHandle()->Unmap(subResIdx, nullptr);
    }

    ITexture::SubresourceLayout DXCTexture::GetSubresourceLayout(
            uint32_t mipLevel,
            uint32_t arrayLayer,
            SubresourceAspect aspect
    ) {
        D3D12_RESOURCE_DESC desc = _res->GetDesc();
        
        ID3D12Device* pDevice;
        _res->GetDevice(IID_PPV_ARGS(&pDevice));

        auto subresIdx = ComputeSubresource(mipLevel, desc.MipLevels, arrayLayer);

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;

        /*
        typedef struct D3D12_SUBRESOURCE_FOOTPRINT
            {
            DXGI_FORMAT Format;
            UINT Width;
            UINT Height;
            UINT Depth;
            UINT RowPitch;
            } 	D3D12_SUBRESOURCE_FOOTPRINT;

        typedef struct D3D12_PLACED_SUBRESOURCE_FOOTPRINT
            {
            UINT64 Offset;
            D3D12_SUBRESOURCE_FOOTPRINT Footprint;
            } 	D3D12_PLACED_SUBRESOURCE_FOOTPRINsT;

        
        */
        uint64_t subResSizeInBytes = 0;
        pDevice->GetCopyableFootprints(&desc, subresIdx, 1, 0, 
            &footprint, nullptr, nullptr, &subResSizeInBytes);

        //_In_range_(0,D3D12_REQ_SUBRESOURCES)  UINT FirstSubresource,
        //_In_range_(0,D3D12_REQ_SUBRESOURCES-FirstSubresource)  UINT NumSubresources,
        //UINT64 BaseOffset,
        //_Out_writes_opt_(NumSubresources)  D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pLayouts,
        //_Out_writes_opt_(NumSubresources)  UINT *pNumRows,
        //_Out_writes_opt_(NumSubresources)  UINT64 *pRowSizeInBytes,
        //_Out_opt_  UINT64 *pTotalBytes

        ITexture::SubresourceLayout ret{};
        ret.offset = footprint.Offset;
        ret.rowPitch = footprint.Footprint.RowPitch;
        ret.depthPitch = ret.rowPitch * footprint.Footprint.Height;
        ret.arrayPitch = ret.depthPitch * footprint.Footprint.Depth;

        pDevice->Release();

        return ret;
    }

    common::sp<DXCTextureView> DXCTextureView::Make (
        const common::sp<DXCTexture>& tex,
        const ITextureView::Description& desc
    ) {
        return common::sp(new DXCTextureView(tex, desc));
    }

    
    void DXCTexture::SetDebugName(const std::string& name) {
        GetHandle()->SetPrivateData( WKPDID_D3DDebugObjectName, name.size(), name.data() );
    }

    void DXCTexture::NotifyUsageOn(IDXCTimeline* timeline) {
        //Resource is used on timeline, clear stale data on other
        //timelines
        for(auto t : timelines) {
            if(t == timeline) {
                continue;
            }
            t->RemoveResource(this);
        }

        timelines = {timeline};
    }
    
}
