#pragma once

#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

#include "veldrid/GraphicsDevice.hpp"


namespace Veldrid
{
    class MetalDevice : public GraphicsDevice{

        id<MTLDevice> _device;
        id<MTLCommandQueue> _cmdQueue;
        
        CAMetalLayer* _metalLayer;
        bool _isOwnSurface;

        GraphicsApiVersion _apiVersion;
        GraphicsDevice::Features _features;

        MetalDevice() = default;

    public:

        ~MetalDevice();

        static sp<GraphicsDevice> Make(
            const GraphicsDevice::Options& options,
            SwapChainSource* swapChainSource = nullptr
        );

        virtual const std::string& DeviceName() const override;
        virtual const std::string& VendorName() const override;
        virtual const GraphicsApiVersion ApiVersion() const override {return _apiVersion;}
        virtual const Features& GetFeatures() const override {return _features;}

        virtual ResourceFactory* GetResourceFactory() override;
        
        virtual void SubmitCommand(
            const std::vector<CommandList*>& cmd,
            const std::vector<Semaphore*>& waitSemaphores,
            const std::vector<Semaphore*>& signalSemaphores,
            Fence* fence) override;
        virtual SwapChain::State PresentToSwapChain(
            const std::vector<Semaphore*>& waitSemaphores,
            SwapChain* sc) override;
               


        virtual void WaitForIdle() override;
    };
} // namespace Veldrid

