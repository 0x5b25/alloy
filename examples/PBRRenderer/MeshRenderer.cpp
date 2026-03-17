#include "MeshRenderer.hpp"

#include "HLSLCompiler.hpp"

#include <iostream>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


struct UniformBufferObject{
    glm::vec4 cameraPos;
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

const static std::string HLSLCode = R"AGAN(
//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************


struct SceneDescriptor
{
    float4 offset;
    float4 padding[15];
};
SceneDescriptor sceneDesc : register(t0);

struct UniformBufferObject{
    float4 cameraPos;
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBufferObject> ubo  : register(b0);

struct Vertex {
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
    float3 normal   : NORMAL ;
    float3 tangent  : TANGENT;
    float3 bitangent: BINORMAL;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal   : NORMAL;
    float2 uv     : COLOR0;
    float4 color  : COLOR1;
};

PSInput VSMain(Vertex input)
{
    PSInput result;

    result.worldPos = mul(ubo.model, float4(input.position, 1.0)).xyz;
    result.position = mul(ubo.proj, mul(ubo.view, float4(result.worldPos, 1.0)));
    result.uv = input.texCoord;
    result.color = float4(0.5, 0.5, 0.5, 1.0);
    result.normal = input.normal;

    //result.position.w = 1;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 cameraPos = ubo.cameraPos.xyz;
    float3 cameraDir = normalize(cameraPos - input.worldPos);
    float intensity = dot(cameraDir, input.normal);
    float3 plainColor = input.color.xyz * intensity;
    return float4(plainColor, 1);
    //return input.uv;
}
)AGAN";


MeshRenderer::MeshRenderer(const CreateArgs& args)
    : _dev(args.device)
    , _rtFormat(args.renderTargetFormat)
    , _dsFormat(args.depthStencilFormat)
    , _msaaSampleCnt(args.msaaSampleCount)
    , _currRndPass(nullptr)
{
    _CreateObjRenderPipeline();
}

MeshRenderer::~MeshRenderer() {

}

void MeshRenderer::_CreateObjRenderPipeline() {
    
    /*****************
     * Compile shaders
     ******************/
    auto pComplier = IHLSLCompiler::Create();

    //auto spvCompiler = alloy::IGLSLCompiler::Get();
    std::string compileInfo;

    std::cout << "Compiling vertex shader...\n";
    std::vector<std::uint8_t> vertexSpv;
    
    std::vector<std::uint8_t> fragSpv;

    try {
        IHLSLCompiler::CompileOptions opts {
            .type = ShaderType::VS,
            .keepDebugInfo = true,
        };
        vertexSpv = pComplier->Compile(HLSLCode, "VSMain", opts);
    } catch (const ShaderCompileError& e){
        std::cout << "Vertex shader compilation failed\n";
        std::cout << "    ERROR: " << e.what() << std::endl;
        throw e;
    }

    try {
        IHLSLCompiler::CompileOptions opts {
            .type = ShaderType::PS,
            .keepDebugInfo = true,
        };
        fragSpv = pComplier->Compile(HLSLCode, "PSMain", opts);
    } catch (const ShaderCompileError& e){
        std::cout << "Fragment shader compilation failed\n";
        std::cout << "    ERROR: " << e.what() << std::endl;
        throw e;
    }
    auto& factory = _dev->GetResourceFactory();
    alloy::IShader::Description vertexShaderDesc{};
    vertexShaderDesc.stage = alloy::IShader::Stage::Vertex;
    vertexShaderDesc.entryPoint = "VSMain";
    alloy::IShader::Description fragmentShaderDesc{};
    fragmentShaderDesc.stage = alloy::IShader::Stage::Fragment;
    fragmentShaderDesc.entryPoint = "PSMain";

    auto fragmentShader = factory.CreateShader(fragmentShaderDesc, {(uint8_t*)fragSpv.data(), fragSpv.size()});
    auto vertexShader = factory.CreateShader(vertexShaderDesc, {(uint8_t*)vertexSpv.data(), vertexSpv.size()});


    /**********************
     * Create Pipeline
     */
    
    alloy::IResourceLayout::Description resLayoutDesc{};
    using ElemKind = alloy::IBindableResource::ResourceKind;
    using alloy::common::operator|;

    {
        auto& pc = resLayoutDesc.pushConstants.emplace_back();
        pc.bindingSlot = 0;
        pc.bindingSpace = 0;
        pc.sizeInDwords = (sizeof(UniformBufferObject) + 3) / 4;
    }
#if 0
    {
        auto& elem = resLayoutDesc.shaderResources.emplace_back();
        //resLayoutDesc.shaderResources[0].name = "ObjectUniform";
        elem.kind = ElemKind::UniformBuffer;
        elem.stages = alloy::IShader::Stage::Vertex | alloy::IShader::Stage::Fragment;
    }

    {
        auto& elem = resLayoutDesc.shaderResources.emplace_back();
        //resLayoutDesc.shaderResources[1].name = "Struct";
        elem.kind = ElemKind::StorageBuffer;
        elem.stages = alloy::IShader::Stage::Vertex | alloy::IShader::Stage::Fragment;
    }
#endif
    _objRenderLayout = factory.CreateResourceLayout(resLayoutDesc);

    //alloy::IResourceSet::Description resSetDesc{};
    //resSetDesc.layout = _layout;
    //resSetDesc.boundResources = {
    //    alloy::BufferRange::MakeByteBuffer(uniformBuffer), 
    //    alloy::BufferRange::MakeByteBuffer(structBuffer)
    //};
    //shaderResources = factory.CreateResourceSet(resSetDesc);

    //auto outputDesc = swapChain->GetBackBuffer()->GetDesc();
    
    alloy::GraphicsPipelineDescription pipelineDescription{};
    pipelineDescription.resourceLayout = _objRenderLayout;
    pipelineDescription.attachmentState = {};
    pipelineDescription.attachmentState.colorAttachments.push_back(
        alloy::AttachmentStateDescription::ColorAttachment::MakeAlphaBlend()
    );

    pipelineDescription.attachmentState.colorAttachments.front().format = _rtFormat;
    
    {
        alloy::AttachmentStateDescription::DepthStencilAttachment dsAttachment {};
        dsAttachment.depthStencilFormat = _dsFormat;
        
        pipelineDescription.attachmentState.depthStencilAttachment = dsAttachment;
    }//pipelineDescription.blendState.attachments[0].blendEnabled = true;
    pipelineDescription.attachmentState.sampleCount = _msaaSampleCnt;

    pipelineDescription.depthStencilState.depthTestEnabled = true;
    pipelineDescription.depthStencilState.depthWriteEnabled = true;
    pipelineDescription.depthStencilState.depthComparison = alloy::ComparisonKind::LessEqual;


    pipelineDescription.rasterizerState.cullMode = alloy::RasterizerStateDescription::FaceCullMode::Back;
    pipelineDescription.rasterizerState.fillMode = alloy::RasterizerStateDescription::PolygonFillMode::Solid;
    pipelineDescription.rasterizerState.frontFace = alloy::RasterizerStateDescription::FrontFace::CounterClockwise;
    pipelineDescription.rasterizerState.depthClipEnabled = true;
    pipelineDescription.rasterizerState.scissorTestEnabled = false;

    pipelineDescription.primitiveTopology = alloy::PrimitiveTopology::TriangleList;

    using VL = alloy::VertexLayout;
    pipelineDescription.shaderSet.vertexLayouts = { {} };

    //struct Vertex {
    //    float3 position : POSITION;
    //    float2 texCoord : TEXCOORD;
    //    float3 normal   : NORMAL ;
    //    float3 tangent  : TANGENT;
    //    float3 bitangent: BINORMAL;
    //};
    pipelineDescription.shaderSet.vertexLayouts[0].SetElements({
        {"POSITION", {alloy::VertexInputSemantic::Name::Position, 0},          alloy::ShaderDataType::Float3},
        {"TEXCOORD", {alloy::VertexInputSemantic::Name::TextureCoordinate, 0}, alloy::ShaderDataType::Float2},
        {"NORMAL",   {alloy::VertexInputSemantic::Name::Normal, 0},            alloy::ShaderDataType::Float3},
        {"TANGENT",  {alloy::VertexInputSemantic::Name::Tangent, 0},           alloy::ShaderDataType::Float3},
        {"BINORMAL", {alloy::VertexInputSemantic::Name::Binormal, 0},          alloy::ShaderDataType::Float3},
        });
    pipelineDescription.shaderSet.vertexShader = vertexShader;
    pipelineDescription.shaderSet.fragmentShader = fragmentShader;

    //pipelineDescription.outputs = swapChain->GetBackBuffer()->GetDesc();
    //pipelineDescription.outputs = fb->GetOutputDescription();
    _objRenderPipeline = factory.CreateGraphicsPipeline(pipelineDescription);
}

void MeshRenderer::BeginRendering(alloy::IRenderCommandEncoder* rndPass, const Viewport& vp) {
    assert(_currRndPass == nullptr);//Previous BeginRendering call not finished.
    _currRndPass = rndPass;

    std::vector<uint32_t> ubo{};
    ubo.resize(sizeof(UniformBufferObject) / 4);

    auto pubo = (UniformBufferObject*)ubo.data();
    pubo->cameraPos = glm::vec4(vp.position, 1.f);
    pubo->model = glm::mat4(1);
    pubo->view = glm::lookAt(
        vp.position,
        vp.position + vp.front,
        vp.up);
    pubo->proj = glm::perspective(
        glm::radians(vp.fovDeg), //glm::radians(45.0f), 
        vp.aspectRatio, //swapChain->GetWidth() / (float)swapChain->GetHeight(),
        vp.nearClip, vp.farClip);

    _currRndPass->SetPipeline(_objRenderPipeline);
    _currRndPass->SetPushConstants(0, ubo, 0);
    _currRndPass->SetFullViewports();
    _currRndPass->SetFullScissorRects();

    //Construct MVP matrices

}

void MeshRenderer::DrawMesh(const Mesh& mesh) {
    auto vertSizeInBytes = mesh.vertices.size() * sizeof(Vertex);
    alloy::IBuffer::Description vertBufferDesc {};
    vertBufferDesc.sizeInBytes = vertSizeInBytes;
    vertBufferDesc.hostAccess = alloy::HostAccess::PreferDeviceMemory;
    vertBufferDesc.usage.vertexBuffer = 1;
    auto vertBuffer = _dev->GetResourceFactory().CreateBuffer(vertBufferDesc);

    auto dst = vertBuffer->MapToCPU();
    memcpy(dst, mesh.vertices.data(), vertSizeInBytes);
    vertBuffer->UnMap();

    _currRndPass->SetVertexBuffer(0, alloy::BufferRange::MakeByteBuffer(vertBuffer));


    //Modify model transform matrtix;
    
    std::vector<uint32_t> modelMat{};
    modelMat.resize(sizeof(UniformBufferObject::model) / 4);

    auto pModelMat = (glm::mat4*)modelMat.data();

    *pModelMat = glm::mat4(1);
    //(*pModelMat)[2][2] = -1;

    constexpr auto pcOff = offsetof(UniformBufferObject, model) / 4;

    _currRndPass->SetPushConstants(0, modelMat, pcOff);

    _currRndPass->Draw(mesh.vertices.size());

}

void MeshRenderer::EndRendering() {
    _currRndPass = nullptr;
}
