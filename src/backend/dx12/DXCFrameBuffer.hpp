#pragma once

#include "alloy/FrameBuffer.hpp"


//backend specific headers
#include "DXCTexture.hpp"
#include "D3DDescriptorHeapMgr.hpp"

//platform specific headers
#include <d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12

//Local headers

namespace alloy::dxc
{
    class DXCDevice;
    class DXCTexture;

    class DXCFrameBuffer : public IFrameBuffer{

        common::sp<DXCDevice> _dev;

    public:
        enum class VisitedAttachmentType {
            ColorAttachment, DepthAttachment, DepthStencilAttachment
        };
        //using AttachmentVisitor = std::function<void(const sp<DXCTexture>&, VisitedAttachmentType)>;

    protected:
        

        DXCFrameBuffer(
            const common::sp<DXCDevice>& dev
        ) : _dev(dev)
        { 
            //CreateCompatibleRenderPasses(
            //    renderPassNoClear, renderPassNoClearLoad, renderPassClear,
            //    isPresented
            //);
        }

    public:

        virtual ~DXCFrameBuffer() override { }

        virtual OutputDescription GetDesc() override;

        //virtual const VkFramebuffer& GetHandle() const = 0;

        //virtual void VisitAttachments(AttachmentVisitor visitor) = 0;

    };

    class DXCRenderTargetBase : public IRenderTarget {

    public:
        
        virtual D3D12_CPU_DESCRIPTOR_HANDLE GetHandle() const = 0;
    };

    class DXCRenderTarget : public DXCRenderTargetBase {
        
        common::sp<DXCTextureView> _view;        
        _Descriptor _handle;

    public:        
        static common::sp<DXCRenderTarget> Make(
            const common::sp<DXCTextureView>& view
        );

        DXCRenderTarget(
            const common::sp<DXCTextureView>& view,
            _Descriptor handle);

        virtual ~DXCRenderTarget() override;

        virtual D3D12_CPU_DESCRIPTOR_HANDLE GetHandle() const override {
            return _handle.handle;
        }
        
        virtual ITextureView& GetTexture() const override {
            return *_view.get();
        }

    };


} // namespace alloy
