#pragma once

#include "veldrid/DeviceResource.hpp"
#include "veldrid/Types.hpp"
#include "veldrid/Texture.hpp"
#include "veldrid/FixedFunctions.hpp"

#include <vector>

namespace Veldrid
{
    
    class Framebuffer : public DeviceResource{

    public:
        struct Description{
            struct Attachment{
                /// The target texture to render into. For color attachments, this resource must have been created with the
                /// <see cref="TextureUsage.RenderTarget"/> flag. For depth attachments, this resource must have been created with the
                /// <see cref="TextureUsage.DepthStencil"/> flag.
                sp<Texture> target;
                
                /// The array layer to render to. This value must be less than <see cref="Texture.ArrayLayers"/> in the target
                std::uint32_t arrayLayer;
                
                /// The mip level to render to. This value must be less than <see cref="Texture.MipLevels"/> in the target
                std::uint32_t mipLevel;

            };

            Attachment depthTarget;
            std::vector<Attachment> colorTarget;

            bool HasColorTarget() const {return !colorTarget.empty();}
            bool HasDepthTarget() const {return depthTarget.target != nullptr;}
        };

    protected:
        Description description;

        std::uint32_t width, height;

        Framebuffer(
            sp<GraphicsDevice>&& dev,
            const Description desc
        )
            : DeviceResource(std::move(dev))
            , description(desc)
            , width(0), height(0)

        {
            if(description.colorTarget.size() > 0){
                auto& texDesc = description.colorTarget.front().target->GetDesc();
                width = texDesc.width;
                height = texDesc.height;
            } else if(description.depthTarget.target != nullptr){
                auto& texDesc = description.depthTarget.target->GetDesc();
                width = texDesc.width;
                height = texDesc.height;
            }
        }

    public:
        /*TODO: Settle for now*/
        OutputDescription GetOutputDescription() const{
            OutputDescription odesc{};

            auto& sampleCount = odesc.sampleCount;

            sampleCount = Texture::Description::SampleCount::x1;
            //OutputAttachmentDescription? depthAttachment = null;
            if (description.depthTarget.target != nullptr)
            {
                auto& texDesc = description.depthTarget.target->GetDesc();
                odesc.depthAttachment = OutputDescription::Attachment{ texDesc.format };
                sampleCount = texDesc.sampleCount;
            }
            odesc.colorAttachment.resize(description.colorTarget.size());
            for (int i = 0; i < description.colorTarget.size(); i++)
            {
                auto& texDesc = description.colorTarget[i].target->GetDesc();
                odesc.colorAttachment[i].format = texDesc.format;
                sampleCount = texDesc.sampleCount;
            }
        
            return odesc;
        }
        
        const Description& GetDesc() const { return description; }

        std::uint32_t Width() const {return width;}
        std::uint32_t Height() const {return height;}

    };

} // namespace Veldrid
