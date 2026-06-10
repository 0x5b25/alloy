#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include "alloy/common/RefCnt.hpp"
#include "alloy/Pipeline.hpp"

#include "VulkanBindableResource.hpp"

#include <vector>


namespace alloy::vk
{
    class VulkanDevice;

    class VulkanPipelineBase {

    protected:
        common::sp<VulkanDevice> dev;

        //std::vector<VulkanResourceLayout::PushConstantInfo> pushConstants;

        VkPipeline _devicePipeline;

        std::uint32_t resourceSetCount;
        std::uint32_t dynamicOffsetsCount;
        //public override bool IsComputePipeline { get; }

        //public ResourceRefCount RefCount { get; }

        //For bookkeeping, prevent resources used in pipeline from
        //being destroyed if no other references.
        common::sp<VulkanResourceLayout> _layout;

    protected:
        VulkanPipelineBase(const common::sp<VulkanDevice>& dev) : dev(dev){}

    public:
        virtual ~VulkanPipelineBase();

        const VkPipeline& GetHandle() const {return _devicePipeline;}
        //const VkPipelineLayout& GetLayout() const { return _pipelineLayout; }
        std::uint32_t GetResourceSetCount() const { return resourceSetCount; }
        std::uint32_t GetDynamicOffsetCount() const {return dynamicOffsetsCount;}

        const std::vector<VulkanResourceLayout::PushConstantInfo>& 
        GetPushConstants() const { 
            static const decltype(GetPushConstants()) emptyInfo { };
            return _layout ? _layout->GetPushConstants() : emptyInfo;
        }

        
        const VulkanResourceLayout* GetLayout() const { return _layout.get(); }

    };

    class VulkanComputePipeline : public IComputePipeline, public VulkanPipelineBase{

        VulkanComputePipeline(
            const common::sp<VulkanDevice>& dev
        ) : VulkanPipelineBase(dev){}


    public:
        ~VulkanComputePipeline();

        static common::sp<IComputePipeline> Make(
            const common::sp<VulkanDevice>& dev,
            const ComputePipelineDescription& desc
        );

        virtual common::sp<IResourceLayout> GetLayout() const override  {
            return _layout;
        }
    };


    class VulkanGraphicsPipeline : public IGfxPipeline, public VulkanPipelineBase{

        //VkRenderPass _renderPass;

        bool scissorTestEnabled;

        VulkanGraphicsPipeline(
            const common::sp<VulkanDevice>& dev
        ) : VulkanPipelineBase(dev){}


    public:
        ~VulkanGraphicsPipeline();

        static common::sp<IGfxPipeline> Make(
            const common::sp<VulkanDevice>& dev,
            const GraphicsPipelineDescription& desc
        );

        virtual common::sp<IResourceLayout> GetLayout() const override  {
            return _layout;
        }
    };


    class VulkanMeshShaderPipeline : public IMeshShaderPipeline, public VulkanPipelineBase{

        //VkRenderPass _renderPass;

        bool scissorTestEnabled;

        VulkanMeshShaderPipeline(
            const common::sp<VulkanDevice>& dev
        ) : VulkanPipelineBase(dev){}


    public:
        ~VulkanMeshShaderPipeline() override {}

        static common::sp<IMeshShaderPipeline> Make(
            const common::sp<VulkanDevice>& dev,
            const MeshShaderPipelineDescription& desc
        );

        
        virtual common::sp<IResourceLayout> GetLayout() const override  {
            return _layout;
        }

    };


} // namespace alloy
