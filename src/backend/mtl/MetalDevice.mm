#include "MetalDevice.h"
#include "alloy/CommandList.hpp"

#include <Foundation/Foundation.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

#import <Metal/Metal.h>

#include "alloy/common/Macros.h"
#include "alloy/common/Waitable.hpp"
#include "alloy/backend/Backends.hpp"
#include "MtlSurfaceUtil.hpp"
#include "MetalCommandList.h"
#include "MetalSwapChain.h"

#include <set>
#include <chrono>

namespace alloy {
    common::sp<IGraphicsDevice> CreateMetalGraphicsDevice(
        const IGraphicsDevice::Options& options
    ) {
        return alloy::mtl::MetalDevice::Make(options);
    }
}

namespace alloy::mtl
{
    
	

    class _MTLFeatures{
        
        MTLGPUFamily _arch;
        
        static MTLGPUFamily _QueryGPUArch(id<MTLDevice> dev){
            MTLGPUFamily arch = MTLGPUFamilyCommon1;
            /*
             MTLGPUFamilyApple8             Represents the Apple family 8 GPU features that correspond to the Apple A15 and M2 GPUs.
             MTLGPUFamilyApple7             Represents the Apple family 7 GPU features that correspond to the Apple A14 and M1 GPUs.
             MTLGPUFamilyApple6             Represents the Apple family 6 GPU features that correspond to the Apple A13 GPUs.
             MTLGPUFamilyApple5             Represents the Apple family 5 GPU features that correspond to the Apple A12 GPUs.
             MTLGPUFamilyApple4             Represents the Apple family 4 GPU features that correspond to the Apple A11 GPUs.
             MTLGPUFamilyApple3             Represents the Apple family 3 GPU features that correspond to the Apple A9 and A10 GPUs.
             MTLGPUFamilyApple2             Represents the Apple family 2 GPU features that correspond to the Apple A8 GPUs.
             MTLGPUFamilyApple1             Represents the Apple family 1 GPU features that correspond to the Apple A7 GPUs.
             */
            static const MTLGPUFamily _Archs [] = {
                //Detects Apple arch first
                MTLGPUFamilyApple1,
                MTLGPUFamilyApple2,
                MTLGPUFamilyApple3,
                MTLGPUFamilyApple4,
                MTLGPUFamilyApple5,
                MTLGPUFamilyApple6,
                MTLGPUFamilyApple7,
                MTLGPUFamilyApple8,
                //Then non-apple mac gpus
                MTLGPUFamilyMac2
            };
            
            for(auto a : _Archs){
                if([dev supportsFamily: a]){
                    arch = a;
                }else break;
            }
            
            return arch;
        }
        
        static MTLGPUFamily _QueryGPUArchLegacy(id<MTLDevice> dev){
            
            MTLGPUFamily arch = MTLGPUFamilyCommon1;
            
            auto _queryFeat = [&](const std::vector<MTLFeatureSet>& set){
                for(auto& f : set){
                    if([dev supportsFeatureSet:f]){
                        return true;
                    }
                
                }
                return false;
            };
            
            /*
             GPU family                  GPU hardware
             
             iOS GPU family 1            Apple A7 devices
             
             iOS GPU family 2
             tvOS GPU family 1           Apple A8 devices
             
             iOS GPU family 3            Apple A10 devices
             tvOS GPU family 2           Apple A9 devices
             
             iOS GPU family 4            Apple A11 devices
             
             iOS GPU family 5            Apple A12 devices
             
             macOS GPU family 1          iMac Pro models
                                         iMac models from 2012 or later
                                         MacBook models from 2015 or later
                                         MacBook Pro models from 2012 or later
                                         MacBook Air models from 2012 or later
                                         Mac mini models from 2012 or later
                                         Mac Pro models from late 2013
             
             macOS GPU family 2          iMac models from 2015 or later
                                         MacBook Pro models from 2016 or later
                                         MacBook models from 2016 or later
                                         iMac Pro models from 2017 or later
             
             */
            do{
#if defined(VLD_PLATFORM_MACOS)
                //Query for macOS series
                if(_queryFeat({
                    MTLFeatureSet_macOS_GPUFamily1_v1
                    ,MTLFeatureSet_macOS_GPUFamily1_v2
                    ,MTLFeatureSet_macOS_GPUFamily1_v3
                    ,MTLFeatureSet_macOS_GPUFamily1_v4
                    ,MTLFeatureSet_macOS_GPUFamily2_v1
                })){
                    arch = MTLGPUFamilyMac2;
                }else break;
#endif
                
#if defined(VLD_PLATFORM_IOS)
                //Query for apple A series
                //A7
                if(_queryFeat({
                    MTLFeatureSet_iOS_GPUFamily1_v1
                    ,MTLFeatureSet_iOS_GPUFamily1_v2
                    ,MTLFeatureSet_iOS_GPUFamily1_v3
                    ,MTLFeatureSet_iOS_GPUFamily1_v4
                    ,MTLFeatureSet_iOS_GPUFamily1_v5
                })){
                    arch = MTLGPUFamilyApple1;
                }else break;
                
                //A8
                if(_queryFeat({
                    MTLFeatureSet_iOS_GPUFamily2_v1
                    ,MTLFeatureSet_iOS_GPUFamily2_v2
                    ,MTLFeatureSet_iOS_GPUFamily2_v3
                    ,MTLFeatureSet_iOS_GPUFamily2_v4
                    ,MTLFeatureSet_iOS_GPUFamily2_v5
                    
                    ,MTLFeatureSet_tvOS_GPUFamily1_v1
                    ,MTLFeatureSet_tvOS_GPUFamily1_v2
                    ,MTLFeatureSet_tvOS_GPUFamily1_v3
                    ,MTLFeatureSet_tvOS_GPUFamily1_v4
                })){
                    arch = MTLGPUFamilyApple2;
                }else break;
                
                //A9 & A10
                if(_queryFeat({
                    MTLFeatureSet_iOS_GPUFamily3_v1
                    ,MTLFeatureSet_iOS_GPUFamily3_v2
                    ,MTLFeatureSet_iOS_GPUFamily3_v3
                    ,MTLFeatureSet_iOS_GPUFamily3_v4
                    
                    ,MTLFeatureSet_tvOS_GPUFamily2_v1
                    ,MTLFeatureSet_tvOS_GPUFamily2_v2
                })){
                    arch = MTLGPUFamilyApple3;
                }else break;
                
                //A11
                if(_queryFeat({
                    MTLFeatureSet_iOS_GPUFamily4_v1
                    ,MTLFeatureSet_iOS_GPUFamily4_v2
                })){
                    arch = MTLGPUFamilyApple4;
                }else break;
                
                //A12
                if(_queryFeat({
                    MTLFeatureSet_iOS_GPUFamily5_v1
                })){
                    arch = MTLGPUFamilyApple5;
                }else break;
#endif
            }while(0);
            
            return arch;
        }
        
    public:
        _MTLFeatures(id<MTLDevice> dev){
            //Detect GPU architectures
                        
            if(@available(iOS 13, macOS 10.15, *)){
                _arch = _QueryGPUArch(dev);
            }else{
                _arch = _QueryGPUArchLegacy(dev);
            }

        }


        static constexpr bool IsMacOS(){
#if defined(VLD_PLATFORM_MACOS)
            return true;
#else
            return false;
#endif
        }

        bool IsDrawBaseVertexInstanceSupported() const
        {
            return _arch != MTLGPUFamilyCommon1
                && _arch != MTLGPUFamilyApple1;
        }
        
        bool IsMultiViewportSupported() const {
            return _arch != MTLGPUFamilyCommon1
                && _arch != MTLGPUFamilyCommon2
                && _arch != MTLGPUFamilyApple1
                && _arch != MTLGPUFamilyApple2
                && _arch != MTLGPUFamilyApple3
            ;
        }

        operator GraphicsApiVersion() const {
            int major = (int)_arch / 1000;
            int minor = (int)_arch % 1000;
            return GraphicsApiVersion{major, minor, 0, 0};
        }

    };

    MetalDevice::~MetalDevice(){
        delete _gfxQ;
        delete _copyQ;
        [_device release];
    }


    common::sp<MetalDevice> MetalDevice::Make(
        const IGraphicsDevice::Options& options
    ){
        @autoreleasepool {
            auto _device = MTLCreateSystemDefaultDevice();
            //_deviceName = _device->name();
            //enumerate featureset support
            _MTLFeatures MetalFeatures(_device);
            
            GraphicsApiVersion _apiVersion = MetalFeatures;// new GraphicsApiVersion(major, minor, 0, 0);
            
            IGraphicsDevice::Features _features;
            
            _features.computeShader = true;
            _features.geometryShader = false;
            _features.tessellationShaders = false;
            _features.multipleViewports = MetalFeatures.IsMultiViewportSupported();
            _features.samplerLodBias = false;
            _features.drawBaseVertex = MetalFeatures.IsDrawBaseVertexInstanceSupported();
            _features.drawBaseInstance = MetalFeatures.IsDrawBaseVertexInstanceSupported();
            _features.drawIndirect = true;
            _features.drawIndirectBaseInstance = true;
            _features.fillModeWireframe = true;
            _features.samplerAnisotropy = true;
            _features.depthClipDisable = true;
            _features.texture1D = true, // TODO: Should be macOS 10.11+ and iOS 11.0+;
            _features.independentBlend = true;
            _features.structuredBuffer = true;
            _features.subsetTextureView = true;
            _features.commandListDebugMarkers = true;
            _features.bufferRangeBinding = true;
            _features.shaderFloat64 = false;
            //    ResourceBindingModel = options.ResourceBindingModel;
            
            MTLCommandBufferHandler _completionHandler;
            
            
            //_libSystem = new NativeLibrary("libSystem.dylib");
            //_concreteGlobalBlock = _libSystem.LoadFunction("_NSConcreteGlobalBlock");
            if (MetalFeatures.IsMacOS())
            {
                //_completionHandler = OnCommandBufferCompleted;
            }
            else
            {
                //_completionHandler = OnCommandBufferCompleted_Static;
            }
            //_completionHandlerFuncPtr = Marshal.GetFunctionPointerForDelegate<MTLCommandBufferHandler>(_completionHandler);
            //_completionBlockDescriptor = Marshal.AllocHGlobal(Unsafe.SizeOf<BlockDescriptor>());
            /*
             BTNodeDescriptor* descriptorPtr = (BlockDescriptor*)_completionBlockDescriptor;
             descriptorPtr->reserved = 0;
             descriptorPtr->Block_size = (ulong)Unsafe.SizeOf<BlockDescriptor>();
             
             _completionBlockLiteral = Marshal.AllocHGlobal(Unsafe.SizeOf<BlockLiteral>());
             BlockLiteral* blockPtr = (BlockLiteral*)_completionBlockLiteral;
             blockPtr->isa = _concreteGlobalBlock;
             blockPtr->flags = 1 << 28 | 1 << 29;
             blockPtr->invoke = _completionHandlerFuncPtr;
             blockPtr->descriptor = descriptorPtr;
             
             if (!MetalFeatures.IsMacOS())
             {
             lock (s_aotRegisteredBlocks)
             {
             s_aotRegisteredBlocks.Add(_completionBlockLiteral, this);
             }
             }
             
             ResourceFactory = new MTLResourceFactory(this);
             
             TextureSampleCount[] allSampleCounts = (TextureSampleCount[])Enum.GetValues(typeof(TextureSampleCount));
             _supportedSampleCounts = new bool[allSampleCounts.Length];
             for (int i = 0; i < allSampleCounts.Length; i++)
             {
             TextureSampleCount count = allSampleCounts[i];
             uint uintValue = FormatHelpers.GetSampleCountUInt32(count);
             if (_device.supportsTextureSampleCount((UIntPtr)uintValue))
             {
             _supportedSampleCounts[i] = true;
             }
             }
             */
            //MtlSurfaceContainer container = {};
            //if (swapChainSource != nullptr)
            //{
            //    //SwapchainDescription desc = swapchainDesc.Value;
            //    container = CreateSurface(_device, swapChainSource);
            //}
            
            auto mtlDev = new MetalDevice();
            mtlDev->_device = _device;
            mtlDev->_cmdQueue = [_device newCommandQueue];
            //_device->newCommandQueue();
            mtlDev->_info.apiVersion = _apiVersion;
            mtlDev->_info.deviceName = [[_device name] cStringUsingEncoding:kCFStringEncodingUTF8];
            mtlDev->_info.deviceID = [_device registryID];
            mtlDev->_features = _features;
            
            mtlDev->_gfxQ = new MetalCmdQ(*mtlDev);
            mtlDev->_copyQ = new MetalCmdQ(*mtlDev);
            
            return common::sp(mtlDev);
            
            //_metalInfo = new BackendInfoMetal(this);
            
            /// Creates and caches common device resources after device creation completes.
            //PostDeviceCreated();
        }
    }



    ICommandQueue* MetalDevice::GetGfxCommandQueue() {
        return _gfxQ;
    }
    ICommandQueue* MetalDevice::GetCopyCommandQueue() {
        return _copyQ;
    }

    ISwapChain::State MetalDevice::PresentToSwapChain(ISwapChain* sc) {
        auto mtlSC = common::PtrCast<MetalSwapChain>(sc);
        mtlSC->PresentBackBuffer();
        return ISwapChain::State::Optimal;
    }


    void MetalDevice::WaitForIdle() {
        _gfxQ->WaitForIdle();
    }

    common::sp<ICommandList> MetalCmdQ::CreateCommandList() {
        
        @autoreleasepool {
            auto dev = common::ref_sp(&_dev);
            auto cmdBuf = [[_cmdQ commandBuffer] retain];
            //[_cmdBuf retain];
            
            auto impl = new MetalCommandList(dev, cmdBuf);
            return common::sp<ICommandList>(impl);
        }
    }


    MetalCmdQ::MetalCmdQ(MetalDevice& device)
        : _dev(device) 
    {
        _cmdQ = [_dev.GetHandle() newCommandQueue];
    }


    MetalCmdQ::~MetalCmdQ() { [_cmdQ release]; }


    void MetalCmdQ::EncodeSignalEvent(IEvent *fence, uint64_t value) {
    auto mtlEvt = common::PtrCast<MetalEvent>(fence);
    
        @autoreleasepool {
            auto dummyCmdBuf = [_cmdQ commandBuffer];
            [dummyCmdBuf encodeSignalEvent:mtlEvt->GetHandle() value:value];
            [dummyCmdBuf commit];
        }
    }

    void MetalCmdQ::EncodeWaitForEvent(IEvent *fence, uint64_t value) {
        auto mtlEvt = common::PtrCast<MetalEvent>(fence);
    
        @autoreleasepool {
            auto dummyCmdBuf = [_cmdQ commandBuffer] ;
            [dummyCmdBuf encodeWaitForEvent:mtlEvt->GetHandle() value:value];
            [dummyCmdBuf commit];
        }
    }

    void MetalCmdQ::SubmitCommand(
        ICommandList *cmd
    ){
        @autoreleasepool {
            auto mtlCmdBuf = static_cast<MetalCommandList*>(cmd);
            auto rawCmdBuf = mtlCmdBuf->GetHandle();
            
            
            //auto stat = [rawCmdBuf status];
            [rawCmdBuf commit];
            //If multiple submissions, pick gang leader
        }
        
    }


void MetalCmdQ::WaitForIdle() {
    @autoreleasepool {
        auto dummyCmdBuf = [[_cmdQ commandBuffer] retain];
        [dummyCmdBuf commit];
        [dummyCmdBuf waitUntilCompleted];
        [dummyCmdBuf release];
    }
}

MetalBuffer::MetalBuffer( const common::sp<MetalDevice>& dev,
            const IBuffer::Description& desc,
            id<MTLBuffer> buffer )
    : IBuffer(desc)
    , _dev(dev)
    , _mtlBuffer(buffer)
{}

MetalBuffer::~MetalBuffer() {
    [_mtlBuffer release];
}


    common::sp<MetalBuffer> MetalBuffer::Make(
        const common::sp<MetalDevice>& dev,
        const IBuffer::Description& desc
    ) {
        @autoreleasepool {
            
            MTLResourceOptions mtlDesc{};
            
            switch(desc.hostAccess) {
                case alloy::HostAccess::PreferRead:
                    mtlDesc |= MTLResourceStorageModeShared
                             | MTLResourceCPUCacheModeDefaultCache;
                    break;
                    
                case alloy::HostAccess::PreferWrite:
                    mtlDesc |= MTLResourceStorageModeShared
                             | MTLResourceCPUCacheModeWriteCombined;
                    break;
                        
                case alloy::HostAccess::None:
                    mtlDesc |= MTLResourceStorageModePrivate;
                    break;
            }
            
            mtlDesc |= MTLResourceHazardTrackingModeTracked;
            
            auto alignedSize = ((desc.sizeInBytes + 255)/256) * 256;
            
            auto buffer = [dev->GetHandle() newBufferWithLength:alignedSize options:mtlDesc];
            //[buffer retain];
            
            auto alBuf = new MetalBuffer(dev, desc, buffer);
            alBuf->description.sizeInBytes = alignedSize;
            return common::sp(alBuf);
        }
    }

    void* MetalBuffer::MapToCPU() {
        @autoreleasepool {
            return [_mtlBuffer contents];
        }
    }

    void MetalBuffer::UnMap() {}



    void MetalBuffer::SetDebugName(const std::string& name) {
        @autoreleasepool {
            auto nsSrc = [NSString stringWithUTF8String:name.c_str()];
            [_mtlBuffer setLabel:nsSrc];
        }
    }


    MetalEvent::~MetalEvent() {
        //[_listener release];
        [_mtlEvt release];
    }


    common::sp<MetalEvent> MetalEvent::Make(const common::sp<MetalDevice>& dev) {
        @autoreleasepool {
            auto mtlDev = dev->GetHandle();
            
            auto mtlEvt = [mtlDev newSharedEvent];
            
            auto evtImpl = new MetalEvent(dev);
            
            evtImpl->_mtlEvt = mtlEvt;
            //evtImpl->_listener = [MTLSharedEventListener new];
            
            return common::sp(evtImpl);
        }

    }


    uint64_t MetalEvent::GetSignaledValue(){
       return  _mtlEvt.signaledValue;
    }

    bool MetalEvent::WaitFromCPU(uint64_t waitForValue, uint32_t timeoutMs){
        
        @autoreleasepool {
            common::ManualResetLatch latch;
            
            auto* pLatch = &latch;
            
            auto listener = [[MTLSharedEventListener new] autorelease];
            
            [_mtlEvt notifyListener:listener atValue:waitForValue block:^(id<MTLSharedEvent> evt, uint64_t value){
                
                pLatch->Signal();
                
            }];
            
            auto duration = std::chrono::milliseconds(timeoutMs);
            
            bool timedout = latch.WaitWithTimeout(duration);
            
            return !timedout;
        }
    }

    void MetalEvent::SignalFromCPU(uint64_t signalValue){
        _mtlEvt.signaledValue = signalValue;
    }


    

} // namespace alloy::mtl
