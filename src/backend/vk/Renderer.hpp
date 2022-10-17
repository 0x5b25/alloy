#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <glm/glm.hpp>
#include "common/macros.h"

#include "Device.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "common/helpers.h"


class RenderPass;


struct Vertex
{
    glm::vec2 Position;
    glm::vec2 UV;
    glm::vec3 Color;

    static VkVertexInputBindingDescription BindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};

        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::vector<VkVertexInputAttributeDescription> AttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{ 3 };

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, Position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, UV);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, Color);

        return attributeDescriptions;
    }
};



class RenderPipeline
{

	DISALLOW_COPY_AND_ASSIGN(RenderPipeline);
public:
    struct Settings
    {
        VkPipelineCreateFlags                            flags;
        std::vector<std::shared_ptr<vkb::ShaderModule>>  shaderStages;
        //Hard coded
        //const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
        //const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
        //No tessellation for now
        //const VkPipelineTessellationStateCreateInfo* pTessellationState;
        //Dynamic states
    	//const VkPipelineViewportStateCreateInfo* pViewportState;

        //const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
        struct RasterizationState
        {
            //VkPipelineRasterizationStateCreateFlags    flags; //reserved
            bool                                       depthClampEnable;
            bool                                       rasterizerDiscardEnable;
            VkPolygonMode                              polygonMode;
            VkCullModeFlags                            cullMode;
            VkFrontFace                                frontFace;
            bool                                       depthBiasEnable;
            float                                      depthBiasConstantFactor;
            float                                      depthBiasClamp;
            float                                      depthBiasSlopeFactor;
            float                                      lineWidth;

            operator VkPipelineRasterizationStateCreateInfo() const
            {
                return VkPipelineRasterizationStateCreateInfo{
                    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                    nullptr, 0,
                    vkb::to_bool32(depthClampEnable),  vkb::to_bool32(rasterizerDiscardEnable),
                	polygonMode, cullMode, frontFace,  vkb::to_bool32(depthBiasEnable),
                    depthBiasConstantFactor,depthBiasClamp,depthBiasSlopeFactor,lineWidth
                };
            }

        } rasterizationState;

        //const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
        struct MultisampleState
        {
            //VkPipelineMultisampleStateCreateFlags    flags; //reserved
            VkSampleCountFlagBits  rasterizationSamples;
            float                  minSampleShading;
        	VkSampleMask           sampleMask[2];//Not used
            bool                   sampleShadingEnable;
            bool                   alphaToCoverageEnable;
            bool                   alphaToOneEnable;

            operator VkPipelineMultisampleStateCreateInfo() const
            {
                return VkPipelineMultisampleStateCreateInfo{
                    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr,
                    0,
                    rasterizationSamples,
                	vkb::to_bool32(sampleShadingEnable), minSampleShading,
                    sampleMask,
                    vkb::to_bool32(alphaToCoverageEnable),
                    vkb::to_bool32(alphaToOneEnable)
                };
            }
        } multisampleState;

        //const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
        struct DepthStencilState
        {
            bool                                  depthTestEnable;
            bool                                  depthWriteEnable;
            bool                                  depthBoundsTestEnable;
            bool                                  stencilTestEnable;
            VkCompareOp                               depthCompareOp;
            VkStencilOpState                          front;
            VkStencilOpState                          back;
            float                                     minDepthBounds;
            float                                     maxDepthBounds;

            operator VkPipelineDepthStencilStateCreateInfo() const
            {
                return VkPipelineDepthStencilStateCreateInfo{
                    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr, 0,
                    vkb::to_bool32(depthTestEnable),
                    vkb::to_bool32(depthWriteEnable),
                    depthCompareOp, vkb::to_bool32(depthBoundsTestEnable),
                    vkb::to_bool32(stencilTestEnable),front,back,
                    minDepthBounds, maxDepthBounds
                };
            }
        } depthStencilState;

    	//const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
        struct ColorBlendState
        {
            //VkPipelineColorBlendStateCreateFlags          flags;
            bool                                             logicOpEnable;
            VkLogicOp                                        logicOp;
            std::vector<VkPipelineColorBlendAttachmentState> attachments;
            float                                            blendConstants[4];

            operator VkPipelineColorBlendStateCreateInfo() const
            {
                return VkPipelineColorBlendStateCreateInfo{
                    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0,
                    vkb::to_bool32(logicOpEnable), logicOp, vkb::to_u32(attachments.size()), attachments.data(),
                    blendConstants[0],
                    blendConstants[1],
                    blendConstants[2],
                    blendConstants[3],

                };
            }
        } colorBlendState;

        
        //const VkPipelineDynamicStateCreateInfo* pDynamicState;
        //VkPipelineLayout                                 layout;
        struct PipelineLayout
        {
            struct DescriptorSetLayout
            {
                VkDescriptorSetLayoutCreateFlags       flags;
                std::vector<VkDescriptorSetLayoutBinding> bindings;
            };
            //VkStructureType                 sType;
           // const void* pNext;
            //VkPipelineLayoutCreateFlags     flags;
            std::vector<DescriptorSetLayout> setLayouts;
            std::vector<VkPushConstantRange> pushConstantRanges;
        }layout;
        //VkSubpassDescriptionFlags       flags; //Not useful for now
        //VkRenderPass                                     renderPass;
        //uint32_t                                         subpass;
        //VkPipeline                                       basePipelineHandle;
        //int32_t                                          basePipelineIndex;

        static Settings MakeDefault(
            std::shared_ptr<vkb::ShaderModule>& vertexShader,
            std::shared_ptr<vkb::ShaderModule>& fragmentShader
		);
    };

private:
    //std::shared_ptr<Device> _dev;
    std::shared_ptr<RenderPass> _renderPass;
    uint32_t _subpassIdx;
    VkPipeline _pipeline;
    std::vector<VkDescriptorSetLayout> _dsl;
    VkPipelineLayout _pipelineLayout;

    //Shader stages

    //Vertex data layout and input assembly are hard coded

    //Uniform layouts?

    //Viewport treated as dynamic state

    //Rasterization

    //Multisampling

    //Depth and stencil?

    //Color blending

    Settings _settings;
    
public:

    RenderPipeline(
        std::shared_ptr<RenderPass>& rndPass, 
        uint32_t subpassIdx,
        const Settings& settings
    );

    ~RenderPipeline();

    static std::shared_ptr<RenderPipeline> Make(
        std::shared_ptr<RenderPass>& rndPass,
        uint32_t subpassIdx,
        const Settings& settings
    )
    {
        return std::make_shared<RenderPipeline>(
			rndPass, subpassIdx, settings
        );
    }

    const VkPipeline& Handle() const { return _pipeline; }

};


//Contains stages and their dependencies
class RenderPass
{
    friend class RenderPipeline;

public:

    struct SubPassDescription
    {
        VkPipelineBindPoint                  pipelineBindPoint;
        std::vector<VkAttachmentReference>   inputAttachments;
        std::vector<VkAttachmentReference>   colorAttachments;
        std::vector<VkAttachmentReference>   resolveAttachments;
        std::optional<VkAttachmentReference> depthStencilAttachment;
        std::vector<uint32_t>                preserveAttachments;
    };

private:
	DISALLOW_COPY_AND_ASSIGN(RenderPass);

    std::shared_ptr<Device> _dev;

    VkRenderPass _rndPass;

public:

    RenderPass() = default;
    ~RenderPass();

	static std::shared_ptr<RenderPass> Make(
		std::shared_ptr<Device>& device,
        const std::vector<SubPassDescription>& subpasses,
        const std::vector<VkAttachmentDescription>& attachments,
        const std::vector<VkSubpassDependency>& dependencies 
		//const std::vector<std::shared_ptr<RenderStageDependency>>& stageDependencies
	);
    

    const VkRenderPass& Handle() const { return _rndPass; }
};

//Vertex buffers, Uniform buffers, Instance buffers,
//Textures...
struct RenderStageResource
{
	
};

class Renderer
{
	//DISALLOW_COPY_AND_ASSIGN(Renderer);

	std::shared_ptr<Device> _dev;

public:
};
