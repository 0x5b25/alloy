#pragma once

#include <dxgi.h>
#include <d3d12.h>

#include "veldrid/GraphicsDevice.hpp"


namespace Veldrid{


    class DXCDevice : public  GraphicsDevice {

    public:

        ~DXCDevice();

        static sp<GraphicsDevice> Make(
            const GraphicsDevice::Options& options,
            SwapChainSource* swapChainSource = nullptr
        );

    };

}
