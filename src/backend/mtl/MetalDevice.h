#pragma once

#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

#include <string>

#include "alloy/GraphicsDevice.hpp"
#include "alloy/CommandList.hpp"
#include "alloy/CommandQueue.hpp"

#include "MetalResourceFactory.h"

namespace alloy::mtl
{

    class MetalDevice;

    class MetalCmdQ : public alloy::ICommandQueue{

        MetalDevice& _dev;
        id<MTLCommandQueue> _cmdQ;


    public:
        MetalCmdQ(MetalDevice& device); 

        virtual ~MetalCmdQ() override;

        id<MTLCommandQueue> GetHandle() { return _cmdQ; }

        virtual void EncodeSignalEvent(IEvent *fence, uint64_t value) override;
        virtual void EncodeWaitForEvent(IEvent *fence, uint64_t value) override;

        virtual void SubmitCommand(ICommandList *cmd) override;

      /// #TODO: Add command buffer creation to here. Both Metal and Vulkan
      /// demands
      // command buffer are allocated from and submitted to the same queue.

        virtual common::sp<ICommandList> CreateCommandList() override;
        void WaitForIdle();

      //virtual ICaptureScope *
      //CreateCaptureScope(const std::string &label) override {
      //  @autoreleasepool {
      //    auto captureMgr = [MTLCaptureManager sharedCaptureManager];
      //    auto scope = [captureMgr newCaptureScopeWithCommandQueue:_cmdQ];
      //    return new MetalCaptureScope(scope, label);
      //  }
      //  }
    };

    class MetalDevice : public IGraphicsDevice, public MetalResourceFactory {

        id<MTLDevice> _device;
        id<MTLCommandQueue> _cmdQueue;
        

        //GraphicsApiVersion _apiVersion;
        IGraphicsDevice::Features _features;
        
        AdapterInfo _info;
        
        MetalCmdQ* _gfxQ, * _copyQ;

        MetalDevice() = default;

    public:

        virtual ~MetalDevice();

        static common::sp<MetalDevice> Make(
            const IGraphicsDevice::Options& options
        );


        id<MTLDevice> GetHandle() { return _device; }

        virtual const AdapterInfo& GetAdapterInfo() const override {return _info;}
        
        virtual const Features& GetFeatures() const override { return _features; }

        virtual ResourceFactory& GetResourceFactory() override {return *this;}


        virtual ICommandQueue* GetGfxCommandQueue() override;
        virtual ICommandQueue* GetCopyCommandQueue() override;

        virtual ISwapChain::State PresentToSwapChain(ISwapChain* sc) override;
               

        
        virtual void* GetNativeHandle() const override { return _device; }
        virtual void WaitForIdle() override;
    };

    class MetalBuffer : public IBuffer {

        common::sp<MetalDevice> _dev;
        id<MTLBuffer> _mtlBuffer;
        

    public:
        MetalBuffer( const common::sp<MetalDevice>& dev,
                    const IBuffer::Description& desc,
                    id<MTLBuffer> buffer );
        
        virtual ~MetalBuffer() override;
        
        
        id<MTLBuffer> GetHandle() const { return _mtlBuffer; }
        
        static common::sp<MetalBuffer> Make(const common::sp<MetalDevice>& dev,
                                            const IBuffer::Description& desc);
        
        
        virtual void* MapToCPU() override;

        virtual void UnMap() override;
        
        
        virtual void SetDebugName(const std::string& ) override;

    };

    class MetalFence : public alloy::IFence {
        
        common::sp<MetalDevice> _dev;
        id<MTLFence> _mtlFence;

    public:
        id<MTLFence> GetHandle() const { return _mtlFence; }

    };

    class MetalEvent : public alloy::IEvent{

        common::sp<MetalDevice> _dev;

        id<MTLSharedEvent> _mtlEvt;
        //MTLSharedEventListener* _listener;

        MetalEvent(const common::sp<MetalDevice>& dev) : _dev(dev) {}

        virtual ~MetalEvent() override;

    public:

        static common::sp<MetalEvent> Make(const common::sp<MetalDevice>& dev);

        id<MTLSharedEvent> GetHandle() const {return _mtlEvt;}

        virtual uint64_t GetSignaledValue() override;

        virtual bool WaitFromCPU(uint64_t waitForValue, uint32_t timeoutMs) override;
        virtual void SignalFromCPU(uint64_t signalValue) override;

    };
} // namespace alloy

