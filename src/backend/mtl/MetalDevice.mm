#include "MetalDevice.h"
#include "alloy/CommandList.hpp"

#include <Foundation/Foundation.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>
#include <IOKit/IOKitLib.h>

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
    common::sp<IContext> CreateMetalContext() {
        return alloy::mtl::MetalContext::Make();
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
        MTLGPUFamily GetGPUArch() const {return _arch;}


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

        bool IsMeshShaderSupported() const {
            return _arch != MTLGPUFamilyApple1
                && _arch != MTLGPUFamilyApple2
                && _arch != MTLGPUFamilyApple3
                && _arch != MTLGPUFamilyApple4
                && _arch != MTLGPUFamilyApple5
                && _arch != MTLGPUFamilyApple6
            ;
        }

        bool IsRayTracingSupported() const {
            return _arch != MTLGPUFamilyApple1
                && _arch != MTLGPUFamilyApple2
                && _arch != MTLGPUFamilyApple3
                && _arch != MTLGPUFamilyApple4
                && _arch != MTLGPUFamilyApple5
            ;
        }

        bool IsUMA() const {
            return _arch != MTLGPUFamilyMac2;
        }

        operator GraphicsApiVersion() const {
            int major = (int)_arch / 1000;
            int minor = (int)_arch % 1000;
            return GraphicsApiVersion{Backend::Metal, major, minor, 0, 0};
        }

    };


    MetalAdapter::MetalAdapter(id<MTLDevice> adp)
        : _adp(adp)
    { 
        PopulateAdpInfo();
    }
    MetalAdapter::~MetalAdapter() {
        [_adp release];
    }

    void MetalAdapter::PopulateAdpInfo() {

        _MTLFeatures feat (_adp);

        //Device info
        @autoreleasepool {
            info.apiVersion = feat;
            info.deviceName = [[_adp name] cStringUsingEncoding:NSUTF8StringEncoding];
            
            auto regID = [_adp registryID];

            auto matching = IORegistryEntryIDMatching(regID);
            io_iterator_t entryIt;

            if(IOServiceGetMatchingServices(kIOMainPortDefault, 
                                            matching,
                                            &entryIt) == kIOReturnSuccess) {
                io_registry_entry_t entry;
                while ((entry = IOIteratorNext(entryIt))) {
                    auto _GetEntryProp = [&entry](CFStringRef propName){
                        auto dataRef = IORegistryEntrySearchCFProperty(
                            entry, 
                            kIOServicePlane, 
                            propName,
                            kCFAllocatorDefault,
                            kIORegistryIterateRecursively | kIORegistryIterateParents
                        );

                        if(!dataRef) return 0u;

                        auto value = 0u;

                        auto pVal = CFDataGetBytePtr(static_cast<CFDataRef>(dataRef));
                        if(pVal) {
                            value = *((uint32_t*)pVal);
                        }
                        CFRelease(dataRef);

                        return value;
                    };

                    if(info.deviceID == 0) {
                        info.deviceID = _GetEntryProp(CFSTR("device-id"));
                    }
                    
                    if(info.vendorID == 0) {
                        info.vendorID = _GetEntryProp(CFSTR("vendor-id"));
                    }

                    IOObjectRelease(entry);

                    if(info.vendorID && info.deviceID) {
                        break;
                    }
                }
                IOObjectRelease(entryIt);
            }
        }

        //Caps
        info.capabilities.supportMeshShader = feat.IsMeshShaderSupported();
        info.capabilities.isUMA = feat.IsUMA();

        auto arch = feat.GetGPUArch();

        //Limits
        auto _SupportsAny = [this](
            const std::vector<MTLGPUFamily>& archList
        )->bool{
            @autoreleasepool {
                for(auto a : archList){
                    if([_adp supportsFamily: a]){
                        return true;
                    }
                }
                return false;
            }
        };
        info.limits.maxImageDimension1D =
            info.limits.maxImageDimension2D = 
                _SupportsAny({MTLGPUFamilyApple3, MTLGPUFamilyMac2}) 
                    ? 16384 : 8192;
        info.limits.maxImageDimension3D = 2048;
        info.limits.maxImageArrayLayers = 2048;
        info.limits.maxFragmentInputComponents = 
            _SupportsAny({MTLGPUFamilyApple4}) 
                    ? 124 : 60;
        info.limits.maxFragmentOutputAttachments = 8;
        //max_bind_groups: 8,
        //max_bindings_per_bind_group: 65535,
        //max_dynamic_uniform_buffers_per_pipeline_layout: base
        //    .max_dynamic_uniform_buffers_per_pipeline_layout,
        //max_dynamic_storage_buffers_per_pipeline_layout: base
        //    .max_dynamic_storage_buffers_per_pipeline_layout;

        auto maxTexPerStage 
            = _SupportsAny({MTLGPUFamilyApple6}) ? 128u 
            : _SupportsAny({MTLGPUFamilyApple4}) ? 96u
            : 31u; 

        info.limits.maxPerStageDescriptorSampledImages = maxTexPerStage;
        info.limits.maxPerStageDescriptorSamplers = 16;
        info.limits.maxPerStageDescriptorStorageBuffers = 31;
        info.limits.maxPerStageDescriptorStorageImages = maxTexPerStage;
        info.limits.maxPerStageDescriptorUniformBuffers = 31;

        auto maxBufferSize = 1u << 28; // 256MB on iOS 8.0+
        if(@available(iOS 12, macOS 10.14, *)) {
            // maxBufferLength available on macOS 10.14+ and iOS 12.0+
            maxBufferSize = [_adp maxBufferLength];
        } else if(@available(macOS 10.11, *)) {
            maxBufferSize = 1u << 30; // 1GB on macOS 10.11 and up
        }

        info.limits.maxUniformBufferRange = maxBufferSize;
        info.limits.maxStorageBufferRange = maxBufferSize;
        info.limits.maxVertexInputBindings = 31;
        info.limits.maxVertexInputAttributes = 31;
        info.limits.maxVertexInputBindingStride = 2048;
        //min_subgroup_size: 4,
        //max_subgroup_size: 64;
        info.limits.maxFragmentInputComponents 
            = _SupportsAny({MTLGPUFamilyApple4, MTLGPUFamilyMac2}) 
                ? 124 : 60;
        info.limits.maxFragmentOutputAttachments
            = _SupportsAny({MTLGPUFamilyApple2, MTLGPUFamilyMac2}) 
                ? 8 : 4;
        
        info.limits.maxPushConstantsSize = 0x1000;
        info.limits.minUniformBufferOffsetAlignment = 4;
        info.limits.minStorageBufferOffsetAlignment = 256;
        //max_color_attachment_bytes_per_sample: self.max_color_attachment_bytes_per_sample
        //    as u32,
        
        info.limits.maxComputeSharedMemorySize
            = _SupportsAny({MTLGPUFamilyApple4, MTLGPUFamilyMac2}) 
                ? 32 << 10 : 16 << 10;

        auto maxThreadInGrp = _SupportsAny({MTLGPUFamilyApple4}) 
            ? 1024 : 512;
        
        info.limits.maxComputeWorkGroupInvocations = maxThreadInGrp;
        info.limits.maxComputeWorkGroupSize[0] = maxThreadInGrp;
        info.limits.maxComputeWorkGroupSize[1] = maxThreadInGrp;
        info.limits.maxComputeWorkGroupSize[2] = maxThreadInGrp;
        //max_compute_workgroups_per_dimension: 0xFFFF,
        //max_non_sampler_bindings: u32::MAX;

        info.limits.pointSizeRange[0] = 0.f;
        info.limits.pointSizeRange[1] = 511.f;
    }


    common::sp<IGraphicsDevice> MetalAdapter::RequestDevice(
        const IGraphicsDevice::Options& options
    ) {
        return MetalDevice::Make(common::ref_sp(this), options);
    }

    MetalDevice::~MetalDevice(){
        delete _gfxQ;
        delete _copyQ;
    }


    common::sp<MetalContext> MetalContext::Make() {
        return common::sp(new MetalContext());
    }

    common::sp<IGraphicsDevice> MetalContext::CreateDefaultDevice(const IGraphicsDevice::Options& options) {
        @autoreleasepool {
            auto device = MTLCreateSystemDefaultDevice();
            auto mtlAdp = new MetalAdapter(device);
            return MetalDevice::Make(common::sp(mtlAdp), options);
        }
    }
    
    std::vector<common::sp<IPhysicalAdapter>> MetalContext::EnumerateAdapters() {
        std::vector<common::sp<IPhysicalAdapter>> adps;
        @autoreleasepool {
            NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();

            for(int i = 0; i < [devices count]; i++) {
                auto dev = devices[i];
                //Argument buffer tier2 support is mandatory for
                //metal shader converter.
                if(dev.argumentBuffersSupport < MTLArgumentBuffersTier2) {
                    [dev release];
                    continue;
                }
                auto mtlAdp = new MetalAdapter(dev);
                adps.emplace_back(mtlAdp);
            }

            [devices release];
        }

        return adps;
    }


    common::sp<MetalDevice> MetalDevice::Make(
        const common::sp<MetalAdapter>& adp,
        const IGraphicsDevice::Options& options
    ){
        @autoreleasepool {
            //auto _device = MTLCreateSystemDefaultDevice();
            //_deviceName = _device->name();
            //enumerate featureset support
            _MTLFeatures MetalFeatures(adp->GetHandle());
            
            //GraphicsApiVersion _apiVersion = MetalFeatures;// new GraphicsApiVersion(major, minor, 0, 0);
            
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
            mtlDev->_adp = adp;
            //mtlDev->_cmdQueue = [adp->GetHandle() newCommandQueue];
            //_device->newCommandQueue();
            mtlDev->_info.apiVersion = MetalFeatures;
            mtlDev->_info.deviceName = [[adp->GetHandle() name] cStringUsingEncoding:NSUTF8StringEncoding];
            mtlDev->_info.deviceID = [adp->GetHandle() registryID];
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
