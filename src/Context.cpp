#include "alloy/Context.hpp"

namespace alloy {

    const GraphicsApiVersion GraphicsApiVersion::Unknown = {};

    common::sp<IContext> CreateMetalContext()
#ifndef VLD_BACKEND_MTL
    { return nullptr; }
#else
    ;
#endif

    common::sp<IContext> CreateVulkanContext()
#ifndef VLD_BACKEND_VK
    { return nullptr; }
#else
    ;
#endif

    common::sp<IContext> CreateDX12Context()
#ifndef VLD_BACKEND_DXC
    { return nullptr; }
#else
    ;
#endif


    common::sp<IContext> IContext::CreateDefault() {

#if defined(VLD_PLATFORM_WIN32)
        return IContext::Create(Backend::DX12);
#elif defined(VLD_PLATFORM_MACOS) || defined(VLD_PLATFORM_IOS)
        return IContext::Create(Backend::Metal);
#else
        return IContext::Create(Backend::Vulkan);
#endif
        
        return nullptr;
    }

    common::sp<IContext> IContext::Create(Backend backend) {
        switch (backend) {
        case Backend::DX12:   return CreateDX12Context();
        case Backend::Vulkan: return CreateVulkanContext();
        case Backend::Metal:  return CreateMetalContext();
        default: return nullptr;
        }
    }

}
