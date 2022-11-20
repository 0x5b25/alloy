#pragma once

#include <volk.h>

#include "veldrid/Shader.hpp"

#include <string>

namespace Veldrid
{
    
    class VulkanDevice;

    class VulkanShader : public Shader{
        
        VulkanDevice* _Dev() const {return reinterpret_cast<VulkanDevice*>(dev.get());}

    private:
        VkShaderModule _shaderModule;
        //std::string _name;

        //VkShaderModule ShaderModule => _shaderModule;

        VulkanShader(
            const sp<VulkanDevice>& dev,
            const Description& desc
        ) : Shader(dev, desc){}
       

    public:
        const VkShaderModule& GetHandle() const { return _shaderModule; }

        ~VulkanShader();

        static sp<Shader> Make(
            const sp<VulkanDevice>& dev,
            const Shader::Description& desc,
            const std::vector<std::uint32_t>& spvBinary
        );
    };

} // namespace Veldrid

