#include "DXCExecuteIndirect.hpp"

#include "D3DCommon.hpp"
#include "DXCDevice.hpp"
#include "DXCBindableResource.hpp"

namespace alloy::dxc {


    DXCIndirectCommandLayout::~DXCIndirectCommandLayout() {
        if(_cmdSig) {
            _cmdSig->Release();
        }
    }

    common::sp<IIndirectCommandLayout> DXCIndirectCommandLayout::Make(
        const common::sp<DXCDevice>& dev,
        const IIndirectCommandLayout::Description& desc
    ) {
        auto* d3dDev = dev->GetDevice();
        std::vector<D3D12_INDIRECT_ARGUMENT_DESC> d3dIndArgs;

        common::sp<DXCResourceLayout> dxcLayout;
        if(desc.layout) {
            dxcLayout = common::SPCast<DXCResourceLayout>(desc.layout);
        }

        for(auto& op : desc.operations) {
            switch(op.type) {
                case IIndirectCommandLayout::OpType::Dispatch: {
                    auto& arg = d3dIndArgs.emplace_back();
                    arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
                } break;
                case IIndirectCommandLayout::OpType::Draw: {
                    auto& arg = d3dIndArgs.emplace_back();
                    arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
                } break;
                case IIndirectCommandLayout::OpType::BindVertexBuffer: {
                    auto& arg = d3dIndArgs.emplace_back();
                    arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
                    arg.VertexBuffer.Slot = op.bindVertexBuffer.slot;
                } break;
                case IIndirectCommandLayout::OpType::BindIndexBuffer: {
                    auto& arg = d3dIndArgs.emplace_back();
                    arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
                } break;
                
                case IIndirectCommandLayout::OpType::SetPushConstant: {
                    auto& arg = d3dIndArgs.emplace_back();
                    arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
                    if(!dxcLayout) {
                        throw std::invalid_argument("Changing push constants requires a resource layout");
                    }
                    auto argBase = dxcLayout->GetHeapCount();
                    arg.Constant.RootParameterIndex = op.setPushConstant.pushConstantIndex + argBase;
                    arg.Constant.Num32BitValuesToSet = op.setPushConstant.num32BitValuesToSet;
                    arg.Constant.DestOffsetIn32BitValues = op.setPushConstant.destOffsetIn32BitValues;
                } break;

                default:
                    //Catch unimplemented operations
                    assert(false);
            }
        }

        D3D12_COMMAND_SIGNATURE_DESC d3dDesc {};
        d3dDesc.ByteStride = desc.argumentStrideInBytes;
        d3dDesc.NumArgumentDescs = d3dIndArgs.size();
        d3dDesc.pArgumentDescs = d3dIndArgs.data();

        ID3D12CommandSignature* d3dCmdSig = nullptr;
        ThrowIfFailed( d3dDev->CreateCommandSignature(
            &d3dDesc,
            dxcLayout ? dxcLayout->GetHandle() : nullptr,
            __uuidof(ID3D12CommandSignature),
            (void**)&d3dCmdSig
        ));

        
        auto rawCmdLayout = new DXCIndirectCommandLayout();
        rawCmdLayout->_dev = dev;
        rawCmdLayout->_resLayout = std::move(dxcLayout);
        rawCmdLayout->_cmdSig = d3dCmdSig;
        return common::sp(rawCmdLayout);
    }

}
