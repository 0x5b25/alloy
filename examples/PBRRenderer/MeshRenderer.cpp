#include "MeshRenderer.hpp"

#include "HLSLCompiler.hpp"

#include <iostream>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace PBRObjectShader {
    #include "Shaders/PBRObjectShader_ps.h"
    #include "Shaders/PBRObjectShader_vs.h"
}

struct UniformBufferObject{
    glm::vec4 cameraPos;
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

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
    auto& factory = _dev->GetResourceFactory();
    alloy::IShader::Description vertexShaderDesc{};
    vertexShaderDesc.stage = alloy::IShader::Stage::Vertex;
    vertexShaderDesc.entryPoint = "VSMain";
    alloy::IShader::Description fragmentShaderDesc{};
    fragmentShaderDesc.stage = alloy::IShader::Stage::Fragment;
    fragmentShaderDesc.entryPoint = "PSMain";

    
    auto fragmentShader = factory.CreateShader(fragmentShaderDesc, PBRObjectShader::g_PSMain);
    auto vertexShader = factory.CreateShader(vertexShaderDesc, PBRObjectShader::g_VSMain);


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
