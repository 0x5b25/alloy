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
            DontCare = 0,
            Load = 1,
            Clear = 2,
        };
        
        enum class StoreAction {
            DontCare = 0,
            Store = 1,

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
            /// The target texture to render into. For color attachments, this resource must have been created with the
            /// <see cref="TextureUsage.RenderTarget"/> flag. For depth attachments, this resource must have been created with the
            /// <see cref="TextureUsage.DepthStencil"/> flag.
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
