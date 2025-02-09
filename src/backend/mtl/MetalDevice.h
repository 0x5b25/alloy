#pragma once

#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

#include <string>

#include "alloy/Context.hpp"
#include "alloy/GraphicsDevice.hpp"
#include "alloy/CommandList.hpp"
#include "alloy/CommandQueue.hpp"

#include "MetalResourceFactory.h"

namespace alloy::mtl
{

    class MetalDevice;

    class MetalAdapter : public alloy::IPhysicalAdapter {

        id<MTLDevice> _adp;

        void PopulateAdpInfo();

    public:
        MetalAdapter(id<MTLDevice> adp);
        virtual ~MetalAdapter() override;

        id<MTLDevice> GetHandle() const {return _adp;}


        virtual common::sp<IGraphicsDevice> RequestDevice(const IGraphicsDevice::Options& options) override;

    };

    class MetalContext : public alloy::IContext {

    public:
        static common::sp<MetalContext> Make();

        virtual common::sp<IGraphicsDevice> CreateDefaultDevice(const IGraphicsDevice::Options& options) override;
        virtual std::vector<common::sp<IPhysicalAdapter>> EnumerateAdapters() override;
    };

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

        //id<MTLDevice> _device;
        common::sp<MetalAdapter> _adp;
        //GraphicsApiVersion _apiVersion;
        IGraphicsDevice::Features _features;
        
        AdapterInfo _info;
        
        MetalCmdQ* _gfxQ, * _copyQ;

        MetalDevice() = default;

    public:

        virtual ~MetalDevice();

        static common::sp<MetalDevice> Make(
            const common::sp<MetalAdapter>& adp,
            const IGraphicsDevice::Options& options
        );

        id<MTLDevice> GetHandle() const { return _adp->GetHandle(); }
        
        virtual const Features& GetFeatures() const override { return _features; }
        virtual IPhysicalAdapter& GetAdapter() const override {return *_adp.get();}


        virtual ResourceFactory& GetResourceFactory() override {return *this;}


        virtual ICommandQueue* GetGfxCommandQueue() override;
        virtual ICommandQueue* GetCopyCommandQueue() override;

        virtual ISwapChain::State PresentToSwapChain(ISwapChain* sc) override;
               

        
        virtual void* GetNativeHandle() const override { return GetHandle(); }
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

