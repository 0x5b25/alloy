#include "VkCommon.hpp"

#include <volk.h>




#include <cstdint>




std::vector<std::string> EnumerateInstanceLayers()
{

    std::uint32_t propCount = 0;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&propCount, nullptr));
    if (propCount == 0) {
        return {};
    }

    std::vector<VkLayerProperties> props(propCount);

    VK_CHECK(vkEnumerateInstanceLayerProperties(&propCount, props.data()));

    std::vector<std::string> ret; ret.reserve(propCount);
    for (int i = 0; i < propCount; i++)
    {
        auto& prop = props[i];
        ret.emplace_back(prop.layerName);
    }

    return ret;
}

std::vector<std::string> EnumerateInstanceExtensions(){
    
    std::uint32_t propCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &propCount, nullptr));
    
    if (propCount == 0) {
        return {};
    }

    std::vector<VkExtensionProperties> props(propCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &propCount, props.data()));

    std::vector<std::string> ret; ret.reserve(propCount);
    for (int i = 0; i < propCount; i++)
    {
        auto& prop = props[i];
        ret.emplace_back(prop.extensionName);
    }

    return ret;
}

