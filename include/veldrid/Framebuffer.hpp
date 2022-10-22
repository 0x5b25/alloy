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
            std::uint32_t GetWidth() const {
                if(colorTarget.size() > 0){
                    auto& texDesc = colorTarget.front().target->GetDesc();
                    return texDesc.width;
                } else if(depthTarget.target != nullptr){
                    auto& texDesc = depthTarget.target->GetDesc();
                    return texDesc.width;
                }
                return 0;
            }
            std::uint32_t GetHeight() const {
                if(colorTarget.size() > 0){
                    auto& texDesc = colorTarget.front().target->GetDesc();
                    return texDesc.height;
                } else if(depthTarget.target != nullptr){
                    auto& texDesc = depthTarget.target->GetDesc();
                    return texDesc.height;
                }
                return 0;
            }
        };

    protected:
        Description description;

        Framebuffer(
            sp<GraphicsDevice>&& dev
        )
            : DeviceResource(std::move(dev))
        {}

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
        
        virtual const Description& GetDesc() const = 0;

    };

} // namespace Veldrid
