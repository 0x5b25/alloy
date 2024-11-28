#pragma once

#include <volk.h>

#include "veldrid/Shader.hpp"

#include <string>
#include <span>

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
            const sp<GraphicsDevice>& dev,
            const Description& desc
        ) : Shader(dev, desc){}
       

    public:
        const VkShaderModule& GetHandle() const { return _shaderModule; }

        ~VulkanShader();

        static sp<Shader> Make(
            const sp<VulkanDevice>& dev,
            const Shader::Description& desc,
            const std::span<std::uint8_t>& il
        );

    };

} // namespace Veldrid

