#include "DXCPipeline.hpp"

//3rd-party headers

//alloy public headers
#include "alloy/common/Common.hpp"
#include "alloy/Helpers.hpp"

//standard library headers
#include <string>
#include <cassert>
#include <unordered_set>

//backend specific headers
#include "DXCDevice.hpp"
#include "DXCShader.hpp"
#include "DXCBindableResource.hpp"

//platform specific headers

//Local headers
#include "D3DTypeCvt.hpp"
#include "D3DCommon.hpp"

//#include "spirv/nir_spirv.h"
//#include "nir.h"
//#include "nir_xfb_info.h"

//#include "dxil_nir.h"
//#include "nir_to_dxil.h"
//#include "dxil_spirv_nir.h"
//#include "spirv_to_dxil.h"
//#include "dxil_validator.h"


using Microsoft::WRL::ComPtr;


namespace alloy::VK::priv
{

} // namespace alloy::VK::priv



namespace alloy::dxc
{

    
    DXCPipelineBase:: ~DXCPipelineBase() {

    }
    
    DXCGraphicsPipeline::~DXCGraphicsPipeline() {

    }


    common::sp<IGfxPipeline> DXCGraphicsPipeline::Make(
        const common::sp<DXCDevice> &dev, 
        const GraphicsPipelineDescription &desc
    ) {
        // Define the vertex input layout.
        //D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        //{
        //    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        //    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        //};

        /*
        typedef struct D3D12_GRAPHICS_PIPELINE_STATE_DESC
        {
            ID3D12RootSignature *pRootSignature;
            D3D12_SHADER_BYTECODE VS;
            D3D12_SHADER_BYTECODE PS;
            D3D12_SHADER_BYTECODE DS;
            D3D12_SHADER_BYTECODE HS;
            D3D12_SHADER_BYTECODE GS;
            D3D12_STREAM_OUTPUT_DESC StreamOutput;
            * D3D12_BLEND_DESC BlendState;
            * UINT SampleMask;
            * D3D12_RASTERIZER_DESC RasterizerState;
            * D3D12_DEPTH_STENCIL_DESC DepthStencilState;
            * D3D12_INPUT_LAYOUT_DESC InputLayout;
            D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
            * D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
            * UINT NumRenderTargets;
            * DXGI_FORMAT RTVFormats[ 8 ];
            * DXGI_FORMAT DSVFormat;
            * DXGI_SAMPLE_DESC SampleDesc;
            UINT NodeMask;
            D3D12_CACHED_PIPELINE_STATE CachedPSO;
            D3D12_PIPELINE_STATE_FLAGS Flags;
        } 	D3D12_GRAPHICS_PIPELINE_STATE_DESC;
        */

        // Describe and create the graphics pipeline state object (PSO).
        std::unordered_set<common::sp<RefCntBase>> refCnts;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc {};
        //psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        //psoDesc.pRootSignature = m_rootSignature.Get();
        //psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        //psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        //psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        //psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        //psoDesc.DepthStencilState.DepthEnable = FALSE;
        //psoDesc.DepthStencilState.StencilEnable = FALSE;
        //psoDesc.SampleMask = UINT_MAX;
        //psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        //psoDesc.NumRenderTargets = 1;
        //psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        //psoDesc.SampleDesc.Count = 1;

        auto dxcResLayout = PtrCast<DXCResourceLayout>(desc.resourceLayout.get());

        psoDesc.pRootSignature = dxcResLayout->GetHandle();

        auto _FillShaderDesc = [&](common::sp<IShader> shader, D3D12_SHADER_BYTECODE& desc){
            auto dxcShader = static_cast<DXCShader*>(shader.get());
            desc.BytecodeLength = dxcShader->GetDataSizeInBytes();
            desc.pShaderBytecode = dxcShader->GetData();
            refCnts.insert(shader);
        };

        _FillShaderDesc(desc.shaderSet.vertexShader, psoDesc.VS);
        _FillShaderDesc(desc.shaderSet.fragmentShader, psoDesc.PS);

        //for(auto& shader : desc.shaderSet.shaders) {
        //    auto& desc = shader->GetDesc();
        //
        //    switch (desc.stage)
        //    {
        //    case alloy::Shader::Stage::Vertex: _FillShaderDesc(shader, psoDesc.VS); break;
        //    case alloy::Shader::Stage::Geometry: _FillShaderDesc(shader, psoDesc.GS); break;
        //    case alloy::Shader::Stage::TessellationControl: _FillShaderDesc(shader, psoDesc.HS); break;
        //    case alloy::Shader::Stage::TessellationEvaluation: _FillShaderDesc(shader, psoDesc.DS); break;
        //    case alloy::Shader::Stage::Fragment: _FillShaderDesc(shader, psoDesc.PS); break;
        //    
        //    default: {
        //        //TODO: report unsupported shader stages
        //    } break;
        //    }
        //
        //}
        

        /*
        // Blend State
        VkPipelineColorBlendStateCreateInfo blendStateCI{};
        blendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        auto attachmentsCount = desc.blendState.attachments.size();
        std::vector<VkPipelineColorBlendAttachmentState> attachments(attachmentsCount);
        for (int i = 0; i < attachmentsCount; i++)
        {
            auto vdDesc = desc.blendState.attachments[i];
            auto& attachmentState = attachments[i];
            attachmentState.srcColorBlendFactor = VdToVkBlendFactor(vdDesc.sourceAlphaFactor);
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
        auto& blendFactor = desc.blendState.blendConstant;
        blendStateCI.blendConstants[0] = blendFactor.r;
        blendStateCI.blendConstants[1] = blendFactor.g;
        blendStateCI.blendConstants[2] = blendFactor.b;
        blendStateCI.blendConstants[3] = blendFactor.a;

        pipelineCI.pColorBlendState = &blendStateCI;
        */
        {
            //For BlendFactor::BlendFactor: 
            //The blend factor is the blend factor set with 
            //ID3D12GraphicsCommandList::OMSetBlendFactor. No pre-blend operation.

            /*void OMSetBlendFactor(
              [in, optional] const FLOAT [4] BlendFactor
            );*/
            auto& blendFactor = desc.blendState.blendConstant;
        //blendStateCI.blendConstants[0] = blendFactor.r;
        //blendStateCI.blendConstants[1] = blendFactor.g;
        //blendStateCI.blendConstants[2] = blendFactor.b;
        //blendStateCI.blendConstants[3] = blendFactor.a;

            auto& blendState = psoDesc.BlendState;
            // BOOL AlphaToCoverageEnable;
            //BOOL IndependentBlendEnable;

            auto attachmentsCount = desc.blendState.attachments.size();

            assert(attachmentsCount <= 8);

            for (int i = 0; i < attachmentsCount; i++)
            {
                auto vdDesc = desc.blendState.attachments[i];
                auto& attachmentState = blendState.RenderTarget[i];

                /*typedef struct D3D12_RENDER_TARGET_BLEND_DESC
                    {
                    BOOL BlendEnable;
                    BOOL LogicOpEnable;
                    D3D12_BLEND SrcBlend;
                    D3D12_BLEND DestBlend;
                    D3D12_BLEND_OP BlendOp;
                    D3D12_BLEND SrcBlendAlpha;
                    D3D12_BLEND DestBlendAlpha;
                    D3D12_BLEND_OP BlendOpAlpha;
                    D3D12_LOGIC_OP LogicOp;
                    UINT8 RenderTargetWriteMask;
                    } 	D3D12_RENDER_TARGET_BLEND_DESC;
                    
                    typedef 
                    enum D3D12_BLEND
                        {
                            D3D12_BLEND_ZERO	= 1,
                            D3D12_BLEND_ONE	= 2,
                            D3D12_BLEND_SRC_COLOR	= 3,
                            D3D12_BLEND_INV_SRC_COLOR	= 4,
                            D3D12_BLEND_SRC_ALPHA	= 5,
                            D3D12_BLEND_INV_SRC_ALPHA	= 6,
                            D3D12_BLEND_DEST_ALPHA	= 7,
                            D3D12_BLEND_INV_DEST_ALPHA	= 8,
                            D3D12_BLEND_DEST_COLOR	= 9,
                            D3D12_BLEND_INV_DEST_COLOR	= 10,
                            D3D12_BLEND_SRC_ALPHA_SAT	= 11,
                            D3D12_BLEND_BLEND_FACTOR	= 14,
                            D3D12_BLEND_INV_BLEND_FACTOR	= 15,
                            D3D12_BLEND_SRC1_COLOR	= 16,
                            D3D12_BLEND_INV_SRC1_COLOR	= 17,
                            D3D12_BLEND_SRC1_ALPHA	= 18,
                            D3D12_BLEND_INV_SRC1_ALPHA	= 19
                        } 	D3D12_BLEND;

                    typedef 
                    enum D3D12_BLEND_OP
                        {
                            D3D12_BLEND_OP_ADD	= 1,
                            D3D12_BLEND_OP_SUBTRACT	= 2,
                            D3D12_BLEND_OP_REV_SUBTRACT	= 3,
                            D3D12_BLEND_OP_MIN	= 4,
                            D3D12_BLEND_OP_MAX	= 5
                        } 	D3D12_BLEND_OP;

                    typedef 
                    enum D3D12_COLOR_WRITE_ENABLE
                        {
                            D3D12_COLOR_WRITE_ENABLE_RED	= 1,
                            D3D12_COLOR_WRITE_ENABLE_GREEN	= 2,
                            D3D12_COLOR_WRITE_ENABLE_BLUE	= 4,
                            D3D12_COLOR_WRITE_ENABLE_ALPHA	= 8,
                            D3D12_COLOR_WRITE_ENABLE_ALL	= ( ( ( D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN )  | D3D12_COLOR_WRITE_ENABLE_BLUE )  | D3D12_COLOR_WRITE_ENABLE_ALPHA ) 
                        } 	D3D12_COLOR_WRITE_ENABLE;

                    
                    
                    */

                attachmentState.SrcBlend = VdToD3DBlendFactor(vdDesc.sourceAlphaFactor);
                attachmentState.DestBlend = VdToD3DBlendFactor(vdDesc.destinationColorFactor);
                attachmentState.BlendOp = VdToD3DBlendOp(vdDesc.colorFunction);
                attachmentState.SrcBlendAlpha = VdToD3DBlendFactor(vdDesc.sourceAlphaFactor);
                attachmentState.DestBlendAlpha = VdToD3DBlendFactor(vdDesc.destinationAlphaFactor);
                attachmentState.BlendOpAlpha = VdToD3DBlendOp(vdDesc.alphaFunction);
                attachmentState.RenderTargetWriteMask = VdToD3DColorWriteMask(vdDesc.colorWriteMask);
                attachmentState.BlendEnable = vdDesc.blendEnabled;
            }

            blendState.AlphaToCoverageEnable = desc.blendState.alphaToCoverageEnabled;
       
        }
        /*        
        // Rasterizer State
        auto& rsDesc = desc.rasterizerState;
        VkPipelineRasterizationStateCreateInfo rsCI{};
        rsCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rsCI.cullMode = VdToVkCullMode(rsDesc.cullMode);
        rsCI.polygonMode = VdToVkPolygonMode(rsDesc.fillMode);
        rsCI.depthClampEnable = !rsDesc.depthClipEnabled;
        rsCI.frontFace = rsDesc.frontFace == RasterizerStateDescription::FrontFace::Clockwise
            ? VkFrontFace::VK_FRONT_FACE_CLOCKWISE 
            : VkFrontFace::VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rsCI.lineWidth = 1.f;
        pipelineCI.pRasterizationState = &rsCI;
*/
        {
            auto& rsDesc = desc.rasterizerState;
            auto& rsCI = psoDesc.RasterizerState;
            /*
            typedef struct D3D12_RASTERIZER_DESC
            {
            D3D12_FILL_MODE FillMode;
            D3D12_CULL_MODE CullMode;
            BOOL FrontCounterClockwise;
            INT DepthBias;
            FLOAT DepthBiasClamp;
            FLOAT SlopeScaledDepthBias;
            BOOL DepthClipEnable;
            BOOL MultisampleEnable;
            BOOL AntialiasedLineEnable;
            UINT ForcedSampleCount;
            D3D12_CONSERVATIVE_RASTERIZATION_MODE ConservativeRaster;
            } 	D3D12_RASTERIZER_DESC;*/

            rsCI.CullMode = VdToD3DCullMode(rsDesc.cullMode);
            rsCI.FillMode = VdToD3DPolygonMode(rsDesc.fillMode);
            rsCI.DepthClipEnable = rsDesc.depthClipEnabled;
            rsCI.FrontCounterClockwise = rsDesc.frontFace != RasterizerStateDescription::FrontFace::Clockwise;

        }
            /*
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

        pipelineCI.pDepthStencilState = &dssCI;*/
        {
            auto& vdDssDesc = desc.depthStencilState;
            auto& dssState = psoDesc.DepthStencilState;
            

            dssState.DepthEnable = vdDssDesc.depthTestEnabled;
            dssState.DepthFunc = VdToD3DCompareOp(vdDssDesc.depthComparison);
            dssState.DepthWriteMask = vdDssDesc.depthWriteEnabled 
                        ? D3D12_DEPTH_WRITE_MASK::D3D12_DEPTH_WRITE_MASK_ALL
                        : D3D12_DEPTH_WRITE_MASK::D3D12_DEPTH_WRITE_MASK_ZERO;

            dssState.StencilEnable = vdDssDesc.stencilTestEnabled;
            dssState.StencilReadMask = vdDssDesc.stencilReadMask;
            dssState.StencilWriteMask = vdDssDesc.stencilWriteMask;

            
            dssState.FrontFace.StencilFailOp = VdToD3DStencilOp(vdDssDesc.stencilFront.fail);
            dssState.FrontFace.StencilPassOp = VdToD3DStencilOp(vdDssDesc.stencilFront.pass);
            dssState.FrontFace.StencilDepthFailOp = VdToD3DStencilOp(vdDssDesc.stencilFront.depthFail);
            dssState.FrontFace.StencilFunc = VdToD3DCompareOp(vdDssDesc.stencilFront.comparison);

            dssState.BackFace.StencilFailOp = VdToD3DStencilOp(vdDssDesc.stencilBack.fail);
            dssState.BackFace.StencilPassOp = VdToD3DStencilOp(vdDssDesc.stencilBack.pass);
            dssState.BackFace.StencilDepthFailOp = VdToD3DStencilOp(vdDssDesc.stencilBack.depthFail);
            dssState.BackFace.StencilFunc = VdToD3DCompareOp(vdDssDesc.stencilBack.comparison);
        }
        
        
        /*

        // Multisample
        VkPipelineMultisampleStateCreateInfo multisampleCI{};
        multisampleCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        VkSampleCountFlagBits vkSampleCount = VdToVkSampleCount(desc.outputs.sampleCount);
        multisampleCI.rasterizationSamples = vkSampleCount;
        multisampleCI.alphaToCoverageEnable = desc.blendState.alphaToCoverageEnabled;
        pipelineCI.pMultisampleState = &multisampleCI;
*/
        {
            psoDesc.SampleMask = 0xffffffff; //This has to do with multi-sampling. 0xffffffff means point sampling is used. 
            auto& msState = psoDesc.SampleDesc;
            msState.Count = (std::uint8_t)desc.outputs.sampleCount;
            msState.Quality = 0;//TODO: Query quality support for device:
            //D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msLevels;
            //msLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Replace with your render target format.
            //msLevels.SampleCount = 4; // Replace with your sample count.
            //msLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
            //
            //m_Device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msLevels, sizeof(msLevels));
        }

/*
        // Input Assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{};
        inputAssemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyCI.topology = VdToVkPrimitiveTopology(desc.primitiveTopology);
        inputAssemblyCI.primitiveRestartEnable = VK_FALSE;
        pipelineCI.pInputAssemblyState = &
*/
        {
            auto& topologyState = psoDesc.PrimitiveTopologyType;
            topologyState = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        }

/*
        // Vertex Input State
        VkPipelineVertexInputStateCreateInfo vertexInputCI{};
        vertexInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
*/
        
        auto& inputDescriptions = desc.shaderSet.vertexLayouts;
        auto bindingCount = inputDescriptions.size();
        unsigned attributeCount = 0;
        for (int i = 0; i < inputDescriptions.size(); i++)
        {
            attributeCount += inputDescriptions[i].elements.size();
        }

        std::vector<D3D12_INPUT_ELEMENT_DESC> iaDescs(attributeCount);
/*
            typedef struct D3D12_INPUT_ELEMENT_DESC
    {
    LPCSTR SemanticName;
    UINT SemanticIndex;
    DXGI_FORMAT Format;
    UINT InputSlot;
    UINT AlignedByteOffset;
    D3D12_INPUT_CLASSIFICATION InputSlotClass;
    UINT InstanceDataStepRate;
    } 	D3D12_INPUT_ELEMENT_DESC;
*/
            

        //std::vector<VkVertexInputBindingDescription> bindingDescs(bindingCount);
        //std::vector<VkVertexInputAttributeDescription> attributeDescs(attributeCount);

        int targetIndex = 0;
        int targetLocation = 0;
        for (int binding = 0; binding < inputDescriptions.size(); binding++)
        {
            auto& inputDesc = inputDescriptions[binding];
            //bindingDescs[binding].binding = binding;
            //bindingDescs[binding].inputRate = (inputDesc.instanceStepRate != 0) 
            //    ? VkVertexInputRate::VK_VERTEX_INPUT_RATE_INSTANCE 
            //    : VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;
            //bindingDescs[binding].stride = inputDesc.stride;
            
            unsigned currentOffset = 0;
            for (int location = 0; location < inputDesc.elements.size(); location++)
            {
                auto& inputElement = inputDesc.elements[location];

                iaDescs[targetIndex]./*LPCSTR*/ SemanticName = inputElement.name.c_str();
                iaDescs[targetIndex]./*UINT*/ SemanticIndex = 0;
                iaDescs[targetIndex]./*DXGI_FORMAT*/ Format = VdToD3DShaderDataType(inputElement.format);
                iaDescs[targetIndex]./*UINT*/ InputSlot = binding;
                iaDescs[targetIndex]./*UINT*/ AlignedByteOffset = inputElement.offset != 0 
                                                                ? inputElement.offset 
                                                                : currentOffset;
                //TODO: [DXC, Vk] Support instanced draw?
                iaDescs[targetIndex]./*D3D12_INPUT_CLASSIFICATION*/ InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                iaDescs[targetIndex]./*UINT*/ InstanceDataStepRate = 0;
                
                targetIndex += 1;
                currentOffset += FormatHelpers::GetSizeInBytes(inputElement.format);
            }

            targetLocation += inputDesc.elements.size();
        }

        auto& viState = psoDesc.InputLayout;
        viState.NumElements = iaDescs.size();
        viState.pInputElementDescs = iaDescs.data();
        


/*
        // Shader Stage

        VkSpecializationInfo specializationInfo{};
        auto& specDescs = desc.shaderSet.specializations;
        if (!specDescs.empty())
        {
            unsigned specDataSize = 0;
            for (auto& spec : specDescs) {
                specDataSize += GetSpecializationConstantSize(spec.type);
            }
            std::vector<std::uint8_t> fullSpecData(specDataSize);
            int specializationCount = specDescs.size();
            std::vector<VkSpecializationMapEntry> mapEntries(specializationCount);
            unsigned specOffset = 0;
            for (int i = 0; i < specializationCount; i++)
            {
                auto data = specDescs[i].data;
                auto srcData = (byte*)&data;
                auto dataSize = GetSpecializationConstantSize(specDescs[i].type);
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

        auto& shaders = desc.shaderSet.shaders;
        std::vector<VkPipelineShaderStageCreateInfo> stageCIs(shaders.size());
        for (unsigned i = 0; i < shaders.size(); i++){
            auto& shader = shaders[i];
            auto vkShader = reinterpret_cast<VulkanShader*>(shader.get());
            auto& stageCI = stageCIs[i];
            stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageCI.module = vkShader->GetHandle();
            stageCI.stage = VdToVkShaderStageSingle(shader->GetDesc().stage);
            // stageCI.pName = CommonStrings.main; // Meh
            stageCI.pName = shader->GetDesc().entryPoint.c_str(); // TODO: DONT ALLOCATE HERE
            stageCI.pSpecializationInfo = &specializationInfo;
        }

        pipelineCI.stageCount = stageCIs.size();
        pipelineCI.pStages = stageCIs.data();

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
        }
        pipelineLayoutCI.pSetLayouts = dsls.data();
        
        VkPipelineLayout pipelineLayout;
        VK_CHECK(vkCreatePipelineLayout(dev->LogicalDev(), &pipelineLayoutCI, nullptr, &pipelineLayout));
        pipelineCI.layout = pipelineLayout;
        
        // Create fake RenderPass for compatibility.
        auto& outputDesc = desc.outputs;
        
        //auto compatRenderPass = CreateFakeRenderPassForCompat(dev.get(), outputDesc, VK_SAMPLE_COUNT_1_BIT);
        auto compatRenderPass = CreateFakeRenderPassForCompat(dev.get(), outputDesc, vkSampleCount);
        */

        //Output formats
        auto color_count = desc.outputs.colorAttachment.size();
        if (color_count > 0) {
            

           psoDesc.NumRenderTargets = color_count;
           for (uint32_t i = 0; i < color_count; i++) {
              psoDesc.RTVFormats[i] = VdToD3DPixelFormat(desc.outputs.colorAttachment[i].format, false);
           }
        }

        if (desc.outputs.depthAttachment.has_value()) {
            psoDesc.DSVFormat = VdToD3DPixelFormat(desc.outputs.depthAttachment.value().format, true);
        }

        //pipeline->multiview.view_mask = MAX2(view_mask, 1);
        //if (view_mask != 0 && /* Is multiview */
        //    view_mask != 1 && /* Is non-trivially multiview */
        //    (view_mask & ~((1 << D3D12_MAX_VIEW_INSTANCE_COUNT) - 1)) == 0 && /* Uses only views 0 thru 3 */
        //    pdev->options3.ViewInstancingTier > D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED /* Actually supported */) {
        //   d3d12_gfx_pipeline_state_stream_new_desc(stream_desc, VIEW_INSTANCING, D3D12_VIEW_INSTANCING_DESC, vi);
        //   vi->pViewInstanceLocations = vi_locs;
        //   for (uint32_t i = 0; i < D3D12_MAX_VIEW_INSTANCE_COUNT; ++i) {
        //      vi_locs[i].RenderTargetArrayIndex = i;
        //      vi_locs[i].ViewportArrayIndex = 0;
        //      if (view_mask & (1 << i))
        //         vi->ViewInstanceCount = i + 1;
        //   }
        //   vi->Flags = D3D12_VIEW_INSTANCING_FLAG_ENABLE_VIEW_INSTANCE_MASKING;
        //   pipeline->multiview.native_view_instancing = true;
        //}
       
       
       /*
        
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
        }*/

        
        ComPtr<ID3D12PipelineState> pipelineState;
        if(FAILED(dev->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)))){
            return nullptr;
        }

        auto rawPipe = new DXCGraphicsPipeline(dev, desc);
        rawPipe->_pso = std::move(pipelineState);
        switch (desc.primitiveTopology)
        {
            // A list of isolated, 3-element triangles.
            case PrimitiveTopology::TriangleList: rawPipe->_primTopo = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
            // A series of connected triangles.
            case PrimitiveTopology::TriangleStrip: rawPipe->_primTopo = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
            // A series of isolated, 2-element line segments.
            case PrimitiveTopology::LineList: rawPipe->_primTopo = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
            // A series of connected line segments.
            case PrimitiveTopology::LineStrip: rawPipe->_primTopo = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
            // A series of isolated points.
            case PrimitiveTopology::PointList: rawPipe->_primTopo = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
        }
        //rawPipe->_devicePipeline = devicePipeline;
        //rawPipe->_pipelineLayout = pipelineLayout;
        //rawPipe->_renderPass = compatRenderPass;
        //rawPipe->scissorTestEnabled = rsDesc.scissorTestEnabled;
        //rawPipe->resourceSetCount = resourceSetCount;
        //rawPipe->dynamicOffsetsCount = dynamicOffsetsCount;

        //For BlendFactor::BlendFactor: 
        //The blend factor is the blend factor set with 
        //ID3D12GraphicsCommandList::OMSetBlendFactor. No pre-blend operation.

        /*void OMSetBlendFactor(
            [in, optional] const FLOAT [4] BlendFactor
        );*/
        rawPipe->_blendConstants[0] = desc.blendState.blendConstant.r;
        rawPipe->_blendConstants[1] = desc.blendState.blendConstant.g;
        rawPipe->_blendConstants[2] = desc.blendState.blendConstant.b;
        rawPipe->_blendConstants[3] = desc.blendState.blendConstant.a;
        
        rawPipe->_refCnts = std::move(refCnts);

        dxcResLayout->ref();
        rawPipe->_rootSig = common::sp(dxcResLayout);

        return common::sp(rawPipe);
    }

    
    void DXCGraphicsPipeline::CmdBindPipeline(ID3D12GraphicsCommandList* pCmdList) {

        pCmdList->SetPipelineState(_pso.Get());

        pCmdList->SetGraphicsRootSignature(_rootSig->GetHandle());

        pCmdList->IASetPrimitiveTopology(_primTopo);
        pCmdList->OMSetBlendFactor(_blendConstants);
    }


    DXCComputePipeline::~DXCComputePipeline() {

    }

    common::sp<IComputePipeline> DXCComputePipeline::Make(
        const common::sp<DXCDevice>& dev,
        const ComputePipelineDescription& desc
    ) {
        assert(false);

        
        // Describe and create the graphics pipeline state object (PSO).
        std::unordered_set<common::sp<RefCntBase>> refCnts;
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc {};

        auto dxcShader = static_cast<DXCShader*>(desc.computeShader.get());
        psoDesc.CS.BytecodeLength = dxcShader->GetDataSizeInBytes();
        psoDesc.CS.pShaderBytecode = dxcShader->GetData();
        refCnts.insert(desc.computeShader);        

        auto dxcResLayout = PtrCast<DXCResourceLayout>(desc.resourceLayout.get());

        psoDesc.pRootSignature = dxcResLayout->GetHandle();

        ComPtr<ID3D12PipelineState> pipelineState;
        ThrowIfFailed(dev->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));

        
        auto rawPipe = new DXCComputePipeline(dev, desc);
        rawPipe->_pso = std::move(pipelineState);
        rawPipe->_refCnts = std::move(refCnts);
        
        dxcResLayout->ref();
        rawPipe->_rootSig = common::sp(dxcResLayout);
        
        return common::sp(rawPipe);
    }

    void DXCComputePipeline::CmdBindPipeline(ID3D12GraphicsCommandList* pCmdList) {

        pCmdList->SetPipelineState(_pso.Get());
        //
        pCmdList->SetComputeRootSignature(_rootSig->GetHandle());
        //
        //pCmdList->IASetPrimitiveTopology(_primTopo);
        //pCmdList->OMSetBlendFactor(_blendConstants);
    }

} // namespace alloy
