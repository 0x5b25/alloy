#pragma once

#include <string>
#include <sstream>
#include <vector>

#include "alloy/common/Macros.h"
#include "alloy/common/RefCnt.hpp"

#include "alloy/Types.hpp"
//#include "alloy/ResourceFactory.hpp"
//#include "alloy/CommandQueue.hpp"
#include "alloy/SwapChain.hpp"

namespace alloy
{
    class IPhysicalAdapter;
    class ResourceFactory;
    class ICommandQueue;
    //class SwapChain;

    

    class IGraphicsDevice : public common::RefCntBase{
        DISABLE_COPY_AND_ASSIGN(IGraphicsDevice);

    public:

        struct F {
            bool a, b, c, d, e;
        };

        struct Features{
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

        //struct SubmitBatch{
        //    const std::vector<CommandList*>& cmd;
        //    const std::vector<Semaphore*>& waitSemaphores;
        //    const std::vector<Semaphore*>& signalSemaphores;
        //};

    protected:
        IGraphicsDevice() = default;

    public:
        //virtual const AdapterInfo& GetAdapterInfo() const = 0;

        //virtual const std::string& DeviceName() const = 0;
        //virtual const std::string& VendorName() const = 0;
        //virtual const GraphicsApiVersion ApiVersion() const = 0;
        virtual const Features& GetFeatures() const = 0;
        virtual IPhysicalAdapter& GetAdapter() const = 0;

        virtual void* GetNativeHandle() const = 0;

        virtual alloy::ResourceFactory& GetResourceFactory() = 0;
        //virtual void SubmitCommand(const CommandList* cmd) = 0;
        virtual ISwapChain::State PresentToSwapChain(
            ISwapChain* sc) = 0;

        virtual ICommandQueue* GetGfxCommandQueue() = 0;
        virtual ICommandQueue* GetCopyCommandQueue() = 0;
               
        virtual void WaitForIdle() = 0;

    };
} // namespace alloy
