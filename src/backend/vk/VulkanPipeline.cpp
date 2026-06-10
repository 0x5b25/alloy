#include "VulkanPipeline.hpp"

#include "alloy/common/Common.hpp"
#include "alloy/Helpers.hpp"

#include <vector>
#include <cassert>
#include <cstring>

#include "VkTypeCvt.hpp"
#include "VkCommon.hpp"
#include "VulkanDevice.hpp"
#include "VulkanShader.hpp"
#include "VulkanBindableResource.hpp"


namespace alloy::vk{
class VkShaderRAII {
    VulkanDevice* _dev;
    VkShaderModule _mod;
public:
    VkShaderRAII(VulkanDevice* dev) : _dev(dev), _mod(VK_NULL_HANDLE) {}
    ~VkShaderRAII() {
        if(_mod != VK_NULL_HANDLE)
            VK_DEV_CALL(_dev, vkDestroyShaderModule(_dev->LogicalDev(), _mod, nullptr));
    }
    VkShaderModule* operator&() {return &_mod;}
    VkShaderModule operator*() {return _mod;}
    VkShaderModule Reset() {
        auto res = _mod;  _mod = VK_NULL_HANDLE; return res;
    }
};


    static bool Str2Semantic(const char* str, VertexInputSemantic::Name& semantic){

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

    VulkanPipelineBase::~VulkanPipelineBase() {
        VK_DEV_CALL(dev, vkDestroyPipeline(dev->LogicalDev(), _devicePipeline, nullptr));
    }

    VulkanComputePipeline::~VulkanComputePipeline(){

    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline(){

    }

    common::sp<IGfxPipeline> VulkanGraphicsPipeline::Make(
        const common::sp<VulkanDevice>& dev,
        const GraphicsPipelineDescription& desc
    ) {
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
        auto attachmentsCount = desc.attachmentState.colorAttachments.size();
        std::vector<VkPipelineColorBlendAttachmentState> attachments(attachmentsCount);
        for (int i = 0; i < attachmentsCount; i++)
        {
            auto vdDesc = desc.attachmentState.colorAttachments[i];
            auto& attachmentState = attachments[i];
            attachmentState.srcColorBlendFactor = VdToVkBlendFactor(vdDesc.sourceColorFactor);
            attachmentState.dstColorBlendFactor = VdToVkBlendFactor(vdDesc.destinationColorFactor);
            attachmentState.colorBlendOp = VdToVkBlendOp(vdDesc.colorFunction);
            attachmentState.srcAlphaBlendFactor = VdToVkBlendFactor(vdDesc.sourceAlphaFactor);
            attachmentState.dstAlphaBlendFactor = VdToVkBlendFactor(vdDesc.destinationAlphaFactor);
            attachmentState.alphaBlendOp = VdToVkBlendOp(vdDesc.alphaFunction);
            attachmentState.colorWriteMask = VdToVkColorWriteMask(vdDesc.colorWriteMask);
            attachmentState.blendEnable = vdDesc.blendEnabled;
        }

        blendStateCI.attachmentCount = attachmentsCount;
        blendStateCI.pAttachments = attachments.data();
        auto& blendFactor = desc.attachmentState.blendConstant;
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

        //depthClampEnable controls whether to clamp the fragment’s depth values
        // as described in Depth Test. If the pipeline is not created with
        //VkPipelineRasterizationDepthClipStateCreateInfoEXT present then enabling
        //depth clamp will also disable clipping primitives to the z planes of
        //the frustrum as described in Primitive Clipping. Otherwise depth clipping
        //is controlled by the state set in VkPipelineRasterizationDepthClipStateCreateInfoEXT.

        rsCI.depthClampEnable = false;
        VkPipelineRasterizationDepthClipStateCreateInfoEXT rsDepthClipCI{};
        if(dev->GetVkFeatures().flags.supportsDepthClip) {

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
        VkSampleCountFlagBits vkSampleCount = VdToVkSampleCount(desc.attachmentState.sampleCount);
        multisampleCI.rasterizationSamples = vkSampleCount;
        multisampleCI.alphaToCoverageEnable = desc.attachmentState.alphaToCoverageEnabled;
        pipelineCI.pMultisampleState = &multisampleCI;

        // Input Assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{};
        inputAssemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyCI.topology = VdToVkPrimitiveTopology(desc.primitiveTopology);
        inputAssemblyCI.primitiveRestartEnable = VK_FALSE;
        pipelineCI.pInputAssemblyState = &inputAssemblyCI;

        // Pipeline Layout
        auto vkResLayout = SPCast<VulkanResourceLayout>(desc.resourceLayout);

        pipelineCI.layout = vkResLayout 
                          ? vkResLayout->GetPipelineLayout()
                          : VK_NULL_HANDLE
                          ;

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

                attributeDescs[targetIndex].format = VdToVkShaderDataType(inputElement.format);
                attributeDescs[targetIndex].binding = binding;
                attributeDescs[targetIndex].location = thisLocation;
                attributeDescs[targetIndex].offset = inputElement.offset != 0
                    ? inputElement.offset
                    : currentOffset;

                targetIndex += 1;
                currentOffset += FormatHelpers::GetSizeInBytes(inputElement.format);
            }

            targetLocation += inputDesc.elements.size();
        }

        vertexInputCI.vertexBindingDescriptionCount = bindingCount;
        vertexInputCI.pVertexBindingDescriptions = bindingDescs.data();
        vertexInputCI.vertexAttributeDescriptionCount = attributeCount;
        vertexInputCI.pVertexAttributeDescriptions = attributeDescs.data();

        pipelineCI.pVertexInputState = &vertexInputCI;

        // Shader Stage

        //VkSpecializationInfo specializationInfo{};
        //auto& specDescs = desc.shaderSet.specializations;
        //if (!specDescs.empty())
        //{
        //    unsigned specDataSize = 0;
        //    for (auto& spec : specDescs) {
        //        specDataSize += GetSpecializationConstantSize(spec.type);
        //    }
        //    std::vector<std::uint8_t> fullSpecData(specDataSize);
        //    int specializationCount = specDescs.size();
        //    std::vector<VkSpecializationMapEntry> mapEntries(specializationCount);
        //    unsigned specOffset = 0;
        //    for (int i = 0; i < specializationCount; i++)
        //    {
        //        auto data = specDescs[i].data;
        //        auto srcData = (byte*)&data;
        //        auto dataSize = GetSpecializationConstantSize(specDescs[i].type);
        //        //Unsafe.CopyBlock(fullSpecData + specOffset, srcData, dataSize);
        //        memcpy(fullSpecData.data() + specOffset, srcData, dataSize);
        //        mapEntries[i].constantID = specDescs[i].id;
        //        mapEntries[i].offset = specOffset;
        //        mapEntries[i].size = dataSize;
        //        specOffset += dataSize;
        //    }
        //    specializationInfo.dataSize = specDataSize;
        //    specializationInfo.pData = fullSpecData.data();
        //    specializationInfo.mapEntryCount = specializationCount;
        //    specializationInfo.pMapEntries = mapEntries.data();
        //}

        SPVRemapper remapper {
            PtrCast<VulkanResourceLayout>(desc.resourceLayout.get()),
            &iaMappings
        };
        //alloy::vk::SPVRemapper remapper{
        //    [&iaMappings](auto& d3dIn, auto& vkOut) -> bool {
        //        VertexInputSemantic d3dSemantic {};
        //        VertexInputSemantic::Name d3dSemanticName;
        //        if(!Str2Semantic(d3dIn.semantic, d3dSemanticName))
        //            return false;
        //
        //        d3dSemantic.name = d3dSemanticName;
        //        d3dSemantic.slot = d3dIn.semantic_index;
        //
        //        auto findRes = iaMappings.find(d3dSemantic);
        //        if(findRes == iaMappings.end())
        //            return false;
        //        vkOut.location = findRes->second;
        //
        //        return true;
        //    }
        //};

        VkShaderRAII vs{dev.get()}, fs{dev.get()};

        VkPipelineShaderStageCreateInfo stageCIs[2] = {};

        {
            auto& shader = desc.shaderSet.vertexShader;
            auto vkShader = PtrCast<VulkanShader>(shader.get());
            auto dxil = vkShader->GetByteCode();

            alloy::vk::ConverterCompilerArgs compiler_args{};
            compiler_args.shaderStage = VK_SHADER_STAGE_VERTEX_BIT;
            compiler_args.entryPoint = shader->GetDesc().entryPoint;
            if(desc.resourceLayout) {
                auto resourceLayout = PtrCast<VulkanResourceLayout>(desc.resourceLayout.get());
                compiler_args.root_constant_words =  resourceLayout->GetPushConstantSize();
            }

            remapper.SetStage(alloy::IShader::Stage::Vertex);

            alloy::vk::SPIRVBlob spvBlob;
            auto cvtRes = alloy::vk::DXIL2SPV(*dev, dxil, compiler_args, remapper, spvBlob);
            VK_ASSERT(cvtRes == alloy::vk::ShaderConverterResult::Success);

            VkShaderModuleCreateInfo shaderModuleCI {};
            shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCI.codeSize = spvBlob.code.size();
            shaderModuleCI.pCode = (const uint32_t*)spvBlob.code.data();
            VK_CHECK(VK_DEV_CALL(dev,
                vkCreateShaderModule(dev->LogicalDev(), &shaderModuleCI, nullptr, &vs)));

            stageCIs[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageCIs[0].module = *vs;
            stageCIs[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            // stageCI.pName = CommonStrings.main; // Meh
            stageCIs[0].pName = "main";//Don't have a way to convince dxil-spv to change this name
        }

        {
            auto& shader = desc.shaderSet.fragmentShader;
            auto vkShader = PtrCast<VulkanShader>(shader.get());
            auto dxil = vkShader->GetByteCode();

            alloy::vk::ConverterCompilerArgs compiler_args{};
            compiler_args.shaderStage = VK_SHADER_STAGE_FRAGMENT_BIT;
            compiler_args.entryPoint = shader->GetDesc().entryPoint;
            if(desc.resourceLayout) {
                auto resourceLayout = PtrCast<VulkanResourceLayout>(desc.resourceLayout.get());
                compiler_args.root_constant_words =  resourceLayout->GetPushConstantSize();
            }

            remapper.SetStage(alloy::IShader::Stage::Fragment);

            alloy::vk::SPIRVBlob spvBlob;
            auto cvtRes = alloy::vk::DXIL2SPV(*dev, dxil, compiler_args, remapper, spvBlob);
            VK_ASSERT(cvtRes == alloy::vk::ShaderConverterResult::Success);

            VkShaderModuleCreateInfo shaderModuleCI {};
            shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCI.codeSize = spvBlob.code.size();//Although pCode is uint32_t*, this is byte size
            shaderModuleCI.pCode = (const uint32_t*)spvBlob.code.data();
            VK_CHECK(VK_DEV_CALL(dev,
                vkCreateShaderModule(dev->LogicalDev(), &shaderModuleCI, nullptr, &fs)));

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

        // Create fake RenderPass for compatibility.

        //We have dynamic rendering now
        //auto compatRenderPass = CreateFakeRenderPassForCompat(dev.get(), outputDesc, VK_SAMPLE_COUNT_1_BIT);
        //auto compatRenderPass = CreateFakeRenderPassForCompat(dev.get(), outputDesc, vkSampleCount);
        //pipelineCI.renderPass = compatRenderPass;

        // Provide information for dynamic rendering
        std::vector<VkFormat> colorAttachmentFormats{};
        colorAttachmentFormats.reserve(desc.attachmentState.colorAttachments.size());

        for(auto& a : desc.attachmentState.colorAttachments) {
            auto f = a.format;
            colorAttachmentFormats.push_back(VdToVkPixelFormat(f, false));
        }

        VkPipelineRenderingCreateInfoKHR dynRenderingCI{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
            .pNext                   = nullptr,
            .colorAttachmentCount    = (uint32_t)colorAttachmentFormats.size(),
            .pColorAttachmentFormats = colorAttachmentFormats.data(),
        };

        if(desc.attachmentState.depthStencilAttachment.has_value()) {

            PixelFormat depthFormat = desc.attachmentState.depthStencilAttachment->depthStencilFormat;
            auto vkFormat = VdToVkPixelFormat(depthFormat, true);
            dynRenderingCI.depthAttachmentFormat  = vkFormat;
            if(FormatHelpers::IsStencilFormat(depthFormat))
                dynRenderingCI.stencilAttachmentFormat = vkFormat;
        }
        // Use the pNext to point to the rendering create struct
        pipelineCI.pNext               = &dynRenderingCI; // reference the new dynamic structure
        pipelineCI.renderPass          = nullptr; // previously required non-null

        VkPipeline devicePipeline;
        VK_CHECK(VK_DEV_CALL(dev,
            vkCreateGraphicsPipelines(
                dev->LogicalDev(),
                VK_NULL_HANDLE,
                1,
                &pipelineCI,
                nullptr,
                &devicePipeline)));

        //auto vkVertShader = reinterpret_cast<VulkanShader*>(shaders[0].get());
        //auto vkFragShader = reinterpret_cast<VulkanShader*>(shaders[1].get());

        //_CreateStandardPipeline(dev->LogicalDev(),
        //    vkVertShader->GetHandle(), vkFragShader->GetHandle(),
        //    640,480, compatRenderPass,
        //    pipelineLayout, devicePipeline
        //    );

        std::uint32_t resourceSetCount = 0;
        if(vkResLayout) {
            resourceSetCount = vkResLayout->GetResSetInfo().size();
        }
        std::uint32_t dynamicOffsetsCount = 0;
        //for(auto& layout : desc.resourceLayouts)
        //{
        //    auto vkLayout = PtrCast<VulkanResourceLayout>(layout.get());
        //    dynamicOffsetsCount += vkLayout->GetDynamicBufferCount();
        //}

        auto rawPipe = new VulkanGraphicsPipeline(dev);
        rawPipe->_devicePipeline = devicePipeline;
        //rawPipe->_renderPass = compatRenderPass;
        rawPipe->scissorTestEnabled = rsDesc.scissorTestEnabled;
        rawPipe->resourceSetCount = resourceSetCount;
        rawPipe->dynamicOffsetsCount = dynamicOffsetsCount;
        //if(vkResLayout) {
        //    rawPipe->pushConstants = vkResLayout->GetPushConstants();
        //}
        rawPipe->_layout = std::move(vkResLayout);

        return common::sp(rawPipe);
    }



    common::sp<IComputePipeline> VulkanComputePipeline::Make(
        const common::sp<VulkanDevice>& dev,
        const ComputePipelineDescription& desc
    ) {
        VkComputePipelineCreateInfo pipelineCI {};
        pipelineCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;

        // Pipeline Layout        
        auto vkResLayout = SPCast<VulkanResourceLayout>(desc.resourceLayout);

        pipelineCI.layout = vkResLayout 
                          ? vkResLayout->GetPipelineLayout()
                          : VK_NULL_HANDLE
                          ;

        // Shader Stage

        //VkSpecializationInfo specializationInfo;
        //auto& specDescs = desc.specializations;
        //if (!specDescs.empty())
        //{
        //    unsigned specDataSize = 0;
        //    for(auto& spec : specDescs)
        //    {
        //        specDataSize += GetSpecializationConstantSize(spec->type);
        //    }
        //    std::vector<std::uint8_t> fullSpecData(specDataSize);
        //    unsigned specializationCount = specDescs.size();
        //    std::vector<VkSpecializationMapEntry> mapEntries(specializationCount);
        //    unsigned specOffset = 0;
        //    for (int i = 0; i < specializationCount; i++)
        //    {
        //        auto data = specDescs[i]->data;
        //        byte* srcData = (byte*)&data;
        //        unsigned dataSize = GetSpecializationConstantSize(specDescs[i]->type);
        //        memcpy(fullSpecData.data() + specOffset, srcData, dataSize);
        //        mapEntries[i].constantID = specDescs[i]->id;
        //        mapEntries[i].offset = specOffset;
        //        mapEntries[i].size = dataSize;
        //        specOffset += dataSize;
        //    }
        //    specializationInfo.dataSize = specDataSize;
        //    specializationInfo.pData = fullSpecData.data();
        //    specializationInfo.mapEntryCount = specializationCount;
        //    specializationInfo.pMapEntries = mapEntries.data();
        //}

        SPVRemapper remapper {
            PtrCast<VulkanResourceLayout>(desc.resourceLayout.get()),
            nullptr
        };

        VkShaderRAII cs{dev.get()};

        VkPipelineShaderStageCreateInfo& stageCI = pipelineCI.stage;

        {
            auto& shader = desc.computeShader;
            auto vkShader = PtrCast<VulkanShader>(shader.get());
            auto dxil = vkShader->GetByteCode();

            alloy::vk::ConverterCompilerArgs compiler_args{};
            compiler_args.shaderStage = VK_SHADER_STAGE_COMPUTE_BIT;
            compiler_args.entryPoint = shader->GetDesc().entryPoint;

            if(desc.resourceLayout) {
                auto resourceLayout = PtrCast<VulkanResourceLayout>(desc.resourceLayout.get());
                compiler_args.root_constant_words =  resourceLayout->GetPushConstantSize();
            }

            remapper.SetStage(alloy::IShader::Stage::Compute);

            alloy::vk::SPIRVBlob spvBlob;
            auto cvtRes = alloy::vk::DXIL2SPV(*dev, dxil, compiler_args, remapper, spvBlob);
            VK_ASSERT(cvtRes == alloy::vk::ShaderConverterResult::Success);

            VkShaderModuleCreateInfo shaderModuleCI {};
            shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCI.codeSize = spvBlob.code.size();
            shaderModuleCI.pCode = (const uint32_t*)spvBlob.code.data();
            VK_CHECK(VK_DEV_CALL(dev,
                vkCreateShaderModule(dev->LogicalDev(), &shaderModuleCI, nullptr, &cs)));

            stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageCI.module = *cs;
            stageCI.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            // stageCI.pName = CommonStrings.main; // Meh
            stageCI.pName = "main";//Don't have a way to convince dxil-spv to change this name
        }


        VkPipeline devicePipeline;
        VK_CHECK(VK_DEV_CALL(dev, vkCreateComputePipelines(
            dev->LogicalDev(),VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &devicePipeline
        )));


        std::uint32_t resourceSetCount = 0;
        if(vkResLayout) {
            resourceSetCount = vkResLayout->GetResSetInfo().size();
        }
        //std::uint32_t dynamicOffsetsCount = 0;
        //for(auto& layout : desc.resourceLayouts)
        //{
        //    auto vkLayout = PtrCast<VulkanResourceLayout>(layout.get());
        //    dynamicOffsetsCount += vkLayout->GetDynamicBufferCount();
        //}

        auto rawPipe = new VulkanComputePipeline(dev);
        rawPipe->_devicePipeline = devicePipeline;
        rawPipe->resourceSetCount = resourceSetCount;

        //if(desc.resourceLayout) {
        //    auto resourceLayout = PtrCast<VulkanResourceLayout>(desc.resourceLayout.get());
        //    rawPipe->pushConstants = resourceLayout->GetPushConstants();
        //}
        rawPipe->_layout = std::move(vkResLayout);
        //rawPipe->dynamicOffsetsCount = dynamicOffsetsCount;

        return common::sp(rawPipe);
    }


    common::sp<IMeshShaderPipeline> VulkanMeshShaderPipeline::Make(
        const common::sp<VulkanDevice>& dev,
        const MeshShaderPipelineDescription& desc
    ) {

        VkGraphicsPipelineCreateInfo pipelineCI{};
        pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        VkPipelineColorBlendStateCreateInfo blendStateCI{};
        blendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        auto attachmentsCount = desc.attachmentState.colorAttachments.size();
        std::vector<VkPipelineColorBlendAttachmentState> attachments(attachmentsCount);
        for (int i = 0; i < attachmentsCount; i++)
        {
            auto vdDesc = desc.attachmentState.colorAttachments[i];
            auto& attachmentState = attachments[i];
            attachmentState.srcColorBlendFactor = VdToVkBlendFactor(vdDesc.sourceColorFactor);
            attachmentState.dstColorBlendFactor = VdToVkBlendFactor(vdDesc.destinationColorFactor);
            attachmentState.colorBlendOp = VdToVkBlendOp(vdDesc.colorFunction);
            attachmentState.srcAlphaBlendFactor = VdToVkBlendFactor(vdDesc.sourceAlphaFactor);
            attachmentState.dstAlphaBlendFactor = VdToVkBlendFactor(vdDesc.destinationAlphaFactor);
            attachmentState.alphaBlendOp = VdToVkBlendOp(vdDesc.alphaFunction);
            attachmentState.colorWriteMask = VdToVkColorWriteMask(vdDesc.colorWriteMask);
            attachmentState.blendEnable = vdDesc.blendEnabled;
        }

        blendStateCI.attachmentCount = attachmentsCount;
        blendStateCI.pAttachments = attachments.data();
        auto& blendFactor = desc.attachmentState.blendConstant;
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

        //depthClampEnable controls whether to clamp the fragment’s depth values
        // as described in Depth Test. If the pipeline is not created with
        //VkPipelineRasterizationDepthClipStateCreateInfoEXT present then enabling
        //depth clamp will also disable clipping primitives to the z planes of
        //the frustrum as described in Primitive Clipping. Otherwise depth clipping
        //is controlled by the state set in VkPipelineRasterizationDepthClipStateCreateInfoEXT.

        rsCI.depthClampEnable = false;
        VkPipelineRasterizationDepthClipStateCreateInfoEXT rsDepthClipCI{};
        if(dev->GetVkFeatures().flags.supportsDepthClip) {

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
        VkSampleCountFlagBits vkSampleCount = VdToVkSampleCount(desc.attachmentState.sampleCount);
        multisampleCI.rasterizationSamples = vkSampleCount;
        multisampleCI.alphaToCoverageEnable = desc.attachmentState.alphaToCoverageEnabled;
        pipelineCI.pMultisampleState = &multisampleCI;

        // Input Assembly
        //VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{};
        //inputAssemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        //inputAssemblyCI.topology = VdToVkPrimitiveTopology(desc.primitiveTopology);
        //inputAssemblyCI.primitiveRestartEnable = VK_FALSE;
        //pipelineCI.pInputAssemblyState = &inputAssemblyCI;

        // Pipeline Layout
        auto vkResLayout = SPCast<VulkanResourceLayout>(desc.resourceLayout);

        pipelineCI.layout = vkResLayout 
                          ? vkResLayout->GetPipelineLayout()
                          : VK_NULL_HANDLE
                          ;

        //#TODO: revisit dxil-spv remapper for mesh shaders
        SPVRemapper remapper {
            PtrCast<VulkanResourceLayout>(desc.resourceLayout.get()),
            nullptr
        };

        VkShaderRAII shaderMods[3] { {dev.get()}, {dev.get()}, {dev.get()}};
        pipelineCI.stageCount = 0;
        VkPipelineShaderStageCreateInfo stageCIs[3] = {};


        auto _CreateShaderStageCI = [&](const common::sp<IShader>& shader, IShader::Stage stage) {
            auto slot = pipelineCI.stageCount++;
            auto& mod = shaderMods[slot];
            auto& ci = stageCIs[slot];
            auto vkShader = PtrCast<VulkanShader>(shader.get());
            auto dxil = vkShader->GetByteCode();

            alloy::vk::ConverterCompilerArgs compiler_args{};
            compiler_args.shaderStage = VdToVkShaderStageSingle(stage);
            compiler_args.entryPoint = shader->GetDesc().entryPoint;
            if(desc.resourceLayout) {
                auto resourceLayout = PtrCast<VulkanResourceLayout>(desc.resourceLayout.get());
                compiler_args.root_constant_words =  resourceLayout->GetPushConstantSize();
            }

            //#TODO: revisit dxil-spv remapper for mesh shaders
            remapper.SetStage(stage);

            alloy::vk::SPIRVBlob spvBlob;
            auto cvtRes = alloy::vk::DXIL2SPV(*dev, dxil, compiler_args, remapper, spvBlob);
            VK_ASSERT(cvtRes == alloy::vk::ShaderConverterResult::Success);

            VkShaderModuleCreateInfo shaderModuleCI {};
            shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCI.codeSize = spvBlob.code.size();
            shaderModuleCI.pCode = (const uint32_t*)spvBlob.code.data();
            VK_CHECK(VK_DEV_CALL(dev,
                vkCreateShaderModule(dev->LogicalDev(), &shaderModuleCI, nullptr, &mod)));

            ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            ci.module = *mod;
            ci.stage = VdToVkShaderStageSingle(stage);
            // stageCI.pName = CommonStrings.main; // Meh
            ci.pName = "main";//Don't have a way to convince dxil-spv to change this name
        };

        if(desc.taskShader) {
            _CreateShaderStageCI(desc.taskShader, IShader::Stage::Task);
        }

        _CreateShaderStageCI(desc.meshShader, IShader::Stage::Mesh);
        _CreateShaderStageCI(desc.fragmentShader, IShader::Stage::Fragment);

        pipelineCI.pStages = stageCIs;

        // ViewportState
        // Vulkan spec specifies that there must be 1 viewport no matter
        // dynamic viewport state enabled or not...
        VkPipelineViewportStateCreateInfo viewportStateCI{};
        viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateCI.viewportCount = 1;
        viewportStateCI.scissorCount = 1;

        pipelineCI.pViewportState = &viewportStateCI;


        // Provide information for dynamic rendering
        std::vector<VkFormat> colorAttachmentFormats{};
        colorAttachmentFormats.reserve(desc.attachmentState.colorAttachments.size());

        for(auto& a : desc.attachmentState.colorAttachments) {
            auto f = a.format;
            colorAttachmentFormats.push_back(VdToVkPixelFormat(f, false));
        }

        VkPipelineRenderingCreateInfoKHR dynRenderingCI{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
            .pNext                   = nullptr,
            .colorAttachmentCount    = (uint32_t)colorAttachmentFormats.size(),
            .pColorAttachmentFormats = colorAttachmentFormats.data(),
        };

        if(desc.attachmentState.depthStencilAttachment.has_value()) {

            PixelFormat depthFormat = desc.attachmentState.depthStencilAttachment->depthStencilFormat;
            auto vkFormat = VdToVkPixelFormat(depthFormat, true);
            dynRenderingCI.depthAttachmentFormat  = vkFormat;
            if(FormatHelpers::IsStencilFormat(depthFormat))
                dynRenderingCI.stencilAttachmentFormat = vkFormat;
        }
        // Use the pNext to point to the rendering create struct
        pipelineCI.pNext               = &dynRenderingCI; // reference the new dynamic structure
        pipelineCI.renderPass          = nullptr; // previously required non-null

        VkPipeline devicePipeline;
        VK_CHECK(VK_DEV_CALL(dev,
            vkCreateGraphicsPipelines(
                dev->LogicalDev(),
                VK_NULL_HANDLE,
                1,
                &pipelineCI,
                nullptr,
                &devicePipeline)));

        //auto vkVertShader = reinterpret_cast<VulkanShader*>(shaders[0].get());
        //auto vkFragShader = reinterpret_cast<VulkanShader*>(shaders[1].get());

        //_CreateStandardPipeline(dev->LogicalDev(),
        //    vkVertShader->GetHandle(), vkFragShader->GetHandle(),
        //    640,480, compatRenderPass,
        //    pipelineLayout, devicePipeline
        //    );

        std::uint32_t resourceSetCount = 0;
        if(vkResLayout) {
            resourceSetCount = vkResLayout->GetResSetInfo().size();
        }
        std::uint32_t dynamicOffsetsCount = 0;
        //for(auto& layout : desc.resourceLayouts)
        //{
        //    auto vkLayout = PtrCast<VulkanResourceLayout>(layout.get());
        //    dynamicOffsetsCount += vkLayout->GetDynamicBufferCount();
        //}

        auto rawPipe = new VulkanMeshShaderPipeline(dev);
        rawPipe->_devicePipeline = devicePipeline;
        //rawPipe->_renderPass = compatRenderPass;
        rawPipe->scissorTestEnabled = rsDesc.scissorTestEnabled;
        rawPipe->resourceSetCount = resourceSetCount;
        rawPipe->dynamicOffsetsCount = dynamicOffsetsCount;
        //if(vkResLayout) {
        //    rawPipe->pushConstants = vkResLayout->GetPushConstants();
        //}
        rawPipe->_layout = std::move(vkResLayout);

        return common::sp(rawPipe);
    }

}
