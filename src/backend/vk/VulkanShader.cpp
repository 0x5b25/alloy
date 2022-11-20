#include "VulkanShader.hpp"

#include "VulkanDevice.hpp"
#include "VkCommon.hpp"


namespace Veldrid
{
    

    VulkanShader::~VulkanShader(){
        vkDestroyShaderModule(_Dev()->LogicalDev(), _shaderModule, nullptr);
    }

    sp<Shader> VulkanShader::Make(
        const sp<VulkanDevice>& dev,
        const Shader::Description& desc,
        const std::vector<std::uint32_t>& spvBinary
    ){
        VkShaderModuleCreateInfo shaderModuleCI {};
        shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCI.codeSize = spvBinary.size() * sizeof(std::uint32_t);
        shaderModuleCI.pCode = spvBinary.data();
        VkShaderModule module;
        VK_CHECK(vkCreateShaderModule(dev->LogicalDev(), &shaderModuleCI, nullptr, &module));

        auto shader = new VulkanShader(dev, desc);
        shader->_shaderModule = module;

        return sp<Shader>(shader);
    }

} // namespace Veldrid

