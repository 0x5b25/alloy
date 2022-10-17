#pragma once

#include <string>
#include <sstream>

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

    protected:
        GraphicsDevice() = default;

    public:

        virtual const std::string& DeviceName() const = 0;
        virtual const std::string& VendorName() const = 0;
        virtual const GraphicsApiVersion ApiVersion() const = 0;
        virtual const Features& GetFeatures() const = 0;

        virtual ResourceFactory* GetResourceFactory() = 0;

    };
} // namespace Veldrid
