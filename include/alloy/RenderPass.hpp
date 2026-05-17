#pragma once
//
//  RenderPass.hpp
//  MetalWrapper
//
//  Created by ZH Li on 2023/12/11.
//
#include "common/RefCnt.hpp"
#include "Types.hpp"
#include "FrameBuffer.hpp"

#include <vector>
#include <optional>

//#include <Metal/Metal.h>

namespace alloy {

   
        
        enum class LoadAction {
            // No explicit renderpass on dx12,
            // this is effectively LoadAction::Load
            DontCare,
            Load,
            Clear,
            
            // Readonly usage, will disable StoreAction
            // On vulkan platform without VK_KHR_load_store_op_none
            // support, this is effectively
            // LoadAction::Load + StoreAction::Store
            ReadOnly,
        };
        
        enum class StoreAction {
            
            // DiscardResource on dx12
            // dont care on Vulkan/Metal
            DontCare,

            Store,

            //Not supported other than metal
            //MultisampleResolve = 2,
            //StoreAndMultisampleResolve = 3,
            //CustomSampleDepthStore = 5,
        };
        
        enum class MSAADepthResolveMode {
            None = 0,
            Min = 1,
            Max = 2,
        };
        
        struct AttachmentAction {
            // The target texture to render into. 
            //   For color attachments, must have renderTarget usage
            //   For depth & stencil attachments, must have depthStencil usage
            // Note: limitations on separate depth and stencil targets:
            //    DX12: no support at all, must use combined depth/stencil format
            //          and pass in same targets for depth and stencil attachment
            //    Vulkan: support separate usage with VK_KHR_dynamic_rendering, which is 
            //            required by alloy
            //    Metal: support separate usage by default
            common::sp<IRenderTarget> target;

                            
            /// The array layer to render to. This value must be less than <see cref="Texture.ArrayLayers"/> in the target
            //std::uint32_t arrayLayer; //seems to translate to depthPlane in metal
                            
            /// The mip level to render to. This value must be less than <see cref="Texture.MipLevels"/> in the target
            //std::uint32_t mipLevel;
            
            
                            
            //std::uint32_t resolveArrayLayer; //seems to translate to depthPlane in metal
                            
            //std::uint32_t resolveMipLevel;
            
            LoadAction loadAction;
            StoreAction storeAction;
        };
        
        struct ColorAttachmentAction : public AttachmentAction {
            Color4f clearColor;

            /// The target texture for MSAA resolve. Can be null
            // Use averaging mode to resolve
            common::sp<IRenderTarget> msaaResolveTarget;
        };
        
        struct DepthAttachmentAction : public AttachmentAction {
            float clearDepth;

            /// The target texture for MSAA resolve. Can be null
            common::sp<IRenderTarget> msaaResolveTarget;

            //DX12 can resolve depth using ResolveSubresourceRegion
            //DX12 & Vulkan support AVERAGE/MIN/MAX
            //Metal only support MIN/MAX
            MSAADepthResolveMode msaaResolveMode;
        };
        
        struct StencilAttachmentAction : public AttachmentAction {
            uint32_t clearStencil;
            
            //DX12 can resolve stencil using ResolveSubresourceRegion
            //Vulkan&Metal can resolve stencil inside renderpass
            //  None of Metal's stencil resolve modes are in DX12/Vulkan
            //So we don't enable stencil MSAA resolve.
            ///False: no filter applyed,
            ///True: use whatever depth resolve filter is selected.
            //bool enableResolveFilter;
            
            //MSAAStencilResolveMode resolveFilter;
        };
        
        struct RenderPassAction {
            std::optional<DepthAttachmentAction> depthTargetAction;
            std::optional<StencilAttachmentAction> stencilTargetAction;
            std::vector<ColorAttachmentAction> colorTargetActions;
            
        };
        
}
