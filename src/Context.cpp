#include "alloy/Context.hpp"

namespace alloy {

    const GraphicsApiVersion GraphicsApiVersion::Unknown = {};

    common::sp<IContext> CreateMetalContext( const IContext::Options& opts)
#ifndef VLD_BACKEND_MTL
    { return nullptr; }
#else
    ;
#endif

    common::sp<IContext> CreateVulkanContext( const IContext::Options& opts)
#ifndef VLD_BACKEND_VK
    { return nullptr; }
#else
    ;
#endif

    common::sp<IContext> CreateDX12Context( const IContext::Options& opts)
#ifndef VLD_BACKEND_DXC
    { return nullptr; }
#else
    ;
#endif


    common::sp<IContext> IContext::CreateDefault(const IContext::Options& opts) {

#if defined(VLD_PLATFORM_WIN32)
        return IContext::Create(Backend::DX12, opts);
#elif defined(VLD_PLATFORM_MACOS) || defined(VLD_PLATFORM_IOS)
        return IContext::Create(Backend::Metal, opts);
#else
        return IContext::Create(Backend::Vulkan, opts);
#endif
        
        return nullptr;
    }

    common::sp<IContext> IContext::Create(Backend backend, const IContext::Options& opts) {
        switch (backend) {
        case Backend::DX12:   return CreateDX12Context(opts);
        case Backend::Vulkan: return CreateVulkanContext(opts);
        case Backend::Metal:  return CreateMetalContext(opts);
        default: return nullptr;
        }
    }

}
