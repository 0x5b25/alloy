#pragma once

#include "alloy/Types.hpp"
#include "alloy/FixedFunctions.hpp"
#include "alloy/BindableResource.hpp"

#include <cstdint>
#include <string>
 
namespace alloy
{
    

    class ISampler : public IBindableResource
    {

    public:       

        struct Description{

            enum class AddressMode : std::uint8_t{
                Wrap, Mirror, Clamp, Border
            } addressModeU, addressModeV, addressModeW;
            
            // The constant color that is sampled when <see cref="SamplerAddressMode.Border"/> is used, or otherwise ignored.
            enum class BorderColor : std::uint8_t{
                // Transparent Black (0, 0, 0, 0)
                TransparentBlack,
                
                // Opaque Black (0, 0, 0, 1)
                OpaqueBlack,
                
                // Opaque White (1, 1, 1, 1)
                OpaqueWhite,
            } borderColor;
        
            // The filter used when sampling.
            enum class SamplerFilter : std::uint8_t{
                // Point sampling is used for minification, magnification, and mip-level sampling.
                MinPoint_MagPoint_MipPoint,
                
                // Point sampling is used for minification and magnification; linear interpolation is used for mip-level sampling.
                MinPoint_MagPoint_MipLinear,
                
                // Point sampling is used for minification and mip-level sampling; linear interpolation is used for mip-level sampling.
                MinPoint_MagLinear_MipPoint,
                
                // Point sampling is used for minification; linear interpolation is used for magnification and mip-level sampling.
                MinPoint_MagLinear_MipLinear,
                
                // Linear interpolation is used for minifcation; point sampling is used for magnification and mip-level sampling.
                MinLinear_MagPoint_MipPoint,
                
                // Linear interpolation is used for minification and mip-level sampling; point sampling is used for magnification.
                MinLinear_MagPoint_MipLinear,
                
                // Linear interpolation is used for minification and magnification, and point sampling is used for mip-level sampling.
                MinLinear_MagLinear_MipPoint,
                
                // Linear interpolation is used for minification, magnification, and mip-level sampling.
                MinLinear_MagLinear_MipLinear,
                
                // Anisotropic filtering is used. The maximum anisotropy is controlled by
                // <see cref="SamplerDescription.MaximumAnisotropy"/>.
                Anisotropic,
            } filter;
            
            // An optional value controlling the kind of comparison to use when sampling. If null, comparison sampling is not used.
            ComparisonKind* comparisonKind;
        
            // The maximum anisotropy of the filter, when <see cref="SamplerFilter.Anisotropic"/> is used, or otherwise ignored.
            float maximumAnisotropy;
        
            // The minimum level of detail.
            float minimumLod;

            // The maximum level of detail.
            float maximumLod;
        
            // The level of detail bias.
            float lodBias;
            
            
        };

        
        virtual ResourceKind GetResourceKind() const override {return ResourceKind::Sampler;}

        const Description& GetDesc() const {return description;}

        ISampler(const Description& desc) : description(desc) {}

        virtual ~ISampler() {}

    protected:
        Description description;
    };
} // namespace alloy


