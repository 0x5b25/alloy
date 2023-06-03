#include "MetalDevice.hpp"

#include <Foundation/Foundation.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

#import <Metal/Metal.h>

#include "veldrid/common/Macros.h"
#include "veldrid/backend/Backends.hpp"
#include "MtlSurfaceUtil.hpp"

#include <set>

void Test(){
    id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
}


namespace Veldrid
{
    
	sp<GraphicsDevice> CreateMetalGraphicsDevice(
        const GraphicsDevice::Options& options,
        SwapChainSource* swapChainSource
    ) {
		return MetalDevice::Make(options, swapChainSource);
	}

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
        [_device release];
    }


    sp<GraphicsDevice> MetalDevice::Make(
        const GraphicsDevice::Options& options,
        SwapChainSource* swapChainSource
    ){
        
        auto _device = MTLCreateSystemDefaultDevice();
        //_deviceName = _device->name();
        //enumerate featureset support
        _MTLFeatures MetalFeatures(_device);

        GraphicsApiVersion _apiVersion = MetalFeatures;// new GraphicsApiVersion(major, minor, 0, 0);

        GraphicsDevice::Features _features;

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
        MtlSurfaceContainer container = {};
        if (swapChainSource != nullptr)
        {
            //SwapchainDescription desc = swapchainDesc.Value;
            container = CreateSurface(_device, swapChainSource);
        }
        
        auto mtlDev = new MetalDevice();
        mtlDev->_device = _device;
        mtlDev->_cmdQueue = [_device newCommandQueue];
        mtlDev->_metalLayer = container.layer;
        mtlDev->_isOwnSurface = container.isOwnSurface;
        //_device->newCommandQueue();
        mtlDev->_apiVersion = _apiVersion;
        mtlDev->_features = _features;

        return sp(mtlDev);

        //_metalInfo = new BackendInfoMetal(this);

        /// Creates and caches common device resources after device creation completes.
        //PostDeviceCreated();
    }

    void MetalDevice::SubmitCommand(
        const std::vector<CommandList *> &cmd,
        const std::vector<Semaphore *> &waitSemaphores,
        const std::vector<Semaphore *> &signalSemaphores,
        Fence *fence
    ){
        assert(!cmd.empty());
        bool isGangedSubmission = !waitSemaphores.empty() || !signalSemaphores.empty();
        
        //If multiple submissions, pick gang leader
        
    }

} // namespace Veldrid
