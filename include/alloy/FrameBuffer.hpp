#pragma once

#include "alloy/Types.hpp"
#include "alloy/Texture.hpp"
#include "alloy/FixedFunctions.hpp"

#include <vector>

namespace alloy
{
    //class Texture;

    // A description of the output attachments used by the <see cref="Pipeline"/>.
    

    class IRenderTarget : public common::RefCntBase {

    public:

        virtual ITextureView& GetTexture() const = 0;


    };

    struct OutputDescription {

        //struct Attachment{
        //    PixelFormat format;
        //};

        common::sp<IRenderTarget> depthAttachment;
        std::vector<common::sp<IRenderTarget>> colorAttachments;
        SampleCount sampleCount;

    };

    ///#TODO: Rename framebuffer to rendertarget to more accurately reflect its usage
    class IFrameBuffer : public common::RefCntBase{

    public:
        struct Description{
            using Attachment = common::sp<ITextureView>;
            //struct Attachment{
            //    /// The target texture to render into. For color attachments, this resource must have been created with the
            //    /// <see cref="TextureUsage.RenderTarget"/> flag. For depth attachments, this resource must have been created with the
            //    /// <see cref="TextureUsage.DepthStencil"/> flag.
            //    common::sp<ITexture> target;
            //    
            //    /// The array layer to render to. This value must be less than <see cref="Texture.ArrayLayers"/> in the target
            //    std::uint32_t arrayLayer;
            //    
            //    /// The mip level to render to. This value must be less than <see cref="Texture.MipLevels"/> in the target
            //    std::uint32_t mipLevel;
            //
            //};

            Attachment depthTarget;
            std::vector<Attachment> colorTargets;

            //bool HasColorTarget() const {return !colorTargets.empty();}
            //bool HasDepthTarget() const {return depthTarget != nullptr;}
            //std::uint32_t GetAttachmentCount() const {
            //    return HasDepthTarget()? colorTargets.size() + 1
            //                           : colorTargets.size();
            //}
            //std::uint32_t GetWidth() const {
            //    if(colorTargets.size() > 0){
            //        auto& texDesc = colorTargets.front()->GetTarget()->GetDesc();
            //        return texDesc.width;
            //    } else if(depthTarget != nullptr){
            //        auto& texDesc = depthTarget->GetTarget()->GetDesc();
            //        return texDesc.width;
            //    }
            //    return 0;
            //}
            //std::uint32_t GetHeight() const {
            //    if(colorTargets.size() > 0){
            //        auto& texDesc = colorTargets.front()->GetTarget()->GetDesc();
            //        return texDesc.height;
            //    } else if(depthTarget!= nullptr){
            //        auto& texDesc = depthTarget->GetTarget()->GetDesc();
            //        return texDesc.height;
            //    }
            //    return 0;
            //}
            //
            //std::uint32_t GetDepth() const {
            //    if(colorTargets.size() > 0){
            //        auto& texDesc = colorTargets.front()->GetTarget()->GetDesc();
            //        return texDesc.depth;
            //    } else if(depthTarget != nullptr){
            //        auto& texDesc = depthTarget->GetTarget()->GetDesc();
            //        return texDesc.depth;
            //    }
            //    return 0;
            //}
        };

    protected:
        //Description description;

        IFrameBuffer( ) { }
        virtual ~IFrameBuffer( ) { }

    public:
        /*TODO: Settle for now*/
        virtual OutputDescription GetDesc() = 0;
        //const{
        //    OutputDescription odesc{};
        //
        //    auto& sampleCount = odesc.sampleCount;
        //    auto& description = GetDesc();
        //
        //    sampleCount = SampleCount::x1;
        //    //OutputAttachmentDescription? depthAttachment = null;
        //    if (description.depthTarget.target != nullptr)
        //    {
        //        auto& texDesc = description.depthTarget.target->GetDesc();
        //        odesc.depthAttachment = OutputDescription::Attachment{ texDesc.format };
        //        sampleCount = texDesc.sampleCount;
        //    }
        //    odesc.colorAttachment.resize(description.colorTargets.size());
        //    for (int i = 0; i < description.colorTargets.size(); i++)
        //    {
        //        auto& texDesc = description.colorTargets[i].target->GetDesc();
        //        odesc.colorAttachment[i].format = texDesc.format;
        //        sampleCount = texDesc.sampleCount;
        //    }
        //
        //    return odesc;
        //}
        
        //virtual const Description& GetDesc() const = 0;

    };

} // namespace alloy
