#include "DXCCommandList.hpp"


#include "alloy/common/Common.hpp"
#include "alloy/Helpers.hpp"

#include "DXCDevice.hpp"
#include "D3DTypeCvt.hpp"
#include "D3DCommon.hpp"
#include "DXCTexture.hpp"
#include "DXCPipeline.hpp"
#include "DXCBindableResource.hpp"
#include "d3d12.h"

#include "D3DPixEventEncoder.hpp"

//#define USE_PIX
//#include <WinPixEventRuntime/pix3.h>

#include <stdexcept>

namespace alloy::dxc
{



    common::sp<DXCCommandList> DXCCommandList::Make( const common::sp<DXCDevice>& dev,
                                                     D3D12_COMMAND_LIST_TYPE type
    ) {

        auto pDev = dev->GetDevice();

        //D3D12_COMMAND_LIST_TYPE cmdListType = D3D12_COMMAND_LIST_TYPE_DIRECT;

        ID3D12CommandAllocator* pAllocator;
        ThrowIfFailed(pDev->CreateCommandAllocator(type, IID_PPV_ARGS(&pAllocator)));


        ID3D12GraphicsCommandList* pCmdList;
        // Create the command list.
        ThrowIfFailed(pDev->CreateCommandList(0, type, pAllocator, nullptr, IID_PPV_ARGS(&pCmdList)));
        ThrowIfFailed(pCmdList->Close());



        if(dev->GetDevCaps().SupportEnhancedBarrier()) {
            ID3D12GraphicsCommandList7* pNewCmdList;
            pCmdList->QueryInterface(IID_PPV_ARGS(&pNewCmdList));
            pCmdList->Release();
            return common::sp(new DXCCommandList7(dev, pAllocator, pNewCmdList));
        }
        else if(dev->GetDevCaps().SupportMeshShader()) {
            ID3D12GraphicsCommandList6* pNewCmdList;
            pCmdList->QueryInterface(IID_PPV_ARGS(&pNewCmdList));
            pCmdList->Release();
            return common::sp(new DXCCommandList6(dev, pAllocator, pNewCmdList));
        } else {
            //First supported in Win10 Creators update
            //Can safely assume widespread support
            ID3D12GraphicsCommandList1* pNewCmdList;
            pCmdList->QueryInterface(IID_PPV_ARGS(&pNewCmdList));
            pCmdList->Release();
            return common::sp(new DXCCommandList(dev, pAllocator, pNewCmdList));
        }
    }

    #define CHK_RENDERPASS_BEGUN() DEBUGCODE(assert(_currentPass != nullptr))
    //#define CHK_RENDERPASS_ENDED() DEBUGCODE(assert(_currentPass == nullptr))
    #define CHK_GFX_PIPELINE_SET()                 \
        DEBUGCODE(                                 \
            assert(                                \
                currentPipeline != nullptr         \
                && IsGfxPipeline(*currentPipeline) \
            )                                      \
        )


    #define CHK_COMPUTE_PIPELINE_SET()             \
        DEBUGCODE(                                 \
            assert(                                \
                currentPipeline != nullptr         \
                && IsComputePipeline(*currentPipeline) \
            )                                      \
        )


    #define CHK_MESH_PIPELINE_SET()                \
        DEBUGCODE(                                 \
            assert(                                \
                currentPipeline != nullptr         \
                && IsMeshShaderPipeline(*currentPipeline) \
            )                                      \
        )

#pragma region CmdEncBase

    ID3D12GraphicsCommandList1* DXCCmdEncBase::GetCmdList() const {
        return static_cast<ID3D12GraphicsCommandList1*>(cmdList->GetHandle());
    }

    //Sadly this can't be made constexpr due to std::floor can't be constexpr
    static uint32_t Color4fToPixColor(const Color4f& c4f) {

        uint32_t r = std::floor(c4f.r * 255);
        uint32_t g = std::floor(c4f.g * 255);
        uint32_t b = std::floor(c4f.b * 255);
        return 0xff000000u | (r << 16) | (g << 8) | b;
    }

    void DXCCmdEncBase::PushDebugGroup(const std::string& name, uint32_t color) {
        EncodePIXBeginEvent(GetCmdList(), color, name);
    }

    void DXCCmdEncBase::PopDebugGroup() {
        EncodePIXEndEvent(GetCmdList());
    }

    void DXCCmdEncBase::InsertDebugMarker(const std::string& name, uint32_t color) {
        EncodePIXMarker(GetCmdList(), color, name);
    }

#pragma endregion


#pragma region RenderCmdEnc

    DXCRenderCmdEnc::DXCRenderCmdEnc(
        DXCDevice *dev,
        DXCCommandList *cmdList,
        const RenderPassAction &act
    )
        : DXCCmdEncBase{dev, cmdList}
        , _fb(act)
    {

        {
            auto colorAttachmentCnt = _fb.colorTargetActions.size();

            std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHeapPtrs;
            rtvHeapPtrs.reserve(colorAttachmentCnt);
            _rtvHandles.reserve(colorAttachmentCnt);
            for(auto& action : _fb.colorTargetActions) {
                auto dxcView = common::PtrCast<DXCTextureView>(action.target.get());
                const auto& dxcViewDesc = dxcView->GetDesc();
                const auto& dxcTexDesc = dxcView->GetTextureObject()->GetDesc();
                auto rtv = dev->AllocateRTV();

                bool isMultisampled = dxcTexDesc.sampleCount != alloy::SampleCount::x1;
                
                D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {
                    .Format = VdToD3DPixelFormat(dxcTexDesc.format),
                    .ViewDimension = isMultisampled?
                        D3D12_RTV_DIMENSION_TEXTURE2DMS :
                        D3D12_RTV_DIMENSION_TEXTURE2D,
                };

                if(!isMultisampled) {
                    rtvDesc.Texture2D = {
                        .MipSlice = dxcViewDesc.baseMipLevel,
                        .PlaneSlice = 0,
                    };
                }

                dev->GetDevice()->CreateRenderTargetView(
                    dxcView->GetTextureHandle(),
                    &rtvDesc, 
                    rtv.handle);
                
                _rtvHandles.push_back(rtv);
                rtvHeapPtrs.push_back(rtv.handle);
            }

#ifdef VLD_DEBUG
            //DX12 must use combined depth stencil target
            if(_fb.depthTargetAction.has_value() && _fb.stencilTargetAction.has_value()) {
                auto dtView = common::PtrCast<DXCTextureView>(_fb.depthTargetAction.value().target.get());
                auto stView = common::PtrCast<DXCTextureView>(_fb.stencilTargetAction.value().target.get());

                const auto& dtViewDesc = dtView->GetDesc();
                const auto& stViewDesc = stView->GetDesc();

                assert(dtViewDesc.baseMipLevel == stViewDesc.baseMipLevel && 
                       dtViewDesc.baseArrayLayer == stViewDesc.baseArrayLayer );

                assert(dtView->GetTextureHandle() == stView->GetTextureHandle());
            }
#endif
            _dsvHandle = { };
            DXCTextureView* dxcDSView = nullptr;
            D3D12_DSV_FLAGS dsvFlags { };

            if(_fb.depthTargetAction.has_value()) {
                const auto& dtAct = *_fb.depthTargetAction;
                dxcDSView
                    = common::PtrCast<DXCTextureView>(dtAct.target.get());
                if(dtAct.loadAction == LoadAction::ReadOnly) {
                    dsvFlags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;
                }
            }

            if(_fb.stencilTargetAction.has_value()) {
                const auto& stAct = *_fb.stencilTargetAction;
                dxcDSView
                    = common::PtrCast<DXCTextureView>(stAct.target.get());
                if(stAct.loadAction == LoadAction::ReadOnly) {
                    dsvFlags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
                }
            }

            if(dxcDSView) {
                const auto& dxcViewDesc = dxcDSView->GetDesc();
                const auto& dxcTexDesc = dxcDSView->GetTextureObject()->GetDesc();
                _dsvHandle = dev->AllocateDSV();

                bool isMultisampled = dxcTexDesc.sampleCount != alloy::SampleCount::x1;

                D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc {
                    .Format = VdToD3DPixelFormat(dxcTexDesc.format),
                    .ViewDimension = isMultisampled?
                        D3D12_DSV_DIMENSION_TEXTURE2DMS :
                        D3D12_DSV_DIMENSION_TEXTURE2D,
                    .Flags = dsvFlags,
                }; 

                if(!isMultisampled) {
                    dsvDesc.Texture2D = {
                        .MipSlice = dxcViewDesc.baseMipLevel,
                    };
                }

                dev->GetDevice()->CreateDepthStencilView(
                    dxcDSView->GetTextureHandle(),
                    &dsvDesc, 
                    _dsvHandle.handle);

            }

            GetCmdList()->OMSetRenderTargets(rtvHeapPtrs.size(), rtvHeapPtrs.data(), FALSE,
                _dsvHandle?&_dsvHandle.handle : nullptr);

            //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);

            for(uint32_t i = 0; i < _fb.colorTargetActions.size(); ++i) {

                const auto& action = _fb.colorTargetActions[i];
                const auto& rtv = _rtvHandles[i];

                if(action.loadAction == alloy::LoadAction::Clear){

                    float clearColor[4] = {action.clearColor.r,
                                           action.clearColor.g,
                                           action.clearColor.b,
                                           action.clearColor.a};

                    GetCmdList()->ClearRenderTargetView(rtv.handle, clearColor, 0, nullptr);
                }
            }


            D3D12_CLEAR_FLAGS clearFlags = (D3D12_CLEAR_FLAGS)0;
            float depthClear = 0;
            uint8_t stencilClear = 0;

            if(_fb.depthTargetAction.has_value()) {

                if(_fb.depthTargetAction->loadAction == alloy::LoadAction::Clear) {
                    clearFlags |= D3D12_CLEAR_FLAG_DEPTH;
                    depthClear = _fb.depthTargetAction->clearDepth;
                }
            }

            if(_fb.stencilTargetAction.has_value()) {
                if(_fb.stencilTargetAction->loadAction == alloy::LoadAction::Clear) {
                    clearFlags |= D3D12_CLEAR_FLAG_STENCIL;
                    stencilClear = _fb.stencilTargetAction->clearStencil;
                }
            }


            if(clearFlags != 0) {
                GetCmdList()->ClearDepthStencilView(
                    _dsvHandle.handle,
                    clearFlags,
                    depthClear, stencilClear,
                    0, nullptr
                );
            }
        }
    }

    
    DXCRenderCmdEnc::~DXCRenderCmdEnc() {
        for(const auto& rtv: _rtvHandles) {
            dev->FreeRTV(rtv);
        }

        if(_dsvHandle) {
            dev->FreeDSV(_dsvHandle);
        }
    }


    //DXCResourceLayout* DXCRenderCmdEnc::GetResourceLayoutForCurrentPipeline() {
    //    CHK_GFX_PIPELINE_SET();
    //    return GetPipeline()->GetPipelineLayout().get();
    //}

    void DXCRenderCmdEnc::EndPass() {
        DXCCmdEncBase::EndPass();
        std::vector<const DXCTextureView*> colorResolveSrcViews, colorResolveDstViews;
        
        bool isDSReadOnly;
        DXCTextureView* dsResolveSrcView = nullptr,
                      * dsResolveDstView = nullptr;

        for(auto& ctAct : _fb.colorTargetActions) {
            if(ctAct.msaaResolveTarget) {
                auto texView = PtrCast<DXCTextureView>(ctAct.target.get());
                auto resolveTexView = PtrCast<DXCTextureView>(ctAct.msaaResolveTarget.get());

                colorResolveSrcViews.emplace_back(texView);
                colorResolveDstViews.emplace_back(resolveTexView);

            }
        }

        if (_fb.depthTargetAction.has_value())
        {
            auto& dsAct = _fb.depthTargetAction.value();
            if(dsAct.msaaResolveTarget &&
               dsAct.msaaResolveMode != MSAADepthResolveMode::None) {
                
                auto texView = PtrCast<DXCTextureView>(dsAct.target.get());
                auto resolveTexView = PtrCast<DXCTextureView>(dsAct.msaaResolveTarget.get());

                isDSReadOnly = dsAct.loadAction == LoadAction::ReadOnly;

                dsResolveSrcView = texView;
                dsResolveDstView = resolveTexView;

            }
        }
        
        cmdList->_TransitionColorTargetsToMSAAResolveLayout(colorResolveSrcViews, colorResolveDstViews);
        if(dsResolveSrcView)
            cmdList->_TransitionDSTargetsToMSAAResolveLayout(dsResolveSrcView, dsResolveDstView, isDSReadOnly);

        for(auto& ctAct : _fb.colorTargetActions) {
            if(ctAct.msaaResolveTarget) {
                auto texView = PtrCast<DXCTextureView>(ctAct.target.get());

                auto resolveTexView = PtrCast<DXCTextureView>(ctAct.msaaResolveTarget.get());
                auto resolveTex = PtrCast<DXCTexture>(resolveTexView->GetTextureObject().get());

                GetCmdList()->ResolveSubresource(
                    resolveTexView->GetTextureHandle(),
                    resolveTexView->ComputeSubresource(0,0),
                    texView->GetTextureHandle(),
                    texView->ComputeSubresource(0,0),
                    resolveTex->GetHandle()->GetDesc().Format);
            }
        }

        if (_fb.depthTargetAction.has_value())
        {
            auto& dsAct = _fb.depthTargetAction.value();
            if(dsAct.msaaResolveTarget) {
                
                auto texView = PtrCast<DXCTextureView>(dsAct.target.get());

                auto resolveTexView = PtrCast<DXCTextureView>(dsAct.msaaResolveTarget.get());
                auto resolveTex = PtrCast<DXCTexture>(resolveTexView->GetTextureObject().get());

                auto resolveMode = _fb.depthTargetAction->msaaResolveMode;

                if(resolveMode != MSAADepthResolveMode::None) {

                    D3D12_RESOLVE_MODE dxcResovleMode = resolveMode == MSAADepthResolveMode::Min ?
                        D3D12_RESOLVE_MODE_MIN :
                        D3D12_RESOLVE_MODE_MAX ;

                    GetCmdList()->ResolveSubresourceRegion(
                        resolveTexView->GetTextureHandle(),
                        resolveTexView->ComputeSubresource(0,0),
                        0,
                        0,
                        texView->GetTextureHandle(),
                        texView->ComputeSubresource(0,0),
                        nullptr,
                        resolveTex->GetHandle()->GetDesc().Format,
                        dxcResovleMode
                    );
                }
            }
        }

        cmdList->_TransitionColorTargetsFromMSAAResolveLayout(colorResolveSrcViews, colorResolveDstViews);
        if(dsResolveSrcView)
            cmdList->_TransitionDSTargetsFromMSAAResolveLayout(dsResolveSrcView, dsResolveDstView, isDSReadOnly);


        //Handle discard loads

        for(auto& ctAct : _fb.colorTargetActions) {
            if(ctAct.loadAction != LoadAction::ReadOnly &&
               ctAct.storeAction == StoreAction::DontCare
            ) {
                auto texView = PtrCast<DXCTextureView>(ctAct.target.get());
                auto dxcSource = PtrCast<DXCTexture>(texView->GetTextureObject().get());

                D3D12_DISCARD_REGION descardRegion {
                    .NumRects = 0,
                    .pRects = nullptr,
                    .FirstSubresource = texView->ComputeSubresource(0,0),
                    .NumSubresources = 1
                };

                GetCmdList()->DiscardResource(
                    dxcSource->GetHandle(),
                    &descardRegion
                );
            }
        }

        // DX12 only supports combined depth stencil. We only need to handle
        // depth here.
        DXCTextureView* dsView = nullptr;
        bool discardDepthStencil = false;
        if (_fb.depthTargetAction.has_value()) {
            auto& dsAct = _fb.depthTargetAction.value();
            if(  dsAct.loadAction != LoadAction::ReadOnly &&
                 dsAct.storeAction == StoreAction::DontCare
            ) { 
                discardDepthStencil = true;
            }

            dsView = PtrCast<DXCTextureView>(dsAct.target.get());
        }

        if (_fb.stencilTargetAction.has_value()) {
            auto& dsAct = _fb.stencilTargetAction.value();
            bool discardStencil = dsAct.loadAction != LoadAction::ReadOnly &&
                                  dsAct.storeAction == StoreAction::DontCare;
            // Only discard if both depth and stencil discards
            if(dsView) {
                if(discardDepthStencil != discardStencil) {
                    discardDepthStencil = false;
                }
            } else {
                discardDepthStencil = discardStencil;
                dsView = PtrCast<DXCTextureView>(dsAct.target.get());
            }
        }

        if (discardDepthStencil)
        {
            auto dxcSource = PtrCast<DXCTexture>(dsView->GetTextureObject().get());

            D3D12_DISCARD_REGION descardRegion {
                .NumRects = 0,
                .pRects = nullptr,
                .FirstSubresource = dsView->ComputeSubresource(0,0),
                .NumSubresources = 1
            };

            GetCmdList()->DiscardResource(
                dxcSource->GetHandle(),
                &descardRegion
            );
        }

    }

    void DXCRenderCmdEnc::SetPipeline(const common::sp<IGfxPipeline>& pipeline){

        auto dxcPipeline = PtrCast<DXCGraphicsPipeline>(pipeline.get());
        assert((DXCPipelineBase*)dxcPipeline != currentPipeline);
        resources.insert(pipeline);

        dxcPipeline->CmdBindPipeline(GetCmdList());

        //ensure resource set counts
        //auto setCnt = vkPipeline->GetResourceSetCount();
        //if (setCnt > _resourceSets.size()) {
        //    _resourceSets.resize(setCnt, {});
        //}

        //Mark current pipeline
        currentPipeline = dxcPipeline;
    }

    void DXCRenderCmdEnc::SetVertexBuffer(
        std::uint32_t index, const common::sp<BufferRange>& buffer
    ){
        //_resReg.RegisterBufferUsage(buffer,
        //    VkPipelineStageFlagBits::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        //    VkAccessFlagBits::VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
        //);
        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);

        CHK_GFX_PIPELINE_SET();

        resources.insert(buffer);

        auto gfxPipeline = GetPipeline();
        auto& pipeDesc = gfxPipeline->GetDesc();

        auto dxcBuffer = PtrCast<DXCBuffer>(buffer->GetBufferObject().get());

        auto pRes = dxcBuffer->GetHandle();
        D3D12_VERTEX_BUFFER_VIEW view{};
        view.BufferLocation = pRes->GetGPUVirtualAddress() + buffer->GetShape().GetOffsetInBytes();
        view.StrideInBytes = pipeDesc.shaderSet.vertexLayouts[index].stride;
        view.SizeInBytes = buffer->GetShape().GetSizeInBytes();

        GetCmdList()->IASetVertexBuffers(index, 1, &view);
    }

    void DXCRenderCmdEnc::SetIndexBuffer(
        const common::sp<BufferRange>& buffer, IndexFormat format
    ){
        //_resReg.RegisterBufferUsage(buffer,
        //    VkPipelineStageFlagBits::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        //    VkAccessFlagBits::VK_ACCESS_INDEX_READ_BIT
        //);
        //_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        //VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(buffer.get());
        //vkCmdBindIndexBuffer(_cmdBuf, vkBuffer->GetHandle(), offset, alloy::VK::priv::VdToVkIndexFormat(format));

        resources.insert(buffer);

        auto dxcBuffer = PtrCast<DXCBuffer>(buffer->GetBufferObject().get());

        auto pRes = dxcBuffer->GetHandle();
        D3D12_INDEX_BUFFER_VIEW view{};
        view.Format = VdToD3DIndexFormat(format);
        view.BufferLocation = pRes->GetGPUVirtualAddress() + buffer->GetShape().GetOffsetInBytes();
        view.SizeInBytes = buffer->GetShape().GetSizeInBytes();

        GetCmdList()->IASetIndexBuffer(&view);
    }


    void DXCRenderCmdEnc::SetGraphicsResourceSet(const common::sp<IResourceSet>& rs){
//#ifdef VLD_DEBUG
//        GetResourceLayoutForCurrentPipeline();
//#endif
        resources.insert(rs);

        auto d3dkrs = PtrCast<DXCResourceSet>(rs.get());

        auto& heaps = d3dkrs->GetHeaps();
        GetCmdList()->SetDescriptorHeaps(heaps.size(), heaps.data());

        for(uint32_t i = 0; i < heaps.size(); i++) {
            GetCmdList()->SetGraphicsRootDescriptorTable(i, heaps[i]->GetGPUDescriptorHandleForHeapStart());
        }

        //auto& entry = _resourceSets[slot];
        //entry.isNewlyChanged = true;
        //entry.resSet = RefRawPtr(vkrs);
        //entry.offsets = dynamicOffsets;
    }

    void DXCRenderCmdEnc::SetGraphicsMutableResourceSet(
        const common::sp<IMutableResourceSet>& rs
    ) {
        resources.insert(rs);

        auto d3dkrs = PtrCast<DXCMutableResourceSet>(rs.get());

        auto& heaps = d3dkrs->GetHeaps();
        GetCmdList()->SetDescriptorHeaps(heaps.size(), heaps.data());

        for(uint32_t i = 0; i < heaps.size(); i++) {
            GetCmdList()->SetGraphicsRootDescriptorTable(i, heaps[i]->GetGPUDescriptorHandleForHeapStart());
        }
    }


    void DXCRenderCmdEnc::SetPushConstants(
        std::uint32_t pushConstantIndex,
        std::span<const uint32_t> data,
        std::uint32_t destOffsetIn32BitValues
    ) {
        //assert(slot < _resourceSets.size());

        auto dxcLayout = currentPipeline->GetPipelineLayout();

        auto argBase = dxcLayout->GetHeapCount();

        std::vector<uint32_t> dataCopy(data.begin(), data.end());

        GetCmdList()->SetGraphicsRoot32BitConstants(
            pushConstantIndex + argBase,
            dataCopy.size(),
            dataCopy.data(),
            destOffsetIn32BitValues
        );
    }


    void DXCRenderCmdEnc::SetViewports(std::span<const Viewport> viewport){
        //dx12 requires all viewports set as an atomic operation.
        std::vector<Viewport> savedData {viewport.begin(), viewport.end()};

        {
            const auto& viewport = savedData;
            if (viewport.size() == 1 || dev->GetFeatures().multipleViewports)
            {
                //bool flip = gd->SupportsFlippedYDirection();
                //float vpY = flip
                //    ? viewport.height + viewport.y
                //    : viewport.y;
                //float vpHeight = flip
                //    ? -viewport.height
                //    : viewport.height;
                std::vector<D3D12_VIEWPORT> vps(viewport.size());
                for(uint32_t i = 0; i < viewport.size(); i++) {
                    vps[i].TopLeftX = viewport[i].x;
                    vps[i].TopLeftY = viewport[i].y;
                    vps[i].Width    = viewport[i].width;
                    vps[i].Height   = viewport[i].height;
                    vps[i].MinDepth = viewport[i].minDepth;
                    vps[i].MaxDepth = viewport[i].maxDepth;
                }
                GetCmdList()->RSSetViewports(vps.size(), vps.data());
            }
        }
    }

    void DXCRenderCmdEnc::SetFullViewport() {
        D3D12_VIEWPORT vp;
        bool vpIsSet = false;

        auto _SetVP = [&](const ITexture::Description& texDesc){
            vp = {
                .TopLeftX = 0,
                .TopLeftY = 0,
                .Width    = (float)texDesc.width,
                .Height   = (float)texDesc.height,
                .MinDepth = 0,
                .MaxDepth = 1,
            };

            vpIsSet = true;
        };

        for(auto& rt : _fb.colorTargetActions) {
            auto& texDesc = rt.target->GetTextureObject()->GetDesc();
            _SetVP(texDesc);
            break;
        }

        if(!vpIsSet) {
            if(_fb.depthTargetAction) {
                auto& dt = _fb.depthTargetAction.value();
                auto& texDesc = dt.target->GetTextureObject()->GetDesc();
                _SetVP(texDesc);
                vpIsSet = true;
            }
        }

        if(!vpIsSet) {
            if(_fb.stencilTargetAction) {
                auto& st = _fb.stencilTargetAction.value();
                auto& texDesc = st.target->GetTextureObject()->GetDesc();
                _SetVP(texDesc);
                vpIsSet = true;
            }
        }
        assert(vpIsSet && "Pass with no render / depth / stencil targets!");
        GetCmdList()->RSSetViewports(1, &vp);
    }

    void DXCRenderCmdEnc::SetScissorRects(std::span<const Rect> rects) {
        std::vector<Rect> savedData {rects.begin(), rects.end()};
        {
            const auto& rects = savedData;
            //dx12 requires all scissor rects set as an atomic operation.
            if (rects.size() == 1 || dev->GetFeatures().multipleViewports) {
                std::vector<D3D12_RECT> srs(rects.size());
                for(uint32_t i = 0; i < rects.size(); i++) {
                    srs[i].left = rects[i].x;
                    srs[i].top = rects[i].y;
                    srs[i].right = rects[i].x + rects[i].width;
                    srs[i].bottom = rects[i].y + rects[i].height;
                }
                //if (_scissorRects[index] != scissor)
                //{
                //    _scissorRects[index] = scissor;
                GetCmdList()->RSSetScissorRects(srs.size(), srs.data());
            }
        }
    }

    void DXCRenderCmdEnc::SetFullScissorRect(){

        D3D12_RECT sr;
        bool srIsSet = false;

        auto _SetSR = [&](const ITexture::Description& texDesc){
            sr = {
                .left = 0,
                .top = 0,
                .right    =  (LONG)texDesc.width,
                .bottom   =  (LONG)texDesc.height,
            };

            srIsSet = true;
        };

        for(auto& rt : _fb.colorTargetActions) {
            auto& texDesc = rt.target->GetTextureObject()->GetDesc();
            _SetSR(texDesc);
            break;
        }

        if(!srIsSet) {
            if(_fb.depthTargetAction) {
                auto& dt = _fb.depthTargetAction.value();
                auto& texDesc = dt.target->GetTextureObject()->GetDesc();
                _SetSR(texDesc);
            }
        }

        if(!srIsSet) {
            if(_fb.stencilTargetAction) {
                auto& st = _fb.stencilTargetAction.value();
                auto& texDesc = st.target->GetTextureObject()->GetDesc();
                _SetSR(texDesc);
            }
        }

        assert(srIsSet && "Pass with no render / depth / stencil targets!");
        GetCmdList()->RSSetScissorRects(1, &sr);
    }

    void DXCRenderCmdEnc::Draw(
        std::uint32_t vertexCount, std::uint32_t instanceCount,
        std::uint32_t vertexStart, std::uint32_t instanceStart
    ){
        CHK_GFX_PIPELINE_SET();
        //PreDrawCommand();
        GetCmdList()->DrawInstanced(vertexCount, instanceCount, vertexStart, instanceStart);
    }

    void DXCRenderCmdEnc::DrawIndexed(
        std::uint32_t indexCount, std::uint32_t instanceCount,
        std::uint32_t indexStart, std::uint32_t vertexOffset,
        std::uint32_t instanceStart
    ){
        CHK_GFX_PIPELINE_SET();
        //PreDrawCommand();
        //vkCmdDrawIndexed(_cmdBuf, indexCount, instanceCount, indexStart, vertexOffset, instanceStart);
        GetCmdList()->DrawIndexedInstanced(indexCount, instanceCount, indexStart, vertexOffset, instanceStart);
    }

#pragma endregion RenderCmdEnc

#if 0
    void DXCRenderCmdEnc::DrawIndirect(
        const sp<Buffer>& indirectBuffer,
        std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride
    ){
        ///#TODO: revisit vulkan drawindirect, parameters seem incomplete
        //_resReg.RegisterBufferUsage(indirectBuffer,
        //    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        //    VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        //    );
        ////_resReg.InsertPipelineBarrierIfNecessary(_cmdBuf);
        //PreDrawCommand();
        //auto dxcBuffer = PtrCast<DXCBuffer>(indirectBuffer.get());
        //_cmdList->ExecuteIndirect()
        //vkCmdDrawIndirect(_cmdBuf, vkBuffer->GetHandle(), offset, drawCount, stride);
    }

    void DXCRenderCmdEnc::DrawIndexedIndirect(
        const sp<Buffer>& indirectBuffer,
        std::uint32_t offset, std::uint32_t drawCount, std::uint32_t stride
    ){

        ///#TODO: revisit vulkan drawindexedindirect, parameters seem incomplete
        //_resReg.RegisterBufferUsage(indirectBuffer,
        //    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        //    VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        //);
        //PreDrawCommand();
        //
        //VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(indirectBuffer.get());
        //vkCmdDrawIndexedIndirect(_cmdBuf, vkBuffer->GetHandle(), offset, drawCount, stride);
    }
#endif

#pragma region RenderCmdEnc6

    DXCRenderCmdEnc6::DXCRenderCmdEnc6(
        DXCDevice *dev,
        DXCCommandList6 *cmdList,
        const RenderPassAction &act
    )
        : DXCRenderCmdEnc(dev, cmdList, act)
    {

    }


    DXCRenderCmdEnc6::~DXCRenderCmdEnc6() {

    }

   //DXCResourceLayout* DXCRenderCmdEnc6::GetResourceLayoutForCurrentPipeline() {
   //    assert(currentPipeline != nullptr);
   //    if(IsMeshShaderPipeline(*currentPipeline)) {
   //        return static_cast<DXCMeshShaderPipeline*>(currentPipeline)->GetPipelineLayout().get();
   //    }
   //
   //    return DXCRenderCmdEnc::GetResourceLayoutForCurrentPipeline();
   //}

    void DXCRenderCmdEnc6::SetPipeline(const common::sp<IMeshShaderPipeline>& pipeline) {

        assert(dev->GetDevCaps().SupportMeshShader());

        auto dxcPipeline = PtrCast<DXCMeshShaderPipeline>(pipeline.get());
        assert((DXCPipelineBase*)dxcPipeline != currentPipeline);
        resources.insert(pipeline);

        dxcPipeline->CmdBindPipeline(GetCmdList());

        //ensure resource set counts
        //auto setCnt = vkPipeline->GetResourceSetCount();
        //if (setCnt > _resourceSets.size()) {
        //    _resourceSets.resize(setCnt, {});
        //}

        //Mark current pipeline
        currentPipeline = dxcPipeline;
    }

    void DXCRenderCmdEnc6::DispatchMesh( std::uint32_t groupCountX,
                                         std::uint32_t groupCountY,
                                         std::uint32_t groupCountZ)
    {
        assert(dev->GetDevCaps().SupportMeshShader());
        CHK_MESH_PIPELINE_SET();

        auto cmdList6 = static_cast<DXCCommandList6*>(cmdList);
        cmdList6->GetCmdList()->DispatchMesh(groupCountX, groupCountY, groupCountZ);

    }
#pragma endregion

#pragma region ComputeCmdEnc

    //DXCResourceLayout* DXCComputeCmdEnc::GetResourceLayoutForCurrentPipeline() {
    //    CHK_COMPUTE_PIPELINE_SET();
    //    return GetPipeline()->GetPipelineLayout().get();
    //}

    void DXCComputeCmdEnc::SetPipeline(const common::sp<IComputePipeline>& pipeline){

        auto dxcPipeline = PtrCast<DXCComputePipeline>(pipeline.get());
        assert((DXCPipelineBase*)dxcPipeline != currentPipeline);
        resources.insert(pipeline);

        dxcPipeline->CmdBindPipeline(GetCmdList());

        //ensure resource set counts
        //auto setCnt = vkPipeline->GetResourceSetCount();
        //if (setCnt > _resourceSets.size()) {
        //    _resourceSets.resize(setCnt, {});
        //}

        //Mark current pipeline
        currentPipeline = dxcPipeline;
    }

    void DXCComputeCmdEnc::SetComputeResourceSet(const common::sp<IResourceSet>& rs){
        //assert(slot < _resourceSets.size());
//#ifdef VLD_DEBUG
//        GetResourceLayoutForCurrentPipeline();
//#endif
        resources.insert(rs);

        auto d3dkrs = PtrCast<DXCResourceSet>(rs.get());

        auto& heaps = d3dkrs->GetHeaps();
        GetCmdList()->SetDescriptorHeaps(heaps.size(), heaps.data());

        for(uint32_t i = 0; i < heaps.size(); i++) {
            GetCmdList()->SetComputeRootDescriptorTable(i, heaps[i]->GetGPUDescriptorHandleForHeapStart());
        }
    }

    void DXCComputeCmdEnc::SetComputeMutableResourceSet(
        const common::sp<IMutableResourceSet>& rs
    ) {
        resources.insert(rs);

        auto d3dkrs = PtrCast<DXCMutableResourceSet>(rs.get());

        auto& heaps = d3dkrs->GetHeaps();
        GetCmdList()->SetDescriptorHeaps(heaps.size(), heaps.data());

        for(uint32_t i = 0; i < heaps.size(); i++) {
            GetCmdList()->SetComputeRootDescriptorTable(i, heaps[i]->GetGPUDescriptorHandleForHeapStart());
        }
    }


    void DXCComputeCmdEnc::SetPushConstants(
        std::uint32_t pushConstantIndex,
        std::span<const uint32_t> data,
        std::uint32_t destOffsetIn32BitValues
    ) {
        //assert(slot < _resourceSets.size());

        auto dxcLayout = currentPipeline->GetPipelineLayout();

        auto argBase = dxcLayout->GetHeapCount();

        std::vector<uint32_t> dataCopy(data.begin(), data.end());

        GetCmdList()->SetComputeRoot32BitConstants(
            pushConstantIndex + argBase,
            dataCopy.size(),
            dataCopy.data(),
            destOffsetIn32BitValues
        );
    }

    void DXCComputeCmdEnc::Dispatch(
        std::uint32_t groupCountX, std::uint32_t groupCountY, std::uint32_t groupCountZ
    ){
        CHK_COMPUTE_PIPELINE_SET();
        //PreDispatchCommand();
        //vkCmdDispatch(_cmdBuf, groupCountX, groupCountY, groupCountZ);
        GetCmdList()->Dispatch(groupCountX, groupCountY, groupCountZ);
    };

#if 0
    void DXCCommandList::DispatchIndirect(const sp<Buffer>& indirectBuffer, std::uint32_t offset) {
        ///#TODO: revisit vulkan DispatchIndirect, parameters seem incomplete
        //The name of the stage flag is slightly confusing, but the spec is
        //otherwisely very clear it aplies to compute :
        //
        //    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT specifies the stage of the
        //    pipeline where VkDrawIndirect* / VkDispatchIndirect * / VkTraceRaysIndirect *
        //    data structures are consumed.
        //
        //and also :
        //
        //    For the compute pipeline, the following stages occur in this order :
        //
        //    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
        //    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
        //

        //_resReg.RegisterBufferUsage(indirectBuffer,
        //    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        //    VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        //);
        //
        //PreDispatchCommand();
        //
        //VulkanBuffer* vkBuffer = PtrCast<VulkanBuffer>(indirectBuffer.get());
        //vkCmdDispatchIndirect(_cmdBuf, vkBuffer->GetHandle(), offset);
    };
#endif


#pragma endregion ComputeCmdEnc


#pragma region XferCmdEnc

#if 0
    static uint32_t _ComputeSubresource(uint32_t mipLevel, uint32_t mipLevelCount, uint32_t arrayLayer)
    {
        return ((arrayLayer * mipLevelCount) + mipLevel);
    };

    void DXCTransferCmdEnc::ResolveTexture(const common::sp<ITexture>& source,
                                           const common::sp<ITexture>& destination) {

        auto* dxcSource = PtrCast<DXCTexture>(source.get());
        resources.insert(source);
        RegisterTexUsage(dxcSource, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);

        auto* dxcDestination = PtrCast<DXCTexture>(destination.get());
        resources.insert(destination);
        RegisterTexUsage(dxcDestination, D3D12_RESOURCE_STATE_RESOLVE_DEST);

        //TODO:Implement full image layout tracking and transition systems
        //vkSource.TransitionImageLayout(_cmdBuf, 0, 1, 0, 1, VkImageLayout.TransferSrcOptimal);
        //vkDestination.TransitionImageLayout(_cmdBuf, 0, 1, 0, 1, VkImageLayout.TransferDstOptimal);

        GetCmdList()->ResolveSubresource(
            dxcDestination->GetHandle(),
            0,
            dxcSource->GetHandle(),
            0,
            dxcDestination->GetHandle()->GetDesc().Format);
    }
#endif

    void DXCTransferCmdEnc::CopyBufferToTexture(
        const common::sp<BufferRange>& src,
        std::uint32_t srcBytesPerRow,
        std::uint32_t srcBytesPerImage,
        const common::sp<ITextureView>& dst,
        const Point3D& dstOrigin,
        std::uint32_t dstMipLevel,
        std::uint32_t dstBaseArrayLayer,
        const Size3D& copySize
    ){

        auto* srcDxcBuffer = PtrCast<DXCBuffer>(src->GetBufferObject().get());
        
        auto dstDxcView = PtrCast<DXCTextureView>(dst.get());
        auto dstDxcTexture = PtrCast<DXCTexture>(dst->GetTextureObject().get());
        const auto& dstViewDesc = dstDxcView->GetDesc();

        resources.insert(src);
        resources.insert(dst);

        auto& dstDesc = dstDxcTexture->GetDesc();

        D3D12_TEXTURE_COPY_LOCATION dstSubresource{};
        dstSubresource.pResource = dstDxcTexture->GetHandle();
        dstSubresource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstSubresource.SubresourceIndex = dstDxcView->ComputeSubresource(
            dstMipLevel,
            dstBaseArrayLayer);


        D3D12_TEXTURE_COPY_LOCATION srcSubresource{};
        srcSubresource.pResource = srcDxcBuffer->GetHandle();
        srcSubresource.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcSubresource.PlacedFootprint.Offset = src->GetShape().GetOffsetInBytes();
        srcSubresource.PlacedFootprint.Footprint.Format = VdToD3DPixelFormat(dstDesc.format);
        srcSubresource.PlacedFootprint.Footprint.Width = copySize.width;
        srcSubresource.PlacedFootprint.Footprint.Height = copySize.height;
        srcSubresource.PlacedFootprint.Footprint.Depth = copySize.depth;
        srcSubresource.PlacedFootprint.Footprint.RowPitch = srcBytesPerRow;

        D3D12_BOX srcRegion {};
        srcRegion.left = 0;
        srcRegion.top = 0;
        srcRegion.front = 0;
        srcRegion.right = copySize.width;
        srcRegion.bottom = copySize.height;
        srcRegion.back = copySize.depth;

        GetCmdList()->CopyTextureRegion(&dstSubresource, dstOrigin.x, dstOrigin.y, dstOrigin.z,
                                   &srcSubresource, &srcRegion);
    }


    void DXCTransferCmdEnc::CopyTextureToBuffer(
        const common::sp<ITextureView>& src,
        const Point3D& srcOrigin,
        std::uint32_t srcMipLevel,
        std::uint32_t srcBaseArrayLayer,
        const common::sp<BufferRange>& dst,
        std::uint32_t dstBytesPerRow,
        std::uint32_t dstBytesPerImage,
        const Size3D& copySize
    ) {


        auto srcDxcView = PtrCast<DXCTextureView>(src.get());
        auto srcDxcTexture = PtrCast<DXCTexture>(src->GetTextureObject().get());
        const auto& srcViewDesc = srcDxcView->GetDesc();

        auto dstBuffer = PtrCast<DXCBuffer>(dst->GetBufferObject().get());

        resources.insert(src);
        resources.insert(dst);

        const auto& srcDesc = srcDxcTexture->GetDesc();

        D3D12_TEXTURE_COPY_LOCATION srcSubresource{};
        srcSubresource.pResource = srcDxcTexture->GetHandle();
        srcSubresource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcSubresource.SubresourceIndex = srcDxcView->ComputeSubresource(
            srcMipLevel,
            srcBaseArrayLayer);


        D3D12_TEXTURE_COPY_LOCATION dstSubresource{};
        dstSubresource.pResource = dstBuffer->GetHandle();
        dstSubresource.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dstSubresource.PlacedFootprint.Offset = dst->GetShape().GetOffsetInBytes();
        dstSubresource.PlacedFootprint.Footprint.Format = VdToD3DPixelFormat(srcDesc.format);
        dstSubresource.PlacedFootprint.Footprint.Width = copySize.width;
        dstSubresource.PlacedFootprint.Footprint.Height = copySize.height;
        dstSubresource.PlacedFootprint.Footprint.Depth = copySize.depth;
        dstSubresource.PlacedFootprint.Footprint.RowPitch = dstBytesPerRow;

        D3D12_BOX srcRegion {};
        srcRegion.left  = srcOrigin.x;
        srcRegion.top   = srcOrigin.y;
        srcRegion.front = srcOrigin.z;
        srcRegion.right  = srcOrigin.x + copySize.width;
        srcRegion.bottom = srcOrigin.y + copySize.height;
        srcRegion.back   = srcOrigin.z + copySize.depth;

        GetCmdList()->CopyTextureRegion(&dstSubresource, 0, 0, 0, &srcSubresource, &srcRegion);
    }

    void DXCTransferCmdEnc::CopyBuffer(
        const common::sp<BufferRange>& source,
        const common::sp<BufferRange>& destination,
        std::uint32_t sizeInBytes
    ){

        auto* srcDxcBuffer = PtrCast<DXCBuffer>(source->GetBufferObject().get());
        auto* dstDxcBuffer = PtrCast<DXCBuffer>(destination->GetBufferObject().get());

        GetCmdList()->CopyBufferRegion(
            dstDxcBuffer->GetHandle(), destination->GetShape().GetOffsetInBytes(),
            srcDxcBuffer->GetHandle(), source->GetShape().GetOffsetInBytes(),
            sizeInBytes
        );
    }

    void DXCTransferCmdEnc::CopyTexture(
        const common::sp<ITextureView>& src,
        const Point3D& srcOrigin,
        std::uint32_t srcMipLevel,
        std::uint32_t srcBaseArrayLayer,
        const common::sp<ITextureView>& dst,
        const Point3D& dstOrigin,
        std::uint32_t dstMipLevel,
        std::uint32_t dstBaseArrayLayer,
        const Size3D& copySize
    ){

        auto srcDxcView = PtrCast<DXCTextureView>(src.get());
        auto srcDxcTexture = PtrCast<DXCTexture>(src->GetTextureObject().get());
        const auto& srcViewDesc = srcDxcView->GetDesc();
        
        auto dstDxcView = PtrCast<DXCTextureView>(dst.get());
        auto dstDxcTexture = PtrCast<DXCTexture>(dst->GetTextureObject().get());
        const auto& dstViewDesc = dstDxcView->GetDesc();

        resources.insert(src);
        resources.insert(dst);

        D3D12_TEXTURE_COPY_LOCATION srcSubresource{};
        srcSubresource.pResource = srcDxcTexture->GetHandle();
        srcSubresource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcSubresource.SubresourceIndex = srcDxcView->ComputeSubresource(
            srcMipLevel, 
            srcBaseArrayLayer);


        D3D12_TEXTURE_COPY_LOCATION dstSubresource{};
        dstSubresource.pResource = dstDxcTexture->GetHandle();
        dstSubresource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstSubresource.SubresourceIndex = dstDxcView->ComputeSubresource(
            dstMipLevel, 
            dstBaseArrayLayer);

        D3D12_BOX srcRegion {};
        srcRegion.left  = srcOrigin.x;
        srcRegion.top   = srcOrigin.y;
        srcRegion.front = srcOrigin.z;
        srcRegion.right  = srcOrigin.x + copySize.width;
        srcRegion.bottom = srcOrigin.y + copySize.height;
        srcRegion.back   = srcOrigin.z + copySize.depth;

        GetCmdList()->CopyTextureRegion(&dstSubresource, dstOrigin.x, dstOrigin.y, dstOrigin.z,
                                   &srcSubresource, &srcRegion);
    }

    void DXCTransferCmdEnc::GenerateMipmaps(const common::sp<ITexture>& texture){
        ///#TODO: remove this function as mipmap generation needs more flexiblity in infcanvas
    }

#pragma endregion XferCmdEnc


#pragma region CmdList

    DXCCommandList::~DXCCommandList(){
        for(auto* p : _passes) {
            delete p;
        }
        _cmdList->Release();
        _cmdAlloc->Release();
    }


    void DXCCommandList::_EndCurrentActivePass() {
        if(_currentPass) {
            _currentPass->EndPass();
            _currentPass = nullptr;
        }
    }

    void DXCCommandList::_BeginDummyPassIfNoActivePass() {
        if(!_currentPass) {
            //Begin a dummy pass for misc command recording
            auto dummyPass = new DXCCmdEncBase(_dev.get(), this);
            //auto* pNewEnc = new _DXCDummyPass(_dev.get(), this);
            _passes.push_back(dummyPass);
            _currentPass = dummyPass;
        }
    }

    static void _FillInTransitionBarrierLegacy(
        D3D12_RESOURCE_BARRIER& barrier,
        const DXCTextureView* pView,
        D3D12_RESOURCE_STATES stateFrom,
        D3D12_RESOURCE_STATES stateTo
    ) {

        auto view = pView;
        auto texture = view->GetTextureObject();
        auto dxcTex = common::PtrCast<DXCTexture>(texture.get());

        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = dxcTex->GetHandle();
        barrier.Transition.Subresource = pView->ComputeSubresource(0,0);
        barrier.Transition.StateBefore = stateFrom;
        barrier.Transition.StateAfter = stateTo;

        
        const auto& viewDesc = view->GetDesc();
        if(viewDesc.arrayLayers == 1 && viewDesc.mipLevels == 1) {
            barrier.Transition.Subresource = pView->ComputeSubresource(0,0);
        } else {
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }
    };
    
    void DXCCommandList::_TransitionColorTargetsToMSAAResolveLayout(
        std::span<const DXCTextureView*> resolveSrcs,
        std::span<const DXCTextureView*> resolveDsts
    ) {
        
        std::vector<D3D12_RESOURCE_BARRIER> barriers{};

        for(auto pSrcView : resolveSrcs) {
            _FillInTransitionBarrierLegacy(
                barriers.emplace_back(), 
                pSrcView,
                D3D12_RESOURCE_STATE_RENDER_TARGET, 
                D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
        }

        
        for(auto pDstView : resolveDsts) {
            _FillInTransitionBarrierLegacy(
                barriers.emplace_back(), 
                pDstView,
                D3D12_RESOURCE_STATE_RENDER_TARGET, 
                D3D12_RESOURCE_STATE_RESOLVE_DEST);
        }

        if(!barriers.empty())
            _cmdList->ResourceBarrier(barriers.size(), barriers.data());
    }

    void DXCCommandList::_TransitionColorTargetsFromMSAAResolveLayout(
        std::span<const DXCTextureView*> resolveSrcs,
        std::span<const DXCTextureView*> resolveDsts
    ) {
        std::vector<D3D12_RESOURCE_BARRIER> barriers{};

        for(auto pSrcView : resolveSrcs) {
            _FillInTransitionBarrierLegacy(
                barriers.emplace_back(), 
                pSrcView,
                D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET );
        }

        
        for(auto pDstView : resolveDsts) {
            _FillInTransitionBarrierLegacy(
                barriers.emplace_back(), 
                pDstView,
                D3D12_RESOURCE_STATE_RESOLVE_DEST, 
                D3D12_RESOURCE_STATE_RENDER_TARGET);
        }

        if(!barriers.empty())
            _cmdList->ResourceBarrier(barriers.size(), barriers.data());
    }
    
    void DXCCommandList::_TransitionDSTargetsToMSAAResolveLayout(
        DXCTextureView* src, DXCTextureView* dst, bool isSrcReadOnly
    ) {
        
        D3D12_RESOURCE_BARRIER barriers[2]{};

        auto srcState = isSrcReadOnly ? D3D12_RESOURCE_STATE_DEPTH_READ
                          : D3D12_RESOURCE_STATE_DEPTH_WRITE;

        _FillInTransitionBarrierLegacy(
            barriers[0], 
            src,
            srcState,
            D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    
        _FillInTransitionBarrierLegacy(
            barriers[1], 
            dst,
            srcState,
            D3D12_RESOURCE_STATE_RESOLVE_DEST);

        _cmdList->ResourceBarrier(2, barriers);
    }
    void DXCCommandList::_TransitionDSTargetsFromMSAAResolveLayout(
        DXCTextureView* src, DXCTextureView* dst, bool isSrcReadOnly
    ) {
        D3D12_RESOURCE_BARRIER barriers[2]{};

        
        auto srcState = isSrcReadOnly ? D3D12_RESOURCE_STATE_DEPTH_READ
                          : D3D12_RESOURCE_STATE_DEPTH_WRITE;


        _FillInTransitionBarrierLegacy(
            barriers[0], 
            src,
            D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
            srcState);
    

        _FillInTransitionBarrierLegacy(
            barriers[1], 
            dst,
            D3D12_RESOURCE_STATE_RESOLVE_DEST, 
            srcState);

        _cmdList->ResourceBarrier(2, barriers);
    }

    void DXCCommandList::Begin(){

        for(auto* p : _passes) {
            delete p;
        }

        _passes.clear();
        _currentPass = nullptr;

        _devRes.clear();
        // Command list allocators can only be reset when the associated
        // command lists have finished execution on the GPU; apps should use
        // fences to determine GPU execution progress.
        ThrowIfFailed(_cmdAlloc->Reset());

        // However, when ExecuteCommandList() is called on a particular command
        // list, that command list can then be reset at any time and must be before
        // re-recording.
        ThrowIfFailed(_cmdList->Reset(_cmdAlloc, nullptr));

    }
    void DXCCommandList::End(){

        //CHK_RENDERPASS_ENDED();
        _EndCurrentActivePass();

        ThrowIfFailed(_cmdList->Close());

    }


    IRenderCommandEncoder& DXCCommandList::BeginRenderPass(const RenderPassAction& actions) {
        //CHK_RENDERPASS_ENDED();
        _EndCurrentActivePass();
        ////Record render pass

        //auto dxcfb = common::SPCast<DXCFrameBufferBase>(fb);

        DXCRenderCmdEnc* pNewEnc = new DXCRenderCmdEnc(_dev.get(), this, actions);

        ///#TODO: really support render passes using ID3D12GraphicsCommandList4::BeginRenderPass.
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;
        //

        return *pNewEnc;
    }

    IComputeCommandEncoder& DXCCommandList::BeginComputePass() {

        //CHK_RENDERPASS_ENDED();
        _EndCurrentActivePass();
        ////Record render pass

        auto* pNewEnc = new DXCComputeCmdEnc(_dev.get(), this);
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;

        return *pNewEnc;
    }
    ITransferCommandEncoder& DXCCommandList::BeginTransferPass() {

        //CHK_RENDERPASS_ENDED();
        _EndCurrentActivePass();
        ////Record render pass

        auto* pNewEnc = new DXCTransferCmdEnc(_dev.get(), this);
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;

        return *pNewEnc;
    }
    //virtual IBaseCommandEncoder* BeginWithBasicEncoder() = 0;

    void DXCCommandList::EndPass() {
        CHK_RENDERPASS_BEGUN();
        _currentPass->EndPass();
        _currentPass = nullptr;
    }


    static D3D12_RESOURCE_STATES _EnnhancedToLegacyBarrierFlags(
        const D3D12_BARRIER_SYNC&,
        const D3D12_BARRIER_ACCESS& access
    ){
        D3D12_RESOURCE_STATES legacyState{};

        if(access & D3D12_BARRIER_ACCESS_COPY_SOURCE)
            legacyState |= D3D12_RESOURCE_STATE_COPY_SOURCE;
        if(access & D3D12_BARRIER_ACCESS_COPY_DEST)
            legacyState |= D3D12_RESOURCE_STATE_COPY_DEST;

        if(access & ( D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ
                    | D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE))
            legacyState |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

        if(access & D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT)
            legacyState |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

        if(access & D3D12_BARRIER_ACCESS_RENDER_TARGET)
            legacyState |= D3D12_RESOURCE_STATE_RENDER_TARGET;

        if(access & D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE)
            legacyState |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
        if(access & D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ)
            legacyState |= D3D12_RESOURCE_STATE_DEPTH_READ;

        if(access & D3D12_BARRIER_ACCESS_UNORDERED_ACCESS) {
            legacyState |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }

        if(access & D3D12_BARRIER_ACCESS_SHADER_RESOURCE) {
            legacyState |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            //if(sync & D3D12_BARRIER_SYNC_PIXEL_SHADING) {
                legacyState |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            //}
        }

        if(access & D3D12_BARRIER_ACCESS_INDEX_BUFFER) {
            legacyState |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
        }

        if(access & (D3D12_BARRIER_ACCESS_VERTEX_BUFFER | D3D12_BARRIER_ACCESS_CONSTANT_BUFFER)) {
            legacyState |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        }

        return legacyState;
    };

    static D3D12_BARRIER_SYNC _GetSyncStages(const alloy::PipelineStages& stages, bool) {
        D3D12_BARRIER_SYNC flags = D3D12_BARRIER_SYNC_NONE;

        if(stages[alloy::PipelineStage::AllCommands])
            return D3D12_BARRIER_SYNC_ALL;

        if(stages[alloy::PipelineStage::AllGraphics])
            flags |= D3D12_BARRIER_SYNC_DRAW;
        if(stages[alloy::PipelineStage::AllShaders])
            flags |= D3D12_BARRIER_SYNC_ALL_SHADING;

        if(stages[alloy::PipelineStage::DrawIndirect])
            flags |= D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
        if(stages[alloy::PipelineStage::VertexInput])
            flags |= D3D12_BARRIER_SYNC_INDEX_INPUT;
        if(stages[alloy::PipelineStage::VertexShader])
            flags |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
        if(stages[alloy::PipelineStage::MeshShader])
            flags |= D3D12_BARRIER_SYNC_NON_PIXEL_SHADING;
        if(stages[alloy::PipelineStage::FragmentShader])
            flags |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
        if(stages[alloy::PipelineStage::DepthStencil])
            flags |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
        if(stages[alloy::PipelineStage::ColorOutput])
            flags |= D3D12_BARRIER_SYNC_RENDER_TARGET;
        if(stages[alloy::PipelineStage::ComputeShader])
            flags |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
        if(stages[alloy::PipelineStage::RayTracing])
            flags |= D3D12_BARRIER_SYNC_RAYTRACING;
        if(stages[alloy::PipelineStage::Copy])
            flags |= D3D12_BARRIER_SYNC_COPY;
        if(stages[alloy::PipelineStage::BuildAS])
            flags |= D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
        return flags;
    };

    static D3D12_BARRIER_ACCESS _GetAccessFlags(const alloy::ResourceAccesses& accesses) {
        D3D12_BARRIER_ACCESS flags {};

        if(!accesses)
            return D3D12_BARRIER_ACCESS_NO_ACCESS;

        if(accesses[alloy::ResourceAccess::IndirectArgumentRead])
            flags |= D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
        if(accesses[alloy::ResourceAccess::VertexBufferRead])
            flags |= D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
        if(accesses[alloy::ResourceAccess::IndexBufferRead])
            flags |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
        if(accesses[alloy::ResourceAccess::ConstantBufferRead])
            flags |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
        if(accesses[alloy::ResourceAccess::ShaderResourceRead])
            flags |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
        if(accesses[alloy::ResourceAccess::UnorderedAccess])
            flags |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
        if(accesses[alloy::ResourceAccess::RenderTarget])
            flags |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
        if(accesses[alloy::ResourceAccess::DepthStencilRead])
            flags |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
        if(accesses[alloy::ResourceAccess::DepthStencilWrite])
            flags |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
        if(accesses[alloy::ResourceAccess::CopySource])
            flags |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
        if(accesses[alloy::ResourceAccess::CopyDest])
            flags |= D3D12_BARRIER_ACCESS_COPY_DEST;
        if(accesses[alloy::ResourceAccess::AccelerationStructureRead])
            flags |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
        if(accesses[alloy::ResourceAccess::AccelerationStructureWrite])
            flags |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;
        if(accesses[alloy::ResourceAccess::Present])
            flags |= D3D12_BARRIER_ACCESS_COMMON;
        return flags;
    };

    template<typename T>
    static void _PopulateBarrierAccess(const ResourceAccessMask& from,
                                       const ResourceAccessMask& to,
                                       T& barrier
    ) {
        barrier.AccessAfter = _GetAccessFlags(to);
        barrier.AccessBefore = _GetAccessFlags(from);
    }

    static void _EnnhancedToLegacyBarrier(
        const D3D12_GLOBAL_BARRIER& data,
        D3D12_RESOURCE_BARRIER& barrier
    ) {
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.StateBefore = _EnnhancedToLegacyBarrierFlags(data.SyncBefore, data.AccessBefore);
        barrier.Transition.StateAfter= _EnnhancedToLegacyBarrierFlags(data.SyncAfter, data.AccessAfter);
    }

    void DXCCommandList::Barrier(std::span<const alloy::BarrierOp> descs) {

        std::vector<D3D12_RESOURCE_BARRIER> barriers{};

        for(auto& desc : descs) {
            D3D12_RESOURCE_BARRIER barrier { };
            bool isBarrierNecessary = true;

            if (std::holds_alternative<alloy::BufferBarrierOp>(desc)) {

                auto& barrierDesc = std::get<alloy::BufferBarrierOp>(desc);

                auto range = barrierDesc.buffer;
                auto buffer = range->GetBufferObject();
                auto dxcBuffer = PtrCast<DXCBuffer>(buffer.get());
                _devRes.insert(range);

                auto syncAfter = _GetSyncStages(barrierDesc.to.stages, false);
                auto syncBefore = _GetSyncStages(barrierDesc.from.stages, true);

                D3D12_GLOBAL_BARRIER dxcBarrierFlags{};
                dxcBarrierFlags.SyncAfter = syncAfter;
                dxcBarrierFlags.SyncBefore = syncBefore;
                _PopulateBarrierAccess(barrierDesc.from.access, barrierDesc.to.access, dxcBarrierFlags);
                _EnnhancedToLegacyBarrier(dxcBarrierFlags, barrier);
                barrier.Transition.pResource = dxcBuffer->GetHandle();
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            }
            else {
                auto& texDesc = std::get<alloy::TextureBarrierOp>(desc);

                auto view = texDesc.texture;
                auto texture = view->GetTextureObject();
                auto dxcTex = common::PtrCast<DXCTexture>(texture.get());
                _devRes.insert(view);

                auto syncAfter = _GetSyncStages(texDesc.to.stages, false);
                auto syncBefore = _GetSyncStages(texDesc.from.stages, true);

                D3D12_GLOBAL_BARRIER dxcBarrierFlags{};
                dxcBarrierFlags.SyncAfter = syncAfter;
                dxcBarrierFlags.SyncBefore = syncBefore;
                _PopulateBarrierAccess(texDesc.from.access, texDesc.to.access, dxcBarrierFlags);
                _EnnhancedToLegacyBarrier(dxcBarrierFlags, barrier);
                barrier.Transition.pResource = dxcTex->GetHandle();

                
                const auto& viewDesc = view->GetDesc();
                if( (viewDesc.arrayLayers == 1 && viewDesc.mipLevels == 1) &&
                    (viewDesc.aspect != ITextureView::Aspect::DepthStencil)
                ) {
                    const auto& texDesc = texture->GetDesc();
                    auto dxcTexView = PtrCast<DXCTextureView>(view.get());

                    barrier.Transition.Subresource = dxcTexView->ComputeSubresource(0,0);
                } else {
                    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                }
                
            }

            if (barrier.Transition.StateBefore == barrier.Transition.StateAfter) {
                isBarrierNecessary = false;
            }

            if (isBarrierNecessary) {
                barriers.push_back(barrier);
            }

        }

        if(!barriers.empty())
            _cmdList->ResourceBarrier(barriers.size(), barriers.data());

    }

    void DXCCommandList::PushDebugGroup(const std::string& name, const Color4f& color) {
        _BeginDummyPassIfNoActivePass();
        auto pixColor = Color4fToPixColor(color);
        _currentPass->PushDebugGroup(name, pixColor);
    };

    void DXCCommandList::PopDebugGroup() {
        _BeginDummyPassIfNoActivePass();
        _currentPass->PopDebugGroup();
    };

    void DXCCommandList::InsertDebugMarker(const std::string& name, const Color4f& color) {
        _BeginDummyPassIfNoActivePass();
        auto pixColor = Color4fToPixColor(color);
        _currentPass->InsertDebugMarker(name, pixColor);
    };

#pragma endregion CmdList

    static void _PopulateTextureBarrier(const alloy::TextureBarrierOp& desc,
                                              D3D12_TEXTURE_BARRIER& barrier
    ) {

        auto _GetTexLayout = [](const alloy::TextureLayout& layout) -> D3D12_BARRIER_LAYOUT {
            switch(layout) {
                case alloy::TextureLayout::Undefined: return D3D12_BARRIER_LAYOUT_UNDEFINED;
                case alloy::TextureLayout::General: return D3D12_BARRIER_LAYOUT_COMMON;
                case alloy::TextureLayout::ShaderReadOnly: return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
                case alloy::TextureLayout::Storage: return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
                case alloy::TextureLayout::ColorAttachment: return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
                case alloy::TextureLayout::DepthStencilReadOnly: return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
                case alloy::TextureLayout::DepthStencilWrite: return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
                case alloy::TextureLayout::CopySource: return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
                case alloy::TextureLayout::CopyDest: return D3D12_BARRIER_LAYOUT_COPY_DEST;
                case alloy::TextureLayout::ResolveSource: return D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;
                case alloy::TextureLayout::ResolveDest: return D3D12_BARRIER_LAYOUT_RESOLVE_DEST;
                case alloy::TextureLayout::Present: return D3D12_BARRIER_LAYOUT_PRESENT;

                default: return D3D12_BARRIER_LAYOUT_COMMON;
            }
        };

        barrier.LayoutAfter = _GetTexLayout(desc.to.layout);
        barrier.LayoutBefore = _GetTexLayout(desc.from.layout);
    }

    IRenderCommandEncoder& DXCCommandList6::BeginRenderPass(const RenderPassAction& actions) {
        //CHK_RENDERPASS_ENDED();
        _EndCurrentActivePass();
        ////Record render pass

        //auto dxcfb = common::SPCast<DXCFrameBufferBase>(fb);

        DXCRenderCmdEnc* pNewEnc = new DXCRenderCmdEnc6(_dev.get(), this, actions);

        ///#TODO: really support render passes using ID3D12GraphicsCommandList4::BeginRenderPass.
        _passes.push_back(pNewEnc);
        _currentPass = pNewEnc;
        //

        return *pNewEnc;
    }

    void DXCCommandList7::Barrier(std::span<const alloy::BarrierOp> descs) {

        std::vector<D3D12_GLOBAL_BARRIER> memBarriers;
        std::vector<D3D12_BUFFER_BARRIER> bufBarriers;
        std::vector<D3D12_TEXTURE_BARRIER> texBarrier;

        for(auto& desc : descs) {
            if(std::holds_alternative<alloy::BufferBarrierOp>(desc)) {
                bufBarriers.emplace_back();
                auto& barrier = bufBarriers.back();
                auto& barrierDesc = std::get<alloy::BufferBarrierOp>(desc);

                auto range = barrierDesc.buffer;
                auto buffer = range->GetBufferObject();
                auto dxcBuffer = PtrCast<DXCBuffer>(buffer.get());
                _devRes.insert(range);

                barrier.SyncAfter = _GetSyncStages(barrierDesc.to.stages, false);
                barrier.SyncBefore = _GetSyncStages(barrierDesc.from.stages, true);

                _PopulateBarrierAccess(barrierDesc.from.access, barrierDesc.to.access, barrier);
                barrier.pResource = dxcBuffer->GetHandle();

                const auto& rangeDesc = range->GetShape();
                barrier.Offset = rangeDesc.GetOffsetInBytes();
                barrier.Size = rangeDesc.GetSizeInBytes();
            }
            else {
                texBarrier.emplace_back();
                auto& barrier = texBarrier.back();
                auto& texDesc = std::get<alloy::TextureBarrierOp>(desc);

                auto view = texDesc.texture;
                auto texture = view->GetTextureObject();
                auto dxcTex = common::PtrCast<DXCTexture>(texture.get());
                _devRes.insert(view);
                
                barrier.SyncAfter = _GetSyncStages(texDesc.to.stages, false);
                barrier.SyncBefore = _GetSyncStages(texDesc.from.stages, true);

                _PopulateBarrierAccess(texDesc.from.access, texDesc.to.access, barrier);
                _PopulateTextureBarrier(texDesc, barrier);
                barrier.pResource = dxcTex->GetHandle();

                const auto& viewDesc = view->GetDesc();
                barrier.Subresources.IndexOrFirstMipLevel = viewDesc.baseMipLevel;
                barrier.Subresources.NumMipLevels = viewDesc.mipLevels;
                barrier.Subresources.FirstArraySlice = viewDesc.baseArrayLayer;
                barrier.Subresources.NumArraySlices = viewDesc.arrayLayers;

                switch (viewDesc.aspect) {
                case ITextureView::Aspect::Stencil:
                    barrier.Subresources.FirstPlane = 1;
                    barrier.Subresources.NumPlanes = 1;
                    break;
                    
                case ITextureView::Aspect::DepthStencil:
                    barrier.Subresources.FirstPlane = 0;
                    barrier.Subresources.NumPlanes = 2;
                    break;

                case ITextureView::Aspect::Color:
                case ITextureView::Aspect::Depth:
                default:
                    barrier.Subresources.FirstPlane = 0;
                    barrier.Subresources.NumPlanes = 1;
                }
            }
        }

        std::vector<D3D12_BARRIER_GROUP> barrierGrps;
        if(!memBarriers.empty()) {
            barrierGrps.emplace_back();
            auto& grp = barrierGrps.back();
            grp.Type = D3D12_BARRIER_TYPE_GLOBAL;
            grp.NumBarriers = memBarriers.size();
            grp.pGlobalBarriers = memBarriers.data();
        }

        if(!bufBarriers.empty()) {
            barrierGrps.emplace_back();
            auto& grp = barrierGrps.back();
            grp.Type = D3D12_BARRIER_TYPE_BUFFER;
            grp.NumBarriers = bufBarriers.size();
            grp.pBufferBarriers = bufBarriers.data();
        }

        if(!texBarrier.empty()) {
            barrierGrps.emplace_back();
            auto& grp = barrierGrps.back();
            grp.Type = D3D12_BARRIER_TYPE_TEXTURE;
            grp.NumBarriers = texBarrier.size();
            grp.pTextureBarriers = texBarrier.data();
        }

        GetCmdList()->Barrier(barrierGrps.size(), barrierGrps.data());
    }

    
    
    static void _FillInTransitionBarrierResEnhanced(
        D3D12_TEXTURE_BARRIER& barrier,
        const DXCTextureView* pView
    ) {

        auto view = pView;
        auto texture = view->GetTextureObject();
        auto dxcTex = common::PtrCast<DXCTexture>(texture.get());
        const auto& viewDesc = view->GetDesc();

        barrier.pResource = dxcTex->GetHandle();
        barrier.Subresources.IndexOrFirstMipLevel = viewDesc.baseMipLevel;
        barrier.Subresources.NumMipLevels = viewDesc.mipLevels;
        barrier.Subresources.FirstArraySlice = viewDesc.baseArrayLayer;
        barrier.Subresources.NumArraySlices = viewDesc.arrayLayers;
        barrier.Subresources.FirstPlane = 0;
        barrier.Subresources.NumPlanes = 1;

    };

    void DXCCommandList7::_TransitionColorTargetsToMSAAResolveLayout(
        std::span<const DXCTextureView*> resolveSrcs,
        std::span<const DXCTextureView*> resolveDsts
    ) {
        
        std::vector<D3D12_TEXTURE_BARRIER> barriers{};
        
        for(auto pSrcView : resolveSrcs) {
            auto& barrier = barriers.emplace_back();
            _FillInTransitionBarrierResEnhanced(
                barrier, 
                pSrcView);
            barrier.SyncBefore = D3D12_BARRIER_SYNC_RENDER_TARGET;
            barrier.AccessBefore = D3D12_BARRIER_ACCESS_RENDER_TARGET;
            barrier.LayoutBefore = D3D12_BARRIER_LAYOUT_RENDER_TARGET;

            barrier.SyncAfter = D3D12_BARRIER_SYNC_RESOLVE;
            barrier.AccessAfter = D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
            barrier.LayoutAfter = D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;
        }

        
        for(auto pDstView : resolveDsts) {
            auto& barrier = barriers.emplace_back();
            _FillInTransitionBarrierResEnhanced(
                barrier, 
                pDstView);
            barrier.SyncBefore = D3D12_BARRIER_SYNC_RENDER_TARGET;
            barrier.AccessBefore = D3D12_BARRIER_ACCESS_RENDER_TARGET;
            barrier.LayoutBefore = D3D12_BARRIER_LAYOUT_RENDER_TARGET;

            barrier.SyncAfter = D3D12_BARRIER_SYNC_RESOLVE;
            barrier.AccessAfter = D3D12_BARRIER_ACCESS_RESOLVE_DEST;
            barrier.LayoutAfter = D3D12_BARRIER_LAYOUT_RESOLVE_DEST;
        }

        if(!barriers.empty()) {
            D3D12_BARRIER_GROUP grp{};
            grp.Type = D3D12_BARRIER_TYPE_TEXTURE;
            grp.NumBarriers = barriers.size();
            grp.pTextureBarriers = barriers.data();

            GetCmdList()->Barrier(1, &grp);
        }
    }

    void DXCCommandList7::_TransitionColorTargetsFromMSAAResolveLayout(
        std::span<const DXCTextureView*> resolveSrcs,
        std::span<const DXCTextureView*> resolveDsts
    ) {
        
        std::vector<D3D12_TEXTURE_BARRIER> barriers{};
        
        for(auto pSrcView : resolveSrcs) {
            auto& barrier = barriers.emplace_back();
            _FillInTransitionBarrierResEnhanced(
                barrier, 
                pSrcView);
            barrier.SyncBefore = D3D12_BARRIER_SYNC_RESOLVE;
            barrier.AccessBefore = D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
            barrier.LayoutBefore = D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;

            barrier.SyncAfter = D3D12_BARRIER_SYNC_ALL;
            barrier.AccessAfter = D3D12_BARRIER_ACCESS_RENDER_TARGET;
            barrier.LayoutAfter = D3D12_BARRIER_LAYOUT_RENDER_TARGET;
        }

        
        for(auto pDstView : resolveDsts) {
            auto& barrier = barriers.emplace_back();
            _FillInTransitionBarrierResEnhanced(
                barrier, 
                pDstView);
            barrier.SyncBefore = D3D12_BARRIER_SYNC_RESOLVE;
            barrier.AccessBefore = D3D12_BARRIER_ACCESS_RESOLVE_DEST;
            barrier.LayoutBefore = D3D12_BARRIER_LAYOUT_RESOLVE_DEST;

            barrier.SyncAfter = D3D12_BARRIER_SYNC_ALL;
            barrier.AccessAfter = D3D12_BARRIER_ACCESS_RENDER_TARGET;
            barrier.LayoutAfter = D3D12_BARRIER_LAYOUT_RENDER_TARGET;
        }

        if(!barriers.empty()) {
            D3D12_BARRIER_GROUP grp{};
            grp.Type = D3D12_BARRIER_TYPE_TEXTURE;
            grp.NumBarriers = barriers.size();
            grp.pTextureBarriers = barriers.data();

            GetCmdList()->Barrier(1, &grp);
        }
    }
    
    void DXCCommandList7::_TransitionDSTargetsToMSAAResolveLayout(
        DXCTextureView* src, DXCTextureView* dst, bool isSrcReadOnly
    ) {
        
        auto srcAccess = isSrcReadOnly ? D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ
                          : D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
        auto srcLayout = isSrcReadOnly ? D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ
                          : D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
        
        D3D12_TEXTURE_BARRIER barriers[2]{};
        {
            auto& barrier = barriers[0];
            _FillInTransitionBarrierResEnhanced(
                barrier, 
                src);
            barrier.SyncBefore = D3D12_BARRIER_SYNC_DEPTH_STENCIL;
            barrier.AccessBefore = srcAccess;
            barrier.LayoutBefore = srcLayout;

            barrier.SyncAfter = D3D12_BARRIER_SYNC_RESOLVE;
            barrier.AccessAfter = D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
            barrier.LayoutAfter = D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;
        }
        {
            auto& barrier = barriers[0];
            _FillInTransitionBarrierResEnhanced(
                barrier, 
                src);
            barrier.SyncBefore = D3D12_BARRIER_SYNC_DEPTH_STENCIL;
            barrier.AccessBefore = srcAccess;
            barrier.LayoutBefore = srcLayout;

            barrier.SyncAfter = D3D12_BARRIER_SYNC_RESOLVE;
            barrier.AccessAfter = D3D12_BARRIER_ACCESS_RESOLVE_DEST;
            barrier.LayoutAfter = D3D12_BARRIER_LAYOUT_RESOLVE_DEST;
        }
        {
            D3D12_BARRIER_GROUP grp{};
            grp.Type = D3D12_BARRIER_TYPE_TEXTURE;
            grp.NumBarriers = 2;
            grp.pTextureBarriers = barriers;

            GetCmdList()->Barrier(1, &grp);
        }

    }
    void DXCCommandList7::_TransitionDSTargetsFromMSAAResolveLayout(
        DXCTextureView* src, DXCTextureView* dst, bool isSrcReadOnly
    ) {
        auto srcAccess = isSrcReadOnly ? D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ
                          : D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
        auto srcLayout = isSrcReadOnly ? D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ
                          : D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
        
        D3D12_TEXTURE_BARRIER barriers[2]{};
        {
            auto& barrier = barriers[0];
            _FillInTransitionBarrierResEnhanced(
                barrier, 
                src);
            barrier.SyncBefore = D3D12_BARRIER_SYNC_RESOLVE;
            barrier.AccessBefore = D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
            barrier.LayoutBefore = D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;

            barrier.SyncAfter = D3D12_BARRIER_SYNC_ALL;
            barrier.AccessAfter = srcAccess;
            barrier.LayoutAfter = srcLayout;
        }
        {
            auto& barrier = barriers[0];
            _FillInTransitionBarrierResEnhanced(
                barrier, 
                src);
            barrier.SyncBefore = D3D12_BARRIER_SYNC_RESOLVE;
            barrier.AccessBefore = D3D12_BARRIER_ACCESS_RESOLVE_DEST;
            barrier.LayoutBefore = D3D12_BARRIER_LAYOUT_RESOLVE_DEST;

            barrier.SyncAfter = D3D12_BARRIER_SYNC_ALL;
            barrier.AccessAfter = srcAccess;
            barrier.LayoutAfter = srcLayout;
        }
        {
            D3D12_BARRIER_GROUP grp{};
            grp.Type = D3D12_BARRIER_TYPE_TEXTURE;
            grp.NumBarriers = 2;
            grp.pTextureBarriers = barriers;

            GetCmdList()->Barrier(1, &grp);
        }
    }


} // namespace alloy::dxc
