#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include "veldrid/Pipeline.hpp"

#include <vector>


namespace Veldrid
{
    class VulkanDevice;

    VkRenderPass CreateFakeRenderPassForCompat(
        VulkanDevice* dev,
        const OutputDescription& outputDesc,
        VkSampleCountFlagBits sampleCnt
    );

    class VulkanPipelineBase : public Pipeline{

    protected:
        VulkanDevice* _Dev() {return reinterpret_cast<VulkanDevice*>(dev.get());}

        VkPipelineLayout _pipelineLayout;
        VkPipeline _devicePipeline;
        
        std::uint32_t resourceSetCount;
        std::uint32_t dynamicOffsetsCount;
        //public override bool IsComputePipeline { get; }

        //public ResourceRefCount RefCount { get; }


    protected:
        VulkanPipelineBase(const sp<GraphicsDevice>& dev) : Pipeline(dev){}
/*
        VulkanPipelineBase(VkGraphicsDevice gd, ref ComputePipelineDescription description)
            : base(ref description)
        {
            _gd = gd;
            IsComputePipeline = true;
            RefCount = new ResourceRefCount(DisposeCore);

            VkComputePipelineCreateInfo pipelineCI = VkComputePipelineCreateInfo.New();

            // Pipeline Layout
            ResourceLayout[] resourceLayouts = description.ResourceLayouts;
            VkPipelineLayoutCreateInfo pipelineLayoutCI = VkPipelineLayoutCreateInfo.New();
            pipelineLayoutCI.setLayoutCount = (uint)resourceLayouts.Length;
            VkDescriptorSetLayout* dsls = stackalloc VkDescriptorSetLayout[resourceLayouts.Length];
            for (int i = 0; i < resourceLayouts.Length; i++)
            {
                dsls[i] = Util.AssertSubtype<ResourceLayout, VkResourceLayout>(resourceLayouts[i]).DescriptorSetLayout;
            }
            pipelineLayoutCI.pSetLayouts = dsls;

            vkCreatePipelineLayout(_gd.Device, ref pipelineLayoutCI, null, out _pipelineLayout);
            pipelineCI.layout = _pipelineLayout;

            // Shader Stage

            VkSpecializationInfo specializationInfo;
            SpecializationConstant[] specDescs = description.Specializations;
            if (specDescs != null)
            {
                uint specDataSize = 0;
                foreach (SpecializationConstant spec in specDescs)
                {
                    specDataSize += VkFormats.GetSpecializationConstantSize(spec.Type);
                }
                byte* fullSpecData = stackalloc byte[(int)specDataSize];
                int specializationCount = specDescs.Length;
                VkSpecializationMapEntry* mapEntries = stackalloc VkSpecializationMapEntry[specializationCount];
                uint specOffset = 0;
                for (int i = 0; i < specializationCount; i++)
                {
                    ulong data = specDescs[i].Data;
                    byte* srcData = (byte*)&data;
                    uint dataSize = VkFormats.GetSpecializationConstantSize(specDescs[i].Type);
                    Unsafe.CopyBlock(fullSpecData + specOffset, srcData, dataSize);
                    mapEntries[i].constantID = specDescs[i].ID;
                    mapEntries[i].offset = specOffset;
                    mapEntries[i].size = (UIntPtr)dataSize;
                    specOffset += dataSize;
                }
                specializationInfo.dataSize = (UIntPtr)specDataSize;
                specializationInfo.pData = fullSpecData;
                specializationInfo.mapEntryCount = (uint)specializationCount;
                specializationInfo.pMapEntries = mapEntries;
            }

            Shader shader = description.ComputeShader;
            VkShader vkShader = Util.AssertSubtype<Shader, VkShader>(shader);
            VkPipelineShaderStageCreateInfo stageCI = VkPipelineShaderStageCreateInfo.New();
            stageCI.module = vkShader.ShaderModule;
            stageCI.stage = VkFormats.VdToVkShaderStages(shader.Stage);
            stageCI.pName = CommonStrings.main; // Meh
            stageCI.pSpecializationInfo = &specializationInfo;
            pipelineCI.stage = stageCI;

            VkResult result = vkCreateComputePipelines(
                _gd.Device,
                VkPipelineCache.Null,
                1,
                ref pipelineCI,
                null,
                out _devicePipeline);
            CheckResult(result);

            ResourceSetCount = (uint)description.ResourceLayouts.Length;
            DynamicOffsetsCount = 0;
            foreach (VkResourceLayout layout in description.ResourceLayouts)
            {
                DynamicOffsetsCount += layout.DynamicBufferCount;
            }
        }
*/
        //public override string Name
        //{
        //    get => _name;
        //    set
        //    {
        //        _name = value;
        //        _gd.SetResourceName(this, value);
        //    }
        //}

    public:
        virtual ~VulkanPipelineBase();

        const VkPipeline& GetHandle() const {return _devicePipeline;}
        const VkPipelineLayout& GetLayout() const { return _pipelineLayout; }
        std::uint32_t GetResourceSetCount() const { return resourceSetCount; }
        std::uint32_t GetDynamicOffsetCount() const {return dynamicOffsetsCount;}
    
    };

    class VulkanComputePipeline : public VulkanPipelineBase{

        VulkanComputePipeline(
            const sp<GraphicsDevice>& dev
        ) : VulkanPipelineBase(dev){}


    public:
        ~VulkanComputePipeline();

        static sp<Pipeline> Make(
            const sp<VulkanDevice>& dev,
            const ComputePipelineDescription& desc
        );

        bool IsComputePipeline() const override {return true;}

    };

    
    class VulkanGraphicsPipeline : public VulkanPipelineBase{


        VkRenderPass _renderPass;

        
        bool scissorTestEnabled;

        VulkanGraphicsPipeline(
            const sp<GraphicsDevice>& dev
        ) : VulkanPipelineBase(dev){}


    public:
        ~VulkanGraphicsPipeline();

        static sp<Pipeline> Make(
            const sp<VulkanDevice>& dev,
            const GraphicsPipelineDescription& desc
        );

        bool IsComputePipeline() const override {return false;}

    };
} // namespace Veldrid


