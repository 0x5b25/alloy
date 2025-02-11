#include "DXCFrameBuffer.hpp"

#include "DXCDevice.hpp"

namespace alloy::dxc {
    common::sp<DXCRenderTarget> DXCRenderTarget::Make(
        const common::sp<DXCTextureView>& view
    ) {
        auto dxcTex = common::PtrCast<DXCTexture>( view->GetTextureObject().get() );
        auto desc = dxcTex->GetDesc();
        auto dev = dxcTex->GetDevice();

        _Descriptor handle {};

        if(desc.usage.depthStencil) {
            handle = dev->AllocateDSV();
            dev->GetDevice()->CreateDepthStencilView(dxcTex->GetHandle(), nullptr, handle.handle);
        }
        else if(desc.usage.renderTarget) {
            handle = dev->AllocateRTV();
            dev->GetDevice()->CreateRenderTargetView(dxcTex->GetHandle(), nullptr, handle.handle);
        }

        if(!handle) return nullptr;

        return common::sp(new DXCRenderTarget(view, handle));
    }

    DXCRenderTarget::DXCRenderTarget(
        const common::sp<DXCTextureView>& view,
        _Descriptor handle
    ) 
        : _view(view)
        , _handle(handle)
    { }

    DXCRenderTarget::~DXCRenderTarget() {
        auto dxcTex = common::PtrCast<DXCTexture>( _view->GetTextureObject().get() );
        auto desc = dxcTex->GetDesc();
        auto dev = dxcTex->GetDevice();


        if(desc.usage.depthStencil) {
            dev->FreeDSV(_handle);
        }
        else if(desc.usage.renderTarget) {
            dev->FreeRTV(_handle);
        }
    }
}
