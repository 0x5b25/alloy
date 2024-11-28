#include "DXCTexture.hpp"

#include "veldrid/common/Common.hpp"

#include "DXCDevice.hpp"
#include "D3DTypeCvt.hpp"

namespace Veldrid {

    
    void DXCTexture::SetResource(ID3D12Resource* res) {
        _res = res;
        _res->AddRef();
    }

    DXCTexture::~DXCTexture() {
        _res->Release();
        if(_allocation) {
            _allocation->Release();
        }
    }

    DXCDevice* DXCTexture::GetDevice() const {
        return PtrCast<DXCDevice>(dev.get());
    }

    sp<Texture> DXCTexture::Make(
        const sp<DXCDevice>& dev,
        const Texture::Description& desc
    ) {
        D3D12_RESOURCE_DESC resourceDesc = {};
        switch(desc.type) {
            case Texture::Description::Type::Texture1D: {
                resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
                resourceDesc.Width = desc.width;
                resourceDesc.Height = 1;
                resourceDesc.DepthOrArraySize = desc.arrayLayers;
            }break;
            case Texture::Description::Type::Texture2D: {
                resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                resourceDesc.Width = desc.width;
                resourceDesc.Height = desc.height;
                resourceDesc.DepthOrArraySize = desc.arrayLayers;
            }break;
            case Texture::Description::Type::Texture3D: { 
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
        resourceDesc.SampleDesc.Count = 1;

        //#TODO: enable high quality MSAA
        if ((desc.usage.depthStencil || desc.usage.renderTarget)
            && desc.type == Texture::Description::Type::Texture2D) {
            resourceDesc.SampleDesc.Count = (uint32_t)desc.sampleCount;
            //switch(desc.sampleCount) {
            //    case SampleCount::x1 : resourceDesc.SampleDesc.Count = 1; break;
            //    case SampleCount::x2 : resourceDesc.SampleDesc.Count = 2; break;
            //    case SampleCount::x4 : resourceDesc.SampleDesc.Count = 4; break;
            //    case SampleCount::x8 : resourceDesc.SampleDesc.Count = 8; break;
            //    case SampleCount::x16: resourceDesc.SampleDesc.Count = 16; break;
            //    case SampleCount::x32: resourceDesc.SampleDesc.Count = 32 break;
            //}
        } else {
            resourceDesc.SampleDesc.Count = 1;
        }
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        //if(desc.usage.sampled)         resourceDesc.Flags |= ;
        if(desc.usage.storage)         resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        if(desc.usage.renderTarget)    resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if(desc.usage.depthStencil)    resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        //if(desc.usage.cubemap)         resourceDesc.Flags |= ;
        //if(desc.usage.generateMipmaps) resourceDesc.Flags |= ;

        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        D3D12_RESOURCE_STATES resourceState;

        switch (desc.hostAccess)
        {        
        case HostAccess::PreferRead:
            allocationDesc.HeapType = D3D12_HEAP_TYPE_READBACK;
            resourceState = D3D12_RESOURCE_STATE_COPY_DEST;
            break;
        case HostAccess::PreferWrite:
            allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
            break;
        case HostAccess::None:
        default:
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            resourceState = D3D12_RESOURCE_STATE_COMMON;
            break;
        }

        allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        D3D12MA::Allocation* allocation;
        HRESULT hr = dev->Allocator()->CreateResource(
            &allocationDesc,
            &resourceDesc,
            resourceState,
            NULL,
            &allocation,
             IID_NULL, NULL);

        if(FAILED(hr)){
            return nullptr;
        }

        auto tex = new DXCTexture{ dev, desc };
        tex->_allocation = allocation;
        tex->SetResource(allocation->GetResource());

        return sp(tex);
    }

    sp<Texture> DXCTexture::WrapNative(
        const sp<DXCDevice>& dev,
        const Texture::Description& desc,
        ID3D12Resource* nativeRes
    ) {
        auto tex = new DXCTexture{ dev, desc };
        tex->_allocation = nullptr;
        tex->SetResource(nativeRes);
        return sp(tex);
    }


    
}
