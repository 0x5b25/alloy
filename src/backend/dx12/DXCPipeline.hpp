#pragma once
//3rd-party headers

//alloy public headers
#include "alloy/common/RefCnt.hpp"

#include "alloy/Pipeline.hpp"
#include "alloy/GraphicsDevice.hpp"
#include "alloy/SyncObjects.hpp"
#include "alloy/Buffer.hpp"
#include "alloy/SwapChain.hpp"

//standard library headers
#include <unordered_set>

//backend specific headers

//platform specific headers
#include <d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers

namespace alloy::dxc
{
    class DXCDevice;
    class DXCResourceLayout;

    class DXCPipelineBase {

    protected:
        common::sp<DXCDevice> dev;

        std::unordered_set<common::sp<common::RefCntBase>> _refCnts;

        Microsoft::WRL::ComPtr<ID3D12PipelineState> _pso;
        common::sp<DXCResourceLayout> _rootSig;


    protected:
        DXCPipelineBase(const common::sp<DXCDevice>& dev) : dev(dev) { }

    public:
        virtual ~DXCPipelineBase();

        ID3D12PipelineState* GetHandle() const {return _pso.Get();}
        
        //virtual void* GetNativeHandle() const override {return GetHandle();}

        virtual void CmdBindPipeline(ID3D12GraphicsCommandList* pCmdList) = 0;

        const common::sp<DXCResourceLayout>& GetPipelineLayout() const { _rootSig; }

    };


    class DXCGraphicsPipeline : public IGfxPipeline, public DXCPipelineBase{

        GraphicsPipelineDescription _desc;

        //Array of blend factors, one for each RGBA component.
        //TODO: [Vk] replace after VK_DYNAMIC_STATE_BLEND_CONSTANTS is in place
        float _blendConstants[4];

        //Primitive topology, here for compensation, if we don't have VK1.3, then
        //we don't have vulkan dynamic state for binding primitive topology through
        //command list
        D3D_PRIMITIVE_TOPOLOGY _primTopo;
        DXCGraphicsPipeline(
            const common::sp<DXCDevice>& dev,
            const GraphicsPipelineDescription& desc
        ) 
            : DXCPipelineBase(dev)
            , _desc(desc)
        {}

    public:
        ~DXCGraphicsPipeline();

        static common::sp<IGfxPipeline> Make(
            const common::sp<DXCDevice>& dev,
            const GraphicsPipelineDescription& desc
        );

        void CmdBindPipeline(ID3D12GraphicsCommandList* pCmdList) override;

        const GraphicsPipelineDescription& GetDesc() const {return _desc;}

    };

    
    class DXCComputePipeline : public IComputePipeline, public DXCPipelineBase{

        
        ComputePipelineDescription _desc;

        DXCComputePipeline(
            const common::sp<DXCDevice>& dev,
            const ComputePipelineDescription& desc
        )   
            : DXCPipelineBase(dev)
            , _desc(desc)
        {}

    public:
        ~DXCComputePipeline();

        static common::sp<IComputePipeline> Make(
             const common::sp<DXCDevice>& dev,
            const ComputePipelineDescription& desc
        );
        
        void CmdBindPipeline(ID3D12GraphicsCommandList* pCmdList) override;
    };
}