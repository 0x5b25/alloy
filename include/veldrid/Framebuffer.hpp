#pragma once

#include "veldrid/DeviceResource.hpp"
#include "veldrid/Types.hpp"
#include "veldrid/Texture.hpp"
#include "veldrid/FixedFunctions.hpp"

#include <vector>

namespace Veldrid
{
    // A description of the output attachments used by the <see cref="Pipeline"/>.
    struct OutputDescription {

        struct Attachment{
            PixelFormat format;
        };

        std::optional<Attachment> depthAttachment;
        std::vector<Attachment> colorAttachment;
        Texture::Description::SampleCount sampleCount;

    };

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
            std::vector<Attachment> colorTargets;

            bool HasColorTarget() const {return !colorTargets.empty();}
            bool HasDepthTarget() const {return depthTarget.target != nullptr;}
            std::uint32_t GetAttachmentCount() const {
                return HasDepthTarget()? colorTargets.size() + 1
                                       : colorTargets.size();
            }
            std::uint32_t GetWidth() const {
                if(colorTargets.size() > 0){
                    auto& texDesc = colorTargets.front().target->GetDesc();
                    return texDesc.width;
                } else if(depthTarget.target != nullptr){
                    auto& texDesc = depthTarget.target->GetDesc();
                    return texDesc.width;
                }
                return 0;
            }
            std::uint32_t GetHeight() const {
                if(colorTargets.size() > 0){
                    auto& texDesc = colorTargets.front().target->GetDesc();
                    return texDesc.height;
                } else if(depthTarget.target != nullptr){
                    auto& texDesc = depthTarget.target->GetDesc();
                    return texDesc.height;
                }
                return 0;
            }
        };

    protected:
        //Description description;

        Framebuffer(
            const sp<GraphicsDevice>& dev
        )
            : DeviceResource(dev)
        {}

    public:
        /*TODO: Settle for now*/
        OutputDescription GetOutputDescription() const{
            OutputDescription odesc{};

            auto& sampleCount = odesc.sampleCount;
            auto& description = GetDesc();

            sampleCount = Texture::Description::SampleCount::x1;
            //OutputAttachmentDescription? depthAttachment = null;
            if (description.depthTarget.target != nullptr)
            {
                auto& texDesc = description.depthTarget.target->GetDesc();
                odesc.depthAttachment = OutputDescription::Attachment{ texDesc.format };
                sampleCount = texDesc.sampleCount;
            }
            odesc.colorAttachment.resize(description.colorTargets.size());
            for (int i = 0; i < description.colorTargets.size(); i++)
            {
                auto& texDesc = description.colorTargets[i].target->GetDesc();
                odesc.colorAttachment[i].format = texDesc.format;
                sampleCount = texDesc.sampleCount;
            }
        
            return odesc;
        }
        
        virtual const Description& GetDesc() const = 0;

    };

} // namespace Veldrid
