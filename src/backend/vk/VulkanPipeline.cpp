#include "VulkanPipeline.hpp"

#include "veldrid/Helpers.hpp"

#include <vector>
#include <set>

#include "VkTypeCvt.hpp"
#include "VkCommon.hpp"
#include "VulkanDevice.hpp"
#include "VulkanShader.hpp"
#include "VulkanBindableResource.hpp"

namespace Veldrid{

    VkRenderPass CreateFakeRenderPassForCompat(
        VulkanDevice* dev,
        const OutputDescription& outputDesc,
        VkSampleCountFlagBits sampleCnt
    ){

        bool hasColorAttachment = !outputDesc.colorAttachment.empty();
        bool hasDepthAttachment = outputDesc.depthAttachment.has_value();

        VkRenderPassCreateInfo renderPassCI{};

        std::vector<VkAttachmentDescription> attachments{};
        unsigned colorAttachmentCount = outputDesc.colorAttachment.size();


        std::vector<VkAttachmentReference> colorAttachmentRefs{colorAttachmentCount};

        for (unsigned i = 0; i < outputDesc.colorAttachment.size(); i++)
        {
            VkAttachmentDescription colorAttachmentDesc{};
            colorAttachmentDesc.format = VdToVkPixelFormat(outputDesc.colorAttachment[i].format);
            colorAttachmentDesc.samples = sampleCnt;
            colorAttachmentDesc.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachmentDesc.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachmentDesc.stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachmentDesc.stencilStoreOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachmentDesc.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachmentDesc.finalLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            attachments.push_back(colorAttachmentDesc);

            colorAttachmentRefs[i].attachment = i;
            colorAttachmentRefs[i].layout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        VkAttachmentReference depthAttachmentRef{};
        if (hasDepthAttachment)
        {
            VkAttachmentDescription depthAttachmentDesc{};
            PixelFormat depthFormat = outputDesc.depthAttachment.value().format;
            bool hasStencil = Helpers::FormatHelpers::IsStencilFormat(depthFormat);
            depthAttachmentDesc.format = VdToVkPixelFormat(depthFormat, true);
            depthAttachmentDesc.samples = sampleCnt;
            depthAttachmentDesc.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachmentDesc.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachmentDesc.stencilLoadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachmentDesc.stencilStoreOp = hasStencil 
                ? VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE 
                : VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachmentDesc.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachmentDesc.finalLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachments.push_back(depthAttachmentDesc);

            depthAttachmentRef.attachment = colorAttachmentCount;
            depthAttachmentRef.layout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS;
        if(hasColorAttachment){
            subpass.colorAttachmentCount = colorAttachmentCount;
            subpass.pColorAttachments = colorAttachmentRefs.data();
        }
        
        if (hasDepthAttachment)
        {
            subpass.pDepthStencilAttachment = &depthAttachmentRef;
        }

        VkSubpassDependency subpassDependency{};
        subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpassDependency.srcStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependency.dstStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependency.dstAccessMask 
            = VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT 
            | VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        renderPassCI.attachmentCount = attachments.size();
        renderPassCI.pAttachments = attachments.data();
        renderPassCI.subpassCount = 1;
        renderPassCI.pSubpasses = &subpass;
        renderPassCI.dependencyCount = 1;
        renderPassCI.pDependencies = &subpassDependency;

        VkRenderPass renderPass;
        VK_CHECK(vkCreateRenderPass(dev->LogicalDev(), &renderPassCI, nullptr, &renderPass));
        return renderPass;
    }


    VulkanPipelineBase::~VulkanPipelineBase() {
        auto vkDev = _Dev();
        vkDestroyPipelineLayout(vkDev->LogicalDev(), _pipelineLayout, nullptr);
        vkDestroyPipeline(vkDev->LogicalDev(), _devicePipeline, nullptr);
    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline(){

        auto vkDev = _Dev();
        if (!IsComputePipeline())
        {
            vkDestroyRenderPass(vkDev->LogicalDev(), _renderPass, nullptr);
        }
    }


    sp<Pipeline> VulkanGraphicsPipeline::Make(
        const sp<VulkanDevice>& dev,
        const GraphicsPipelineDescription& desc
    )
    {

        VkGraphicsPipelineCreateInfo pipelineCI{};
        pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        // Blend State
        VkPipelineColorBlendStateCreateInfo blendStateCI{};
        blendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        auto attachmentsCount = desc.blendState.attachments.size();
        std::vector<VkPipelineColorBlendAttachmentState> attachments(attachmentsCount);
        for (int i = 0; i < attachmentsCount; i++)
        {
            auto vdDesc = desc.blendState.attachments[i];
            auto& attachmentState = attachments[i];
            attachmentState.srcColorBlendFactor = VdToVkBlendFactor(vdDesc->sourceAlphaFactor);
            attachmentState.dstColorBlendFactor = VdToVkBlendFactor(vdDesc->destinationColorFactor);
            attachmentState.colorBlendOp = VdToVkBlendOp(vdDesc->colorFunction);
            attachmentState.srcAlphaBlendFactor = VdToVkBlendFactor(vdDesc->sourceAlphaFactor);
            attachmentState.dstAlphaBlendFactor = VdToVkBlendFactor(vdDesc->destinationAlphaFactor);
            attachmentState.alphaBlendOp = VdToVkBlendOp(vdDesc->alphaFunction);
            attachmentState.colorWriteMask = VdToVkColorWriteMask(vdDesc->colorWriteMask);
            attachmentState.blendEnable = vdDesc->blendEnabled;
        }

        blendStateCI.attachmentCount = attachmentsCount;
        blendStateCI.pAttachments = attachments.data();
        auto& blendFactor = desc.blendState.blendConstant;
        blendStateCI.blendConstants[0] = blendFactor.r;
        blendStateCI.blendConstants[1] = blendFactor.g;
        blendStateCI.blendConstants[2] = blendFactor.b;
        blendStateCI.blendConstants[3] = blendFactor.a;

        pipelineCI.pColorBlendState = &blendStateCI;

        // Rasterizer State
        auto& rsDesc = desc.rasterizerState;
        VkPipelineRasterizationStateCreateInfo rsCI{};
        rsCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rsCI.cullMode = VdToVkCullMode(rsDesc.cullMode);
        rsCI.polygonMode = VdToVkPolygonMode(rsDesc.fillMode);
        rsCI.depthClampEnable = !rsDesc.depthClipEnabled;
        rsCI.frontFace = rsDesc.FrontFace == RasterizerStateDescription::FrontFace::Clockwise
            ? VkFrontFace::VK_FRONT_FACE_CLOCKWISE 
            : VkFrontFace::VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rsCI.lineWidth = 1.f;

        pipelineCI.pRasterizationState = &rsCI;

        // Dynamic State
        VkPipelineDynamicStateCreateInfo dynamicStateCI{};
        dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        VkDynamicState dynamicStates[2];
        dynamicStates[0] = VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT;
        dynamicStates[1] = VkDynamicState::VK_DYNAMIC_STATE_SCISSOR;
        dynamicStateCI.dynamicStateCount = 2;
        dynamicStateCI.pDynamicStates = dynamicStates;

        pipelineCI.pDynamicState = &dynamicStateCI;

        // Depth Stencil State
        auto& vdDssDesc = desc.depthStencilState;
        VkPipelineDepthStencilStateCreateInfo dssCI{};
        dssCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        dssCI.depthWriteEnable = vdDssDesc.depthWriteEnabled;
        dssCI.depthTestEnable = vdDssDesc.depthTestEnabled;
        dssCI.depthCompareOp = VdToVkCompareOp(vdDssDesc.depthComparison);
        dssCI.stencilTestEnable = vdDssDesc.stencilTestEnabled;

        dssCI.front.failOp = VdToVkStencilOp(vdDssDesc.stencilFront.fail);
        dssCI.front.passOp = VdToVkStencilOp(vdDssDesc.stencilFront.pass);
        dssCI.front.depthFailOp = VdToVkStencilOp(vdDssDesc.stencilFront.depthFail);
        dssCI.front.compareOp = VdToVkCompareOp(vdDssDesc.stencilFront.comparison);
        dssCI.front.compareMask = vdDssDesc.stencilReadMask;
        dssCI.front.writeMask = vdDssDesc.stencilWriteMask;
        dssCI.front.reference = vdDssDesc.stencilReference;

        dssCI.back.failOp = VdToVkStencilOp(vdDssDesc.stencilBack.fail);
        dssCI.back.passOp = VdToVkStencilOp(vdDssDesc.stencilBack.pass);
        dssCI.back.depthFailOp = VdToVkStencilOp(vdDssDesc.stencilBack.depthFail);
        dssCI.back.compareOp = VdToVkCompareOp(vdDssDesc.stencilBack.comparison);
        dssCI.back.compareMask = vdDssDesc.stencilReadMask;
        dssCI.back.writeMask = vdDssDesc.stencilWriteMask;
        dssCI.back.reference = vdDssDesc.stencilReference;

        pipelineCI.pDepthStencilState = &dssCI;

        // Multisample
        VkPipelineMultisampleStateCreateInfo multisampleCI{};
        multisampleCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        VkSampleCountFlagBits vkSampleCount = VdToVkSampleCount(desc.outputs.sampleCount);
        multisampleCI.rasterizationSamples = vkSampleCount;
        multisampleCI.alphaToCoverageEnable = desc.blendState.alphaToCoverageEnabled;

        pipelineCI.pMultisampleState = &multisampleCI;

        // Input Assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{};
        inputAssemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyCI.topology = VdToVkPrimitiveTopology(desc.primitiveTopology);

        pipelineCI.pInputAssemblyState = &inputAssemblyCI;

        // Vertex Input State
        VkPipelineVertexInputStateCreateInfo vertexInputCI{};
        vertexInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        auto& inputDescriptions = desc.shaderSet.vertexLayouts;
        auto bindingCount = inputDescriptions.size();
        unsigned attributeCount = 0;
        for (int i = 0; i < inputDescriptions.size(); i++)
        {
            attributeCount += inputDescriptions[i]->elements.size();
        }
        std::vector<VkVertexInputBindingDescription> bindingDescs(bindingCount);
        std::vector<VkVertexInputAttributeDescription> attributeDescs(attributeCount);

        int targetIndex = 0;
        int targetLocation = 0;
        for (int binding = 0; binding < inputDescriptions.size(); binding++)
        {
            auto* inputDesc = inputDescriptions[binding];
            bindingDescs[binding].binding = binding;
            bindingDescs[binding].inputRate = (inputDesc->instanceStepRate != 0) 
                ? VkVertexInputRate::VK_VERTEX_INPUT_RATE_INSTANCE 
                : VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;
            bindingDescs[binding].stride = inputDesc->stride;
            
            unsigned currentOffset = 0;
            for (int location = 0; location < inputDesc->elements.size(); location++)
            {
                auto* inputElement = inputDesc->elements[location];

                attributeDescs[targetIndex].format = VdToVkShaderDataType(inputElement->format);
                attributeDescs[targetIndex].binding = binding;
                attributeDescs[targetIndex].location = targetLocation + location;
                attributeDescs[targetIndex].offset = inputElement->offset != 0 
                    ? inputElement->offset 
                    : currentOffset;
                
                targetIndex += 1;
                currentOffset += Helpers::FormatHelpers::GetSizeInBytes(inputElement->format);
            }

            targetLocation += inputDesc->elements.size();
        }

        vertexInputCI.vertexBindingDescriptionCount = bindingCount;
        vertexInputCI.pVertexBindingDescriptions = bindingDescs.data();
        vertexInputCI.vertexAttributeDescriptionCount = attributeCount;
        vertexInputCI.pVertexAttributeDescriptions = attributeDescs.data();

        pipelineCI.pVertexInputState = &vertexInputCI;

        // Shader Stage

        VkSpecializationInfo specializationInfo{};
        auto& specDescs = desc.shaderSet.specializations;
        if (!specDescs.empty())
        {
            unsigned specDataSize = 0;
            for (auto* spec : specDescs) {
                specDataSize += GetSpecializationConstantSize(spec->type);
            }
            std::vector<std::uint8_t> fullSpecData(specDataSize);
            int specializationCount = specDescs.size();
            std::vector<VkSpecializationMapEntry> mapEntries(specializationCount);
            unsigned specOffset = 0;
            for (int i = 0; i < specializationCount; i++)
            {
                auto data = specDescs[i]->data;
                auto srcData = (byte*)&data;
                auto dataSize = GetSpecializationConstantSize(specDescs[i]->type);
                //Unsafe.CopyBlock(fullSpecData + specOffset, srcData, dataSize);
                memcpy(fullSpecData.data() + specOffset, srcData, dataSize);
                mapEntries[i].constantID = specDescs[i]->id;
                mapEntries[i].offset = specOffset;
                mapEntries[i].size = dataSize;
                specOffset += dataSize;
            }
            specializationInfo.dataSize = specDataSize;
            specializationInfo.pData = fullSpecData.data();
            specializationInfo.mapEntryCount = specializationCount;
            specializationInfo.pMapEntries = mapEntries.data();
        }

        auto& shaders = desc.shaderSet.shaders;
        std::vector<VkPipelineShaderStageCreateInfo> stageCIs(shaders.size());
        for (unsigned i = 0; i < shaders.size(); i++){
            auto& shader = shaders[i];
            auto vkShader = reinterpret_cast<VulkanShader*>(shader.get());
            auto& stageCI = stageCIs[i];
            stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageCI.module = vkShader->GetHandle();
            stageCI.stage = VdToVkShaderStages(shader->GetDesc().stage);
            // stageCI.pName = CommonStrings.main; // Meh
            stageCI.pName = shader->GetDesc().entryPoint.c_str(); // TODO: DONT ALLOCATE HERE
            stageCI.pSpecializationInfo = &specializationInfo;
        }

        pipelineCI.stageCount = stageCIs.size();
        pipelineCI.pStages = stageCIs.data();

        // ViewportState
        VkPipelineViewportStateCreateInfo viewportStateCI{};
        viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateCI.viewportCount = 1;
        viewportStateCI.scissorCount = 1;

        pipelineCI.pViewportState = &viewportStateCI;

        // Pipeline Layout
        auto& resourceLayouts = desc.resourceLayouts;
        VkPipelineLayoutCreateInfo pipelineLayoutCI {};
        pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCI.setLayoutCount = resourceLayouts.size();
        std::vector<VkDescriptorSetLayout> dsls (resourceLayouts.size());
        for (int i = 0; i < resourceLayouts.size(); i++)
        {
            dsls[i] = PtrCast<VulkanResourceLayout>(resourceLayouts[i].get())->GetHandle();
        }
        pipelineLayoutCI.pSetLayouts = dsls.data();

        VkPipelineLayout pipelineLayout;
        VK_CHECK(vkCreatePipelineLayout(dev->LogicalDev(), &pipelineLayoutCI, nullptr, &pipelineLayout));
        pipelineCI.layout = pipelineLayout;

        // Create fake RenderPass for compatibility.

        auto& outputDesc = desc.outputs;
        auto compatRenderPass = CreateFakeRenderPassForCompat(dev.get(), outputDesc, vkSampleCount);
        
        pipelineCI.renderPass = compatRenderPass;

        VkPipeline devicePipeline;
        VK_CHECK(vkCreateGraphicsPipelines(dev->LogicalDev(), VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &devicePipeline));

        std::uint32_t resourceSetCount = desc.resourceLayouts.size();
        std::uint32_t dynamicOffsetsCount = 0;
        for(auto& layout : desc.resourceLayouts)
        {
            auto vkLayout = PtrCast<VulkanResourceLayout>(layout.get());
            dynamicOffsetsCount += vkLayout->GetDynamicBufferCount();
        }

        auto rawPipe = new VulkanGraphicsPipeline(dev);
        rawPipe->_devicePipeline = devicePipeline;
        rawPipe->_pipelineLayout = pipelineLayout;
        rawPipe->_renderPass = compatRenderPass;
        rawPipe->scissorTestEnabled = rsDesc.scissorTestEnabled;
        rawPipe->resourceSetCount = resourceSetCount;
        rawPipe->dynamicOffsetsCount = dynamicOffsetsCount;

        return sp(rawPipe);
    }



    sp<Pipeline> VulkanComputePipeline::Make(
        const sp<VulkanDevice>& dev,
        const ComputePipelineDescription& desc
    ){
        VkComputePipelineCreateInfo pipelineCI {};

        // Pipeline Layout
        auto& resourceLayouts = desc.resourceLayouts;
        VkPipelineLayoutCreateInfo pipelineLayoutCI{};
        pipelineLayoutCI.setLayoutCount = resourceLayouts.size();
        std::vector<VkDescriptorSetLayout> dsls{resourceLayouts.size()};
        for (int i = 0; i < resourceLayouts.size(); i++)
        {
            dsls[i] = PtrCast<VulkanResourceLayout>(resourceLayouts[i].get())->GetHandle();
        }
        pipelineLayoutCI.pSetLayouts = dsls.data();

        VkPipelineLayout pipelineLayout;
        VK_CHECK(vkCreatePipelineLayout(dev->LogicalDev(), &pipelineLayoutCI, nullptr, &pipelineLayout));

        // Shader Stage

        VkSpecializationInfo specializationInfo;
        auto& specDescs = desc.specializations;
        if (!specDescs.empty())
        {
            unsigned specDataSize = 0;
            for(auto& spec : specDescs)
            {
                specDataSize += GetSpecializationConstantSize(spec->type);
            }
            std::vector<std::uint8_t> fullSpecData(specDataSize);
            unsigned specializationCount = specDescs.size();
            std::vector<VkSpecializationMapEntry> mapEntries(specializationCount);
            unsigned specOffset = 0;
            for (int i = 0; i < specializationCount; i++)
            {
                auto data = specDescs[i]->data;
                byte* srcData = (byte*)&data;
                unsigned dataSize = GetSpecializationConstantSize(specDescs[i]->type);
                memcpy(fullSpecData.data() + specOffset, srcData, dataSize);
                mapEntries[i].constantID = specDescs[i]->id;
                mapEntries[i].offset = specOffset;
                mapEntries[i].size = dataSize;
                specOffset += dataSize;
            }
            specializationInfo.dataSize = specDataSize;
            specializationInfo.pData = fullSpecData.data();
            specializationInfo.mapEntryCount = specializationCount;
            specializationInfo.pMapEntries = mapEntries.data();
        }

        auto& shader = desc.computeShader;
        auto* vkShader = PtrCast<VulkanShader>(shader.get());
        VkPipelineShaderStageCreateInfo stageCI{};
        stageCI.module = vkShader->GetHandle();
        stageCI.stage = VdToVkShaderStages(shader->GetDesc().stage);
        stageCI.pName = "main"; // Meh
        stageCI.pSpecializationInfo = &specializationInfo;
        pipelineCI.stage = stageCI;

        VkPipeline devicePipeline;
        VK_CHECK(vkCreateComputePipelines(
            dev->LogicalDev(),VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &devicePipeline
        ));

        
        std::uint32_t resourceSetCount = desc.resourceLayouts.size();
        std::uint32_t dynamicOffsetsCount = 0;
        for(auto& layout : desc.resourceLayouts)
        {
            auto vkLayout = PtrCast<VulkanResourceLayout>(layout.get());
            dynamicOffsetsCount += vkLayout->GetDynamicBufferCount();
        }

        auto rawPipe = new VulkanComputePipeline(dev);
        rawPipe->_devicePipeline = devicePipeline;
        rawPipe->_pipelineLayout = pipelineLayout;
        rawPipe->resourceSetCount = resourceSetCount;
        rawPipe->dynamicOffsetsCount = dynamicOffsetsCount;

        return sp(rawPipe);
    }

}
