#pragma once

#include <string>
#include <sstream>
#include <vector>

#include "veldrid/common/Macros.h"
#include "veldrid/common/RefCnt.hpp"

#include "veldrid/Types.hpp"
#include "veldrid/ResourceFactory.hpp"

namespace Veldrid
{
    struct GraphicsApiVersion
    {
        static const GraphicsApiVersion Unknown;

        int major;
        int minor;
        int subminor;
        int patch;

        bool IsKnown() {return major != 0 && minor != 0 && subminor != 0 && patch != 0; }        

        operator std::string()
        {
            std::stringstream ss;
            ss << major << "." << minor << "." << subminor << "." << patch;
            return ss.str();
        }

        
    };

    class GraphicsDevice : public RefCntBase{
        DISABLE_COPY_AND_ASSIGN(GraphicsDevice);

    public:
        struct AdapterInfo{
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

        struct Features{
            union{
                struct {
                    std::uint32_t computeShader            : 1;
                    std::uint32_t geometryShader           : 1;
                    std::uint32_t tessellationShaders      : 1;
                    std::uint32_t multipleViewports        : 1;
                    std::uint32_t samplerLodBias           : 1;
                    std::uint32_t drawBaseVertex           : 1;
                    std::uint32_t drawBaseInstance         : 1;
                    std::uint32_t drawIndirect             : 1;
                    std::uint32_t drawIndirectBaseInstance : 1;
                    std::uint32_t fillModeWireframe        : 1;
                    std::uint32_t samplerAnisotropy        : 1;
                    std::uint32_t depthClipDisable         : 1;
                    std::uint32_t texture1D                : 1;
                    std::uint32_t independentBlend         : 1;
                    std::uint32_t structuredBuffer         : 1;
                    std::uint32_t subsetTextureView        : 1;
                    std::uint32_t commandListDebugMarkers  : 1;
                    std::uint32_t bufferRangeBinding       : 1;
                    std::uint32_t shaderFloat64            : 1;

                    std::uint32_t reserved : 13;
                };
                std::uint32_t value;
            };            
        };

        struct Options{
            bool debug;
            bool hasMainSwapChain;
            PixelFormat swapChainPixelFormat;
            bool swapChainUseSRGBFormat;
            bool syncToVerticalBlank;
            ResourceBindingModel resourceBindingModel;
            bool preferDepthRangeZeroToOne;
            bool preferStandardClipSpaceYDirection;
        };

        enum class UVOrigin{ TopLeft, TopRight, BottomLeft, BottomRight };

        struct SubmitBatch{
            const std::vector<CommandList*>& cmd;
            const std::vector<Semaphore*>& waitSemaphores;
            const std::vector<Semaphore*>& signalSemaphores;
        };

    protected:
        GraphicsDevice() = default;

    public:
        virtual const AdapterInfo& GetAdapterInfo() const = 0;

        //virtual const std::string& DeviceName() const = 0;
        //virtual const std::string& VendorName() const = 0;
        //virtual const GraphicsApiVersion ApiVersion() const = 0;
        virtual const Features& GetFeatures() const = 0;

        virtual void* GetNativeHandle() const = 0;

        virtual ResourceFactory* GetResourceFactory() = 0;
        virtual void SubmitCommand(
            const std::vector<SubmitBatch>& batch,
            Fence* fence) = 0;
        virtual SwapChain::State PresentToSwapChain(
            const std::vector<Semaphore*>& waitSemaphores,
            SwapChain* sc) = 0;
               
        virtual void WaitForIdle() = 0;

    };
} // namespace Veldrid
