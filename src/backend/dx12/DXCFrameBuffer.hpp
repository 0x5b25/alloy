#pragma once

#include "veldrid/Framebuffer.hpp"


//backend specific headers

//platform specific headers
#include <d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12

//Local headers

namespace Veldrid
{
    class DXCDevice;
    class DXCTexture;

    class DXCFrameBufferBase : public Framebuffer{
    public:
        enum class VisitedAttachmentType {
            ColorAttachment, DepthAttachment, DepthStencilAttachment
        };
        //using AttachmentVisitor = std::function<void(const sp<DXCTexture>&, VisitedAttachmentType)>;

    protected:
        

        DXCFrameBufferBase(
            const sp<GraphicsDevice>& dev
        ) : Framebuffer(dev)
        { 
            //CreateCompatibleRenderPasses(
            //    renderPassNoClear, renderPassNoClearLoad, renderPassClear,
            //    isPresented
            //);
        }

    public:

        virtual ~DXCFrameBufferBase() override { }

        //virtual const VkFramebuffer& GetHandle() const = 0;

        virtual uint32_t GetRTVCount() const = 0;
        virtual bool HasDSV() const = 0;
        virtual bool DSVHasStencil() const = 0;

        virtual D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(uint32_t slot) const = 0;
        virtual D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const = 0;


        //virtual void VisitAttachments(AttachmentVisitor visitor) = 0;

    };

    class DXCFrameBuffer : public DXCFrameBufferBase {


        



    };


} // namespace Veldrid
