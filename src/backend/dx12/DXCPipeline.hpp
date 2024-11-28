#pragma once
//3rd-party headers

//veldrid public headers
#include "veldrid/common/RefCnt.hpp"

#include "veldrid/Pipeline.hpp"
#include "veldrid/GraphicsDevice.hpp"
#include "veldrid/SyncObjects.hpp"
#include "veldrid/Buffer.hpp"
#include "veldrid/SwapChain.hpp"

//standard library headers
#include <unordered_set>

//backend specific headers

//platform specific headers
#include <directx/d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers

namespace Veldrid
{
    class DXCDevice;

    class DXCPipelineBase : public Pipeline{

    protected:
        std::unordered_set<sp<RefCntBase>> _refCnts;

        DXCDevice* _Dev();

        Microsoft::WRL::ComPtr<ID3D12PipelineState> _pso;


    protected:
        DXCPipelineBase(const sp<GraphicsDevice>& dev) : Pipeline(dev){}

    public:
        virtual ~DXCPipelineBase();

        ID3D12PipelineState* GetHandle() const {return _pso.Get();}

        
        virtual void* GetNativeHandle() const override {return GetHandle();}


    };


    class DXCGraphicsPipeline : public DXCPipelineBase{

        //Array of blend factors, one for each RGBA component.
        //TODO: [Vk] replace after VK_DYNAMIC_STATE_BLEND_CONSTANTS is in place
        float _blendConstants[4];

        //Primitive topology, here for compensation, if we don't have VK1.3, then
        //we don't have vulkan dynamic state for binding primitive topology through
        //command list
        D3D_PRIMITIVE_TOPOLOGY _primTopo;

        DXCGraphicsPipeline(
            const sp<GraphicsDevice>& dev
        ) : DXCPipelineBase(dev){}

    public:
        ~DXCGraphicsPipeline();

        static sp<Pipeline> Make(
            const sp<DXCDevice>& dev,
            const GraphicsPipelineDescription& desc
        );

        bool IsComputePipeline() const override {return true;}

    };

    
    class DXCComputePipeline : public DXCPipelineBase{

        DXCComputePipeline(
            const sp<GraphicsDevice>& dev
        ) : DXCPipelineBase(dev){}

    public:
        ~DXCComputePipeline();

        static sp<Pipeline> Make(
            const sp<DXCDevice>& dev,
            const ComputePipelineDescription& desc
        );

        bool IsComputePipeline() const override {return false;}
    };
}