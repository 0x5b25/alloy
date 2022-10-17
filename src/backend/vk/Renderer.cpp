#include "Renderer.hpp"

#include <cassert>

RenderPipeline::Settings RenderPipeline::Settings::MakeDefault(
	std::shared_ptr<vkb::ShaderModule>& vertexShader,
	std::shared_ptr<vkb::ShaderModule>& fragmentShader)
{
    RenderPipeline::Settings settings{};

	settings.shaderStages = { vertexShader, fragmentShader };
    
    //Pipeline per vertex data layout
    //auto bindingDescription = Vertex::BindingDescription();
    //auto attributeDescriptions = Vertex::AttributeDescriptions();
    //
    //VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    //vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    //vertexInputInfo.vertexBindingDescriptionCount = 1;
    //vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    //vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    //vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    //Input render element(triangles) layout
    //VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    //inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    //inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    //inputAssembly.primitiveRestartEnable = VK_FALSE;

    //And viewport
    //VkViewport viewport{};
    //viewport.x = 0.0f;
    //viewport.y = 0.0f;
    //viewport.width = (float)swapChain->Extent().width;
    //viewport.height = (float)swapChain->Extent().height;
    //viewport.minDepth = 0.0f;
    //viewport.maxDepth = 1.0f;
    //VkRect2D scissor{};
    //scissor.offset = { 0, 0 };
    //scissor.extent = swapChain->Extent();
    //VkPipelineViewportStateCreateInfo viewportState{};
    //viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    //viewportState.viewportCount = 1;
    //viewportState.pViewports = &viewport;
    //viewportState.scissorCount = 1;
    //viewportState.pScissors = &scissor;

    //Rasterizer settings
    settings.rasterizationState.depthClampEnable = false;
    settings.rasterizationState.rasterizerDiscardEnable = false;
    settings.rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    settings.rasterizationState.lineWidth = 1.0f;
    settings.rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    settings.rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    settings.rasterizationState.depthBiasEnable = VK_FALSE;
    settings.rasterizationState.depthBiasConstantFactor = 0.0f; // Optional
    settings.rasterizationState.depthBiasClamp = 0.0f; // Optional
    settings.rasterizationState.depthBiasSlopeFactor = 0.0f; // Optional

    //Multisampling
    
    settings.multisampleState.sampleShadingEnable = VK_FALSE;
    settings.multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    settings.multisampleState.minSampleShading = 1.0f; // Optional
    //settings.multisampleState.sampleMask = {0,0}; // Optional
    settings.multisampleState.alphaToCoverageEnable = VK_FALSE; // Optional
    settings.multisampleState.alphaToOneEnable = VK_FALSE; // Optional

    //Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    settings.colorBlendState.logicOpEnable = VK_FALSE;
    settings.colorBlendState.logicOp = VK_LOGIC_OP_COPY; // Optional
    settings.colorBlendState.attachments = { colorBlendAttachment };
    settings.colorBlendState.blendConstants[0] = 0.0f; // Optional
    settings.colorBlendState.blendConstants[1] = 0.0f; // Optional
    settings.colorBlendState.blendConstants[2] = 0.0f; // Optional
    settings.colorBlendState.blendConstants[3] = 0.0f; // Optional
    
	return settings;
}

RenderPipeline::RenderPipeline(
    std::shared_ptr<RenderPass>& rndPass,
    uint32_t subpassIdx,
    const Settings& settings
) :
    _renderPass(rndPass),
    _subpassIdx(subpassIdx),
    _settings(settings)
{
    assert(rndPass != nullptr);

    //Shader stages
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    for(auto& sh : settings.shaderStages)
    {
        shaderStages.push_back(sh->ShaderStageCreateInfo());
    }

    //Pipeline per vertex data layout
    auto bindingDescription = Vertex::BindingDescription();
    auto attributeDescriptions = Vertex::AttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    //Input render element(triangles) layout
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    //Pipeline layout
    auto dslCnt = settings.layout.setLayouts.size();
    _dsl.reserve(dslCnt);
    for(size_t i = 0; i < dslCnt; i++)
    {
        _dsl.push_back(VK_NULL_HANDLE);
        VkDescriptorSetLayoutCreateInfo dslCI{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        dslCI.flags = settings.layout.setLayouts[i].flags;
        dslCI.bindingCount = settings.layout.setLayouts[i].bindings.size();
        dslCI.pBindings = settings.layout.setLayouts[i].bindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(
            rndPass->_dev->Handle(),&dslCI,nullptr,&_dsl.back()
        ));

    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = _dsl.size(); // Optional
    pipelineLayoutInfo.pSetLayouts = _dsl.data(); // Optional
    pipelineLayoutInfo.pushConstantRangeCount = settings.layout.pushConstantRanges.size(); // Optional
    pipelineLayoutInfo.pPushConstantRanges = settings.layout.pushConstantRanges.data(); // Optional

    VK_CHECK(vkCreatePipelineLayout(
        rndPass->_dev->Handle(),
        &pipelineLayoutInfo, nullptr, &_pipelineLayout));

    //Rasterizer settings
    VkPipelineRasterizationStateCreateInfo rasterizer
		= settings.rasterizationState;    
    

    //Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling
        = settings.multisampleState;

    //Depth stencil states
    VkPipelineDepthStencilStateCreateInfo depthStencil
        = settings.depthStencilState;

    //Color blending
    VkPipelineColorBlendStateCreateInfo colorBlending
        = settings.colorBlendState;

    //Dynamic states
    VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
		//VK_DYNAMIC_STATE_LINE_WIDTH
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    //Graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = nullptr;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState; // Optional
    pipelineInfo.layout = _pipelineLayout;
    pipelineInfo.renderPass = rndPass->Handle();
    pipelineInfo.subpass = subpassIdx;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    VK_CHECK(vkCreateGraphicsPipelines(
        _renderPass->_dev->Handle(), VK_NULL_HANDLE, 1, 
        &pipelineInfo, nullptr, &_pipeline));

}

RenderPipeline::~RenderPipeline()
{

    vkDestroyPipeline(_renderPass->_dev->Handle(),
        _pipeline, nullptr);

    vkDestroyPipelineLayout(_renderPass->_dev->Handle(),
        _pipelineLayout, nullptr);

    for(auto& dsl : _dsl)
    {

        vkDestroyDescriptorSetLayout(_renderPass->_dev->Handle(),
            dsl, nullptr);
    }

}

RenderPass::~RenderPass()
{
    vkDestroyRenderPass(_dev->Handle(), _rndPass, nullptr);
}

std::shared_ptr<RenderPass> RenderPass::Make(
    std::shared_ptr<Device>& device,
    const std::vector<SubPassDescription>& subpasses,
    const std::vector<VkAttachmentDescription>& attachments,
    const std::vector<VkSubpassDependency>& dependencies
)
{
    auto pass = std::make_shared<RenderPass>();

    pass->_dev = device;

    std::vector<VkSubpassDescription> _subpasses;
    for(auto& s : subpasses)
    {
        _subpasses.emplace_back();
        _subpasses.back().pipelineBindPoint = s.pipelineBindPoint;
        _subpasses.back().flags = 0;
        _subpasses.back().inputAttachmentCount
    		= s.inputAttachments.size();
        _subpasses.back().pInputAttachments
            = s.inputAttachments.data();

        _subpasses.back().colorAttachmentCount
            = s.colorAttachments.size();
        _subpasses.back().pColorAttachments
            = s.colorAttachments.data();

        if(s.resolveAttachments.empty())
        {
            _subpasses.back().pResolveAttachments = VK_NULL_HANDLE;
        }else
        {
            _subpasses.back().pResolveAttachments =
                s.resolveAttachments.data();
        }

        if (!s.depthStencilAttachment.has_value()) {
            _subpasses.back().pDepthStencilAttachment = VK_NULL_HANDLE;
        }
    	else {
            _subpasses.back().pDepthStencilAttachment
                = &s.depthStencilAttachment.value();
        }
        _subpasses.back().preserveAttachmentCount
            = s.preserveAttachments.size();
        _subpasses.back().pPreserveAttachments
            = s.preserveAttachments.data();
    }
    

    //Render passes
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = attachments.size();
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = _subpasses.size();
    renderPassInfo.pSubpasses = _subpasses.data();
    renderPassInfo.dependencyCount = dependencies.size();
    renderPassInfo.pDependencies = dependencies.data();
    VK_CHECK(vkCreateRenderPass(
        device->Handle(), &renderPassInfo, 
        nullptr, &pass->_rndPass));

    return pass;
}
