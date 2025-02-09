#pragma once

#include <vector>

#include "common/RefCnt.hpp"
#include "Types.hpp"
#include "GraphicsDevice.hpp"

namespace alloy {

    enum class Backend{
        /// <summary>
        /// Direct3D 11.
        /// </summary>
        //Direct3D11,
        /// <summary>
        /// Direct3D 12.
        /// </summary>
        DX12,
        /// <summary>
        /// Vulkan.
        /// </summary>
        Vulkan,
        /// <summary>
        /// OpenGL.
        /// </summary>
        //OpenGL,
        /// <summary>
        /// Metal.
        /// </summary>
        Metal,
        /// <summary>
        /// OpenGL ES.
        /// </summary>
        //OpenGLES,
    };

    struct MemorySegmentProperties {
        struct {
            uint32_t isInSysMem : 1;
            uint32_t isCPUVisible : 1;
        } flags;

        uint64_t sizeInBytes;
        uint64_t budgetInBytes;
    };

    struct GraphicsApiVersion {
        static const GraphicsApiVersion Unknown;

        Backend backend;

        int major;
        int minor;
        int subminor;
        int patch;

        bool IsKnown() {return major != 0 && minor != 0 && subminor != 0 && patch != 0; }        

        operator std::string()
        {
            std::stringstream ss;
            switch(backend) {
                case Backend::DX12: ss << "DirectX "; break;
                case Backend::Vulkan: ss << "Vulkan "; break;
                case Backend::Metal: ss << "Metal "; break;
            }
            ss << major << "." << minor << "." << subminor << "." << patch;
            return ss.str();
        }
    };

    struct AdapterInfo{

        struct Limits {
            uint32_t              maxImageDimension1D;
            uint32_t              maxImageDimension2D;
            uint32_t              maxImageDimension3D;
            uint32_t              maxImageDimensionCube;
            uint32_t              maxImageArrayLayers;
            uint32_t              maxTexelBufferElements;
            uint32_t              maxUniformBufferRange;
            uint32_t              maxStorageBufferRange;
            uint32_t              maxPushConstantsSize;
            uint32_t              maxMemoryAllocationCount;
            uint32_t              maxSamplerAllocationCount;
            size_t                bufferImageGranularity;
            size_t                sparseAddressSpaceSize;
            uint32_t              maxBoundDescriptorSets;
            uint32_t              maxPerStageDescriptorSamplers;
            uint32_t              maxPerStageDescriptorUniformBuffers;
            uint32_t              maxPerStageDescriptorStorageBuffers;
            uint32_t              maxPerStageDescriptorSampledImages;
            uint32_t              maxPerStageDescriptorStorageImages;
            uint32_t              maxPerStageDescriptorInputAttachments;
            uint32_t              maxPerStageResources;
            uint32_t              maxVertexInputAttributes;
            uint32_t              maxVertexInputBindings;
            uint32_t              maxVertexInputAttributeOffset;
            uint32_t              maxVertexInputBindingStride;
            uint32_t              maxVertexOutputComponents;
            uint32_t              maxFragmentInputComponents;
            uint32_t              maxFragmentOutputAttachments;
            uint32_t              maxFragmentDualSrcAttachments;
            uint32_t              maxFragmentCombinedOutputResources;
            uint32_t              maxComputeSharedMemorySize;
            //uint32_t              maxComputeWorkGroupCount[3];
            uint32_t              maxComputeWorkGroupInvocations;
            uint32_t              maxComputeWorkGroupSize[3];
            uint32_t              maxDrawIndexedIndexValue;
            uint32_t              maxDrawIndirectCount;
            float                 maxSamplerLodBias;
            float                 maxSamplerAnisotropy;
            uint32_t              maxViewports;
            uint32_t              maxViewportDimensions[2];
            size_t                minMemoryMapAlignment;
            size_t                minTexelBufferOffsetAlignment;
            size_t                minUniformBufferOffsetAlignment;
            size_t                minStorageBufferOffsetAlignment;
            uint32_t              maxFramebufferWidth;
            uint32_t              maxFramebufferHeight;
            uint32_t              maxFramebufferLayers;
            SampleCount           framebufferColorSampleCounts;
            SampleCount           framebufferDepthSampleCounts;
            SampleCount           framebufferStencilSampleCounts;
            SampleCount           framebufferNoAttachmentsSampleCounts;
            uint32_t              maxColorAttachments;
            SampleCount           sampledImageColorSampleCounts;
            SampleCount           sampledImageIntegerSampleCounts;
            SampleCount           sampledImageDepthSampleCounts;
            SampleCount           sampledImageStencilSampleCounts;
            SampleCount           storageImageSampleCounts;
            uint32_t              maxSampleMaskWords;
            float                 timestampPeriod;
            uint32_t              maxClipDistances;
            uint32_t              maxCullDistances;
            uint32_t              maxCombinedClipAndCullDistances;
            float                 pointSizeRange[2];
            float                 lineWidthRange[2];
            float                 pointSizeGranularity;
            float                 lineWidthGranularity;
        } limits;

        struct Capabilities {
            uint32_t supportMeshShader : 1;
            uint32_t supportBindless : 1;
            uint32_t supportDedicatedTransferQueue : 1;
            uint32_t isUMA : 1;
        } capabilities;

        std::vector<MemorySegmentProperties> memSegments;
/*
// Provided by VK_VERSION_1_0
typedef struct VkPhysicalDeviceProperties {
    uint32_t                            apiVersion;
    uint32_t                            driverVersion;
    uint32_t                            vendorID;
    uint32_t                            deviceID;
    VkPhysicalDeviceType                deviceType;
    char                                deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
    uint8_t                             pipelineCacheUUID[VK_UUID_SIZE];
    VkPhysicalDeviceLimits              limits;
    VkPhysicalDeviceSparseProperties    sparseProperties;
} VkPhysicalDeviceProperties;
*/
        GraphicsApiVersion apiVersion;
        std::uint64_t driverVersion;
        std::uint32_t vendorID;
        std::uint32_t deviceID;
        std::string deviceName;
    };

    class IPhysicalAdapter : public common::RefCntBase {

    protected:
        AdapterInfo info;

    public:
        const AdapterInfo& GetAdapterInfo() const {return info;}

        virtual common::sp<IGraphicsDevice> RequestDevice(const IGraphicsDevice::Options& options) = 0;
    };


    class IContext : public common::RefCntBase {
    public:
        static common::sp<IContext> CreateDefault();

        static common::sp<IContext> Create(Backend backend);

        virtual common::sp<IGraphicsDevice> CreateDefaultDevice(const IGraphicsDevice::Options& options) = 0;
        virtual std::vector<common::sp<IPhysicalAdapter>> EnumerateAdapters() = 0;
    };

}
