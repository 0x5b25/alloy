#include "MeshRenderer.hpp"

#include "HLSLCompiler.hpp"

#include <iostream>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Scene.hpp"

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
    : _scene(args.scene)
    , _dev(args.device)
    , _rtFormat(args.renderTargetFormat)
    , _dsFormat(args.depthStencilFormat)
    , _msaaSampleCnt(args.msaaSampleCount)
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


    //alloy::IResourceSet::Description resSetDesc{};
    //resSetDesc.layout = _layout;
    //resSetDesc.boundResources = {
    //    alloy::BufferRange::MakeByteBuffer(uniformBuffer), 
    //    alloy::BufferRange::MakeByteBuffer(structBuffer)
    //};
    //shaderResources = factory.CreateResourceSet(resSetDesc);

    //auto outputDesc = swapChain->GetBackBuffer()->GetDesc();
    
    alloy::GraphicsPipelineDescription pipelineDescription{};
    pipelineDescription.resourceLayout = _scene.GetResourceLayout();
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
    //pipelineDescription.shaderSet.vertexLayouts[0].SetElements({
    //    {"POSITION", {alloy::VertexInputSemantic::Name::Position, 0},          alloy::ShaderDataType::Float3},
    //    {"TEXCOORD", {alloy::VertexInputSemantic::Name::TextureCoordinate, 0}, alloy::ShaderDataType::Float2},
    //    {"NORMAL",   {alloy::VertexInputSemantic::Name::Normal, 0},            alloy::ShaderDataType::Float3},
    //    {"TANGENT",  {alloy::VertexInputSemantic::Name::Tangent, 0},           alloy::ShaderDataType::Float3},
    //    {"BINORMAL", {alloy::VertexInputSemantic::Name::Binormal, 0},          alloy::ShaderDataType::Float3},
    //    });
    pipelineDescription.shaderSet.vertexShader = vertexShader;
    pipelineDescription.shaderSet.fragmentShader = fragmentShader;

    //pipelineDescription.outputs = swapChain->GetBackBuffer()->GetDesc();
    //pipelineDescription.outputs = fb->GetOutputDescription();
    _objRenderPipeline = factory.CreateGraphicsPipeline(pipelineDescription);
}

void MeshRenderer::DrawScene(alloy::IRenderCommandEncoder* rndPass, const Viewport& vp) {

    _scene.UpdateGPUScene();

    std::vector<uint32_t> ubo{};
    ubo.resize(sizeof(SceneDescriptor) / 4);

    auto pubo = (SceneDescriptor*)ubo.data();

    pubo->skyBoxLightColor = glm::vec4(0.992, 0.984, 0.827, 10);
    pubo->skyBoxLightDir = glm::normalize(glm::vec4(-1.0, -1.0, -1.0, 1.0));

    pubo->cameraPos = glm::vec4(vp.position, 1.f);
    pubo->view = glm::lookAt(
        vp.position,
        vp.position + vp.front,
        vp.up);
    pubo->proj = glm::perspective(
        glm::radians(vp.fovDeg), //glm::radians(45.0f), 
        vp.aspectRatio, //swapChain->GetWidth() / (float)swapChain->GetHeight(),
        vp.nearClip, vp.farClip);

    rndPass->SetPipeline(_objRenderPipeline);
    rndPass->SetPushConstants(0, ubo, 0);
    rndPass->SetGraphicsResourceSet(_scene.GetResourceSet());
    rndPass->SetFullViewports();
    rndPass->SetFullScissorRects();

    //Construct MVP matrices

    //rndPass->SetVertexBuffer(0, alloy::BufferRange::MakeByteBuffer(_scene.GetVertexBuffer()));
    //rndPass->SetIndexBuffer(alloy::BufferRange::MakeByteBuffer(_scene.GetIndexBuffer()), alloy::IndexFormat::UInt32);

    rndPass->Draw(_scene.GetVertexCount());

}

