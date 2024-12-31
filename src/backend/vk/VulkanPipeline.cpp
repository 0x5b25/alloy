#include "VulkanPipeline.hpp"

#include "veldrid/common/Common.hpp"
#include "veldrid/Helpers.hpp"

#include <vector>
#include <set>
#include <cstring>

#include "VkTypeCvt.hpp"
#include "VkCommon.hpp"
#include "VulkanDevice.hpp"
#include "VulkanShader.hpp"
#include "VulkanBindableResource.hpp"


template <>
struct std::hash<Veldrid::VertexInputSemantic>
{
    std::size_t operator()(const Veldrid::VertexInputSemantic& k) const
    {
        return std::hash<uint32_t>()((uint32_t)k.name)
             ^ (std::hash<uint32_t>()(k.slot) << 1);
    }
};

namespace Veldrid{
class VkShaderRAII {
    VkDevice _dev;
    VkShaderModule _mod;
public:
    VkShaderRAII(VkDevice dev) : _dev(dev), _mod(VK_NULL_HANDLE) {}
    ~VkShaderRAII() { 
        if(_mod != VK_NULL_HANDLE)
        vkDestroyShaderModule(_dev, _mod, nullptr);
    }
    VkShaderModule* operator&() {return &_mod;}
    VkShaderModule operator*() {return _mod;}
    VkShaderModule Reset() { 
        auto res = _mod;  _mod = VK_NULL_HANDLE; return res;
    }
};

class VkPipelineLayoutRAII {
    VkDevice _dev;
    VkPipelineLayout _obj;
public:
    VkPipelineLayoutRAII(VkDevice dev) : _dev(dev), _obj(VK_NULL_HANDLE) {}
    ~VkPipelineLayoutRAII() { 
        if(_obj != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(_dev, _obj, nullptr);
    }
    VkPipelineLayout* operator&() {return &_obj;}
    VkPipelineLayout operator*() {return _obj;}
    VkPipelineLayout Reset() { 
        auto res = _obj;  _obj = VK_NULL_HANDLE; return res;
    }
};

    bool Str2Semantic(const char* str, VertexInputSemantic::Name& semantic){

        static const struct {
            const char* str;
            VertexInputSemantic::Name enumVal;
        } lut[] {
            {"BINORMAL",     VertexInputSemantic::Name::Binormal},
            {"BLENDINDICES", VertexInputSemantic::Name::BlendIndices},
            {"BLENDWEIGHT",  VertexInputSemantic::Name::BlendWeight},
            {"COLOR",        VertexInputSemantic::Name::Color},
            {"NORMAL",       VertexInputSemantic::Name::Normal},
            {"POSITION",     VertexInputSemantic::Name::Position},
            {"PSIZE",        VertexInputSemantic::Name::PointSize},
            {"TANGENT",      VertexInputSemantic::Name::Tangent},
            {"TEXCOORD",     VertexInputSemantic::Name::TextureCoordinate},
        };

        for(auto& entry : lut) {
            if(std::strcmp(entry.str, str) == 0) {
                semantic = entry.enumVal;
                return true;
            }
        }

        return false;
    }


    VkRenderPass CreateFakeRenderPassForCompat(
        VulkanDevice* dev,
        const OutputDescription& outputDesc,
        VkSampleCountFlagBits sampleCnt
    ){

        bool hasColorAttachment = !outputDesc.colorAttachment.empty();
        bool hasDepthAttachment = outputDesc.depthAttachment.has_value();

        VkRenderPassCreateInfo renderPassCI{};
        renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

        std::vector<VkAttachmentDescription> attachments{};
        unsigned colorAttachmentCount = outputDesc.colorAttachment.size();


        std::vector<VkAttachmentReference> colorAttachmentRefs{colorAttachmentCount};

        for (unsigned i = 0; i < outputDesc.colorAttachment.size(); i++)
        {
            VkAttachmentDescription colorAttachmentDesc{};
            colorAttachmentDesc.format = VK::priv::VdToVkPixelFormat(outputDesc.colorAttachment[i].format);
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
            depthAttachmentDesc.format = VK::priv::VdToVkPixelFormat(depthFormat, true);
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

    VulkanComputePipeline::~VulkanComputePipeline(){

    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline(){

        auto vkDev = _Dev();
        if (!IsComputePipeline())
        {
            vkDestroyRenderPass(vkDev->LogicalDev(), _renderPass, nullptr);
        }
    }

    void _CreateStandardPipeline(
        VkDevice dev,
        VkShaderModule vertShaderModule,
        VkShaderModule fragShaderModule,
        float width,
        float height,
        VkRenderPass compatRenderPass,

        VkPipelineLayout& outLayout,
        VkPipeline& outPipeline
    ) {
        //auto vertShaderCode = readFile("shaders/vert.spv");
        //auto fragShaderCode = readFile("shaders/frag.spv");
        //
        //VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        //VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = width;
        viewport.height = height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = {(unsigned)width, (unsigned)height};
        
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        // Dynamic State
        VkPipelineDynamicStateCreateInfo dynamicStateCI{};
        dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        VkDynamicState dynamicStates[2];
        dynamicStates[0] = VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT;
        dynamicStates[1] = VkDynamicState::VK_DYNAMIC_STATE_SCISSOR;
        dynamicStateCI.dynamicStateCount = 2;
        dynamicStateCI.pDynamicStates = dynamicStates;

        //pipelineCI.pDynamicState = &dynamicStateCI;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pushConstantRangeCount = 0;


        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(dev, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
        outLayout = pipelineLayout;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pDynamicState = &dynamicStateCI;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = compatRenderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        VkPipeline graphicsPipeline;
        if (vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        outPipeline = graphicsPipeline;

        return;
    }

    sp<Pipeline> VulkanGraphicsPipeline::Make(
        const sp<VulkanDevice>& dev,
        const GraphicsPipelineDescription& desc
    )
    {
        std::vector<sp<RefCntBase>> refCnts;

        VkGraphicsPipelineCreateInfo pipelineCI{};
        pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        //*************************************************
        //*************************************************
        //*************************************************
        // From Standard creation info ***Makes it work!***
        //*************************************************
        //*************************************************
        //*************************************************
        // Blend State
        VkPipelineColorBlendStateCreateInfo blendStateCI{};
        blendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        auto attachmentsCount = desc.blendState.attachments.size();
        std::vector<VkPipelineColorBlendAttachmentState> attachments(attachmentsCount);
        for (int i = 0; i < attachmentsCount; i++)
        {
            auto vdDesc = desc.blendState.attachments[i];
            auto& attachmentState = attachments[i];
            attachmentState.srcColorBlendFactor = VK::priv::VdToVkBlendFactor(vdDesc.sourceColorFactor);
            attachmentState.dstColorBlendFactor = VK::priv::VdToVkBlendFactor(vdDesc.destinationColorFactor);
            attachmentState.colorBlendOp = VK::priv::VdToVkBlendOp(vdDesc.colorFunction);
            attachmentState.srcAlphaBlendFactor = VK::priv::VdToVkBlendFactor(vdDesc.sourceAlphaFactor);
            attachmentState.dstAlphaBlendFactor = VK::priv::VdToVkBlendFactor(vdDesc.destinationAlphaFactor);
            attachmentState.alphaBlendOp = VK::priv::VdToVkBlendOp(vdDesc.alphaFunction);
            attachmentState.colorWriteMask = VK::priv::VdToVkColorWriteMask(vdDesc.colorWriteMask);
            attachmentState.blendEnable = vdDesc.blendEnabled;
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
        rsCI.cullMode = VK::priv::VdToVkCullMode(rsDesc.cullMode);
        rsCI.polygonMode = VK::priv::VdToVkPolygonMode(rsDesc.fillMode);

        //depthClampEnable controls whether to clamp the fragment’s depth values
        // as described in Depth Test. If the pipeline is not created with
        //VkPipelineRasterizationDepthClipStateCreateInfoEXT present then enabling
        //depth clamp will also disable clipping primitives to the z planes of
        //the frustrum as described in Primitive Clipping. Otherwise depth clipping
        //is controlled by the state set in VkPipelineRasterizationDepthClipStateCreateInfoEXT.

        rsCI.depthClampEnable = false;
        VkPipelineRasterizationDepthClipStateCreateInfoEXT rsDepthClipCI{};
        if(dev->GetVkFeatures().supportsDepthClip) {            

            //If the pNext chain of VkPipelineRasterizationStateCreateInfo includes
            // a VkPipelineRasterizationDepthClipStateCreateInfoEXT structure, then 
            //that structure controls whether depth clipping is enabled or disabled.

            // Provided by VK_EXT_depth_clip_enable
            //typedef struct VkPipelineRasterizationDepthClipStateCreateInfoEXT {
            //    VkStructureType                                        sType;
            //    const void*                                            pNext;
            //    VkPipelineRasterizationDepthClipStateCreateFlagsEXT    flags;
            //    VkBool32                                               depthClipEnable;
            //} VkPipelineRasterizationDepthClipStateCreateInfoEXT;
            rsDepthClipCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT;
            rsDepthClipCI.pNext = rsCI.pNext;
            rsCI.pNext = &rsDepthClipCI;
            rsDepthClipCI.depthClipEnable = rsDesc.depthClipEnabled;
        }

        rsCI.frontFace = rsDesc.frontFace == RasterizerStateDescription::FrontFace::Clockwise
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
        dssCI.depthCompareOp = VK::priv::VdToVkCompareOp(vdDssDesc.depthComparison);
        dssCI.stencilTestEnable = vdDssDesc.stencilTestEnabled;

        dssCI.front.failOp = VK::priv::VdToVkStencilOp(vdDssDesc.stencilFront.fail);
        dssCI.front.passOp = VK::priv::VdToVkStencilOp(vdDssDesc.stencilFront.pass);
        dssCI.front.depthFailOp = VK::priv::VdToVkStencilOp(vdDssDesc.stencilFront.depthFail);
        dssCI.front.compareOp = VK::priv::VdToVkCompareOp(vdDssDesc.stencilFront.comparison);
        dssCI.front.compareMask = vdDssDesc.stencilReadMask;
        dssCI.front.writeMask = vdDssDesc.stencilWriteMask;
        dssCI.front.reference = vdDssDesc.stencilReference;

        dssCI.back.failOp = VK::priv::VdToVkStencilOp(vdDssDesc.stencilBack.fail);
        dssCI.back.passOp = VK::priv::VdToVkStencilOp(vdDssDesc.stencilBack.pass);
        dssCI.back.depthFailOp = VK::priv::VdToVkStencilOp(vdDssDesc.stencilBack.depthFail);
        dssCI.back.compareOp = VK::priv::VdToVkCompareOp(vdDssDesc.stencilBack.comparison);
        dssCI.back.compareMask = vdDssDesc.stencilReadMask;
        dssCI.back.writeMask = vdDssDesc.stencilWriteMask;
        dssCI.back.reference = vdDssDesc.stencilReference;

        pipelineCI.pDepthStencilState = &dssCI;

        // Multisample
        VkPipelineMultisampleStateCreateInfo multisampleCI{};
        multisampleCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        VkSampleCountFlagBits vkSampleCount = VK::priv::VdToVkSampleCount(desc.outputs.sampleCount);
        multisampleCI.rasterizationSamples = vkSampleCount;
        multisampleCI.alphaToCoverageEnable = desc.blendState.alphaToCoverageEnabled;
        pipelineCI.pMultisampleState = &multisampleCI;

        // Input Assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{};
        inputAssemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyCI.topology = VK::priv::VdToVkPrimitiveTopology(desc.primitiveTopology);
        inputAssemblyCI.primitiveRestartEnable = VK_FALSE;
        pipelineCI.pInputAssemblyState = &inputAssemblyCI;

        // Vertex Input State
        VkPipelineVertexInputStateCreateInfo vertexInputCI{};
        vertexInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        auto& inputDescriptions = desc.shaderSet.vertexLayouts;
        auto bindingCount = inputDescriptions.size();
        unsigned attributeCount = 0;
        for (int i = 0; i < inputDescriptions.size(); i++)
        {
            attributeCount += inputDescriptions[i].elements.size();
        }
        std::vector<VkVertexInputBindingDescription> bindingDescs(bindingCount);
        std::vector<VkVertexInputAttributeDescription> attributeDescs(attributeCount);

        std::unordered_map<VertexInputSemantic, uint32_t> iaMappings;

        int targetIndex = 0;
        int targetLocation = 0;
        for (int binding = 0; binding < inputDescriptions.size(); binding++)
        {
            auto& inputDesc = inputDescriptions[binding];
            bindingDescs[binding].binding = binding;
            bindingDescs[binding].inputRate = (inputDesc.instanceStepRate != 0) 
                ? VkVertexInputRate::VK_VERTEX_INPUT_RATE_INSTANCE 
                : VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;
            bindingDescs[binding].stride = inputDesc.stride;
            
            unsigned currentOffset = 0;
            for (int location = 0; location < inputDesc.elements.size(); location++)
            {
                auto& inputElement = inputDesc.elements[location];
                auto thisLocation = targetLocation + location;

                iaMappings.insert({inputElement.semantic, thisLocation});

                attributeDescs[targetIndex].format = VK::priv::VdToVkShaderDataType(inputElement.format);
                attributeDescs[targetIndex].binding = binding;
                attributeDescs[targetIndex].location = thisLocation;
                attributeDescs[targetIndex].offset = inputElement.offset != 0 
                    ? inputElement.offset 
                    : currentOffset;
                
                targetIndex += 1;
                currentOffset += Helpers::FormatHelpers::GetSizeInBytes(inputElement.format);
            }

            targetLocation += inputDesc.elements.size();
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
            for (auto& spec : specDescs) {
                specDataSize += VK::priv::GetSpecializationConstantSize(spec.type);
            }
            std::vector<std::uint8_t> fullSpecData(specDataSize);
            int specializationCount = specDescs.size();
            std::vector<VkSpecializationMapEntry> mapEntries(specializationCount);
            unsigned specOffset = 0;
            for (int i = 0; i < specializationCount; i++)
            {
                auto data = specDescs[i].data;
                auto srcData = (byte*)&data;
                auto dataSize = VK::priv::GetSpecializationConstantSize(specDescs[i].type);
                //Unsafe.CopyBlock(fullSpecData + specOffset, srcData, dataSize);
                memcpy(fullSpecData.data() + specOffset, srcData, dataSize);
                mapEntries[i].constantID = specDescs[i].id;
                mapEntries[i].offset = specOffset;
                mapEntries[i].size = dataSize;
                specOffset += dataSize;
            }
            specializationInfo.dataSize = specDataSize;
            specializationInfo.pData = fullSpecData.data();
            specializationInfo.mapEntryCount = specializationCount;
            specializationInfo.pMapEntries = mapEntries.data();
        }

        alloy::vk::SPVRemapper remapper{
            [&iaMappings](auto& d3dIn, auto& vkOut) -> bool {
                VertexInputSemantic d3dSemantic {};
                VertexInputSemantic::Name d3dSemanticName;
                if(!Str2Semantic(d3dIn.semantic, d3dSemanticName))
                    return false;
                
                d3dSemantic.name = d3dSemanticName;
                d3dSemantic.slot = d3dIn.semantic_index;

                auto findRes = iaMappings.find(d3dSemantic);
                if(findRes == iaMappings.end())
                    return false;
                vkOut.location = findRes->second;

                return true;
            }
        };

        VkShaderRAII vs{dev->LogicalDev()}, fs{dev->LogicalDev()};
        
        VkPipelineShaderStageCreateInfo stageCIs[2] = {};

        {
            auto& shader = desc.shaderSet.vertexShader;
            auto vkShader = PtrCast<VulkanShader>(shader.get());
            auto dxil = vkShader->GetDXIL();

            alloy::vk::ConverterCompilerArgs compiler_args{};
            compiler_args.shaderStage = VK_SHADER_STAGE_VERTEX_BIT;
            compiler_args.entryPoint = shader->GetDesc().entryPoint;
            
            remapper.SetStage(Veldrid::Shader::Stage::Vertex);

            alloy::vk::SPIRVBlob spvBlob;
            auto cvtRes = alloy::vk::DXIL2SPV(dxil, compiler_args, remapper, spvBlob);
            VK_ASSERT(cvtRes == alloy::vk::ShaderConverterResult::Success);

            VkShaderModuleCreateInfo shaderModuleCI {};
            shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCI.codeSize = spvBlob.code.size();
            shaderModuleCI.pCode = (const uint32_t*)spvBlob.code.data();
            VK_CHECK(vkCreateShaderModule(dev->LogicalDev(), &shaderModuleCI, nullptr, &vs));

            stageCIs[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageCIs[0].module = *vs;
            stageCIs[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            // stageCI.pName = CommonStrings.main; // Meh
            stageCIs[0].pName = "main";//Don't have a way to convince dxil-spv to change this name
        }

        {
            auto& shader = desc.shaderSet.fragmentShader;
            auto vkShader = PtrCast<VulkanShader>(shader.get());
            auto dxil = vkShader->GetDXIL();

            alloy::vk::ConverterCompilerArgs compiler_args{};
            compiler_args.shaderStage = VK_SHADER_STAGE_FRAGMENT_BIT;
            compiler_args.entryPoint = shader->GetDesc().entryPoint;
            
            remapper.SetStage(Veldrid::Shader::Stage::Fragment);

            alloy::vk::SPIRVBlob spvBlob;
            auto cvtRes = alloy::vk::DXIL2SPV(dxil, compiler_args, remapper, spvBlob);
            VK_ASSERT(cvtRes == alloy::vk::ShaderConverterResult::Success);

            VkShaderModuleCreateInfo shaderModuleCI {};
            shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCI.codeSize = spvBlob.code.size();//Although pCode is uint32_t*, this is byte size
            shaderModuleCI.pCode = (const uint32_t*)spvBlob.code.data();
            VK_CHECK(vkCreateShaderModule(dev->LogicalDev(), &shaderModuleCI, nullptr, &fs));

            stageCIs[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageCIs[1].module = *fs;
            stageCIs[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            // stageCI.pName = CommonStrings.main; // Meh
            stageCIs[1].pName = "main";//Don't have a way to convince dxil-spv to change this name
            
        }

        pipelineCI.stageCount = 2;
        pipelineCI.pStages = stageCIs;

        // ViewportState
        // Vulkan spec specifies that there must be 1 viewport no matter
        // dynamic viewport state enabled or not...
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

            refCnts.push_back(resourceLayouts[i]);            
        }
        pipelineLayoutCI.pSetLayouts = dsls.data();
        
        VkPipelineLayoutRAII pipelineLayout{dev->LogicalDev()};
        VK_CHECK(vkCreatePipelineLayout(dev->LogicalDev(), &pipelineLayoutCI, nullptr, &pipelineLayout));
        pipelineCI.layout = *pipelineLayout;
        
        // Create fake RenderPass for compatibility.
        auto& outputDesc = desc.outputs;
        
        //auto compatRenderPass = CreateFakeRenderPassForCompat(dev.get(), outputDesc, VK_SAMPLE_COUNT_1_BIT);
        auto compatRenderPass = CreateFakeRenderPassForCompat(dev.get(), outputDesc, vkSampleCount);
        
        pipelineCI.renderPass = compatRenderPass;
        
        VkPipeline devicePipeline;
        VK_CHECK(vkCreateGraphicsPipelines(dev->LogicalDev(), VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &devicePipeline));

        //auto vkVertShader = reinterpret_cast<VulkanShader*>(shaders[0].get());
        //auto vkFragShader = reinterpret_cast<VulkanShader*>(shaders[1].get());

        //_CreateStandardPipeline(dev->LogicalDev(),
        //    vkVertShader->GetHandle(), vkFragShader->GetHandle(),
        //    640,480, compatRenderPass, 
        //    pipelineLayout, devicePipeline
        //    );

        std::uint32_t resourceSetCount = desc.resourceLayouts.size();
        std::uint32_t dynamicOffsetsCount = 0;
        for(auto& layout : desc.resourceLayouts)
        {
            auto vkLayout = PtrCast<VulkanResourceLayout>(layout.get());
            dynamicOffsetsCount += vkLayout->GetDynamicBufferCount();
        }

        auto rawPipe = new VulkanGraphicsPipeline(dev);
        rawPipe->_devicePipeline = devicePipeline;
        rawPipe->_pipelineLayout = pipelineLayout.Reset();
        rawPipe->_renderPass = compatRenderPass;
        rawPipe->scissorTestEnabled = rsDesc.scissorTestEnabled;
        rawPipe->resourceSetCount = resourceSetCount;
        rawPipe->dynamicOffsetsCount = dynamicOffsetsCount;
        rawPipe->_refCnts = std::move(refCnts);

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
                specDataSize += VK::priv::GetSpecializationConstantSize(spec->type);
            }
            std::vector<std::uint8_t> fullSpecData(specDataSize);
            unsigned specializationCount = specDescs.size();
            std::vector<VkSpecializationMapEntry> mapEntries(specializationCount);
            unsigned specOffset = 0;
            for (int i = 0; i < specializationCount; i++)
            {
                auto data = specDescs[i]->data;
                byte* srcData = (byte*)&data;
                unsigned dataSize = VK::priv::GetSpecializationConstantSize(specDescs[i]->type);
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

        
        alloy::vk::SPVRemapper remapper{nullptr};

        VkShaderRAII cs{dev->LogicalDev()};
        
        VkPipelineShaderStageCreateInfo stageCI;

        {
            auto& shader = desc.computeShader;
            auto vkShader = PtrCast<VulkanShader>(shader.get());
            auto dxil = vkShader->GetDXIL();

            alloy::vk::ConverterCompilerArgs compiler_args{};
            compiler_args.shaderStage = VK_SHADER_STAGE_COMPUTE_BIT;
            compiler_args.entryPoint = shader->GetDesc().entryPoint;

            remapper.SetStage(Veldrid::Shader::Stage::Compute);

            alloy::vk::SPIRVBlob spvBlob;
            auto cvtRes = alloy::vk::DXIL2SPV(dxil, compiler_args, remapper, spvBlob);
            VK_ASSERT(cvtRes == alloy::vk::ShaderConverterResult::Success);

            VkShaderModuleCreateInfo shaderModuleCI {};
            shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCI.codeSize = spvBlob.code.size() / 4;
            shaderModuleCI.pCode = (const uint32_t*)spvBlob.code.data();
            VK_CHECK(vkCreateShaderModule(dev->LogicalDev(), &shaderModuleCI, nullptr, &cs));

            stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageCI.module = *cs;
            stageCI.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            // stageCI.pName = CommonStrings.main; // Meh
            stageCI.pName = "main";//Don't have a way to convince dxil-spv to change this name
        }

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
