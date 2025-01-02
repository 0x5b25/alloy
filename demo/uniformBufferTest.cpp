#include <veldrid/backend/Backends.hpp>
#include <veldrid/SwapChainSources.hpp>
#include <veldrid/SwapChain.hpp>
#include <veldrid/BindableResource.hpp>
#include <veldrid/GraphicsDevice.hpp>
#include <veldrid/CommandQueue.hpp>
#include <veldrid/ResourceFactory.hpp>
#include <veldrid/Buffer.hpp>
#include <veldrid/CommandList.hpp>
#include <veldrid/Pipeline.hpp>
#include <veldrid/Shader.hpp>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <string>
#include <iostream>
#include <type_traits>

#include "app/App.hpp"
#include "app/HLSLCompiler.hpp"

struct VertexData
{
    float position[2]; // This is the position, in normalized device coordinates.
    float uv[2];
    float color[4]; // This is the color of the vertex.
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

const std::string VertexCode = R"(
#version 450

layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 UV;
layout(location = 2) in vec4 Color;

layout(location = 0) out vec4 fsin_Color;
layout(location = 1) out vec2 uvCoord;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(binding = 1) uniform StructBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} sbo;

void main()
{
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(Position, 0.0, 1.0);
    fsin_Color = Color;
    uvCoord = UV;
}
)";

const std::string FragmentCode = R"(
#version 450

layout(location = 0) in vec4 fsin_Color;
layout(location = 1) in vec2 uvCoord;
layout(location = 0) out vec4 fsout_Color;

void main()
{
    fsout_Color = fsin_Color;
    //fsout_Color = vec4(1,1,1,1);
}
//layout(location = 0) out vec4 outColor;
//
//void main() {
//    outColor = vec4(1.0, 0.0, 0.0, 1.0);
//}
)";

const wchar_t HLSLCode[] = LR"AGAN(
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
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBufferObject> ubo  : register(b0);

struct PSInput
{
    float4 position : SV_POSITION;
    float4 uv : COLOR0;
    float4 color : COLOR1;
};

PSInput VSMain(float4 position : POSITION, float4 uv : TEXCOORD, float4 color : COLOR)
{
    PSInput result;

    //result.position = ubo.proj * ubo.view * ubo.model * float4(position.xy, 0.0, 1.0);
    result.position = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(position.xy, 0.0, 1.0))));
    result.color = color;
    //result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
    //return input.uv;
}
)AGAN";

using namespace alloy;

class UniformApp : public AppBase {

    Veldrid::sp<Veldrid::GraphicsDevice> dev;
    Veldrid::sp<Veldrid::SwapChain> swapChain;

    Veldrid::sp<Veldrid::Shader> fragmentShader, vertexShader;
    Veldrid::sp<Veldrid::Buffer> vertexBuffer, indexBuffer;
    Veldrid::sp<Veldrid::Buffer> uniformBuffer, structBuffer;

    Veldrid::sp<Veldrid::ResourceSet> shaderResources;

    Veldrid::sp<Veldrid::Pipeline> pipeline;

    Veldrid::sp<Veldrid::Fence> renderFinishFence;

    Veldrid::sp<Veldrid::CommandList> cmd;

    uint64_t renderFinishFenceValue = 0;
    bool _isFirstSubmission = true;

    template<typename T>
    void UpdateBuffer(
        const Veldrid::sp<Veldrid::Buffer>& buffer,
        const std::vector<T>& data
    ) {
        const unsigned stagingBufferSizeInBytes = 512;
        const auto transferSizeInBytes = sizeof(T) * data.size();

        Veldrid::Buffer::Description desc{};
        desc.sizeInBytes = stagingBufferSizeInBytes;
        desc.hostAccess = Veldrid::HostAccess::PreferWrite;
        //desc.usage.staging = 1;
        auto transferBuffer = dev->GetResourceFactory()->CreateBuffer(desc);

        auto fence = dev->GetResourceFactory()->CreateFence();
        uint64_t signalValue = 1;

        unsigned remainingSize = transferSizeInBytes;
        auto readPtr = (const std::uint8_t*)data.data();
        auto writePtr = transferBuffer->MapToCPU();
        assert(writePtr != nullptr);

        while (remainingSize > 0)
        {
            //prepare the tranfer buffer
            auto transferedSize = transferSizeInBytes - remainingSize;
            auto batchSize = std::min(stagingBufferSizeInBytes, remainingSize);
            memcpy(writePtr, readPtr + transferedSize, batchSize);
            
            //Record the command buffer
            auto cmd = dev->GetResourceFactory()->CreateCommandList();
            cmd->Begin();
            cmd->CopyBuffer(transferBuffer, 0, buffer, transferedSize, batchSize);
            cmd->End();

            //submit and wait
            dev->GetGfxCommandQueue()->SubmitCommand(cmd.get());
            dev->GetGfxCommandQueue()->EncodeSignalFence(fence.get(), signalValue);
            fence->WaitFromCPU(signalValue);
            signalValue++;

            //Advance counters
            remainingSize -= batchSize;
        }

        transferBuffer->UnMap();

    }

    void CreateSwapChain(
        Veldrid::SwapChainSource* swapChainSrc,
        unsigned surfaceWidth,
        unsigned surfaceHeight
    ) {



        Veldrid::SwapChain::Description swapChainDesc{};
        swapChainDesc.source = swapChainSrc;
        swapChainDesc.initialWidth = surfaceWidth;
        swapChainDesc.initialHeight = surfaceHeight;
        swapChainDesc.depthFormat = Veldrid::PixelFormat::D24_UNorm_S8_UInt;
        swapChain = dev->GetResourceFactory()->CreateSwapChain(swapChainDesc);
    }

    void CreateShaders() {
        auto pComplier = IHLSLCompiler::Create();

        //auto spvCompiler = Veldrid::IGLSLCompiler::Get();
        std::string compileInfo;

        std::cout << "Compiling vertex shader...\n";
        std::vector<std::uint8_t> vertexSpv;
        
        std::vector<std::uint8_t> fragSpv;

        try {
            vertexSpv = pComplier->Compile(HLSLCode, L"VSMain", ShaderType::VS);
        } catch (const ShaderCompileError& const e){
            std::cout << "Vertex shader compilation failed\n";
            std::cout << "    ERROR: " << e.what() << std::endl;
            throw e;
        }

        try {
            fragSpv = pComplier->Compile(HLSLCode, L"PSMain", ShaderType::PS);
        } catch (const ShaderCompileError& const e){
            std::cout << "Fragment shader compilation failed\n";
            std::cout << "    ERROR: " << e.what() << std::endl;
            throw e;
        }
/*
        Veldrid::Shader::Stage stage = Veldrid::Shader::Stage::Vertex;
        if (!spvCompiler->CompileToSPIRV(
            stage,
            VertexCode,
            "main",
            {},
            vertexSpv,
            compileInfo
        )) {
            std::cout << compileInfo << "\n";
        }

        std::cout << "Compiling fragment shader...\n";

        stage = Veldrid::Shader::Stage::Fragment;
        if (!spvCompiler->CompileToSPIRV(
            stage,
            FragmentCode,
            "main",
            {},
            fragSpv,
            compileInfo
        )) {
            std::cout << compileInfo << "\n";
        }
*/
        auto factory = dev->GetResourceFactory();
        Veldrid::Shader::Description vertexShaderDesc{};
        vertexShaderDesc.stage = Veldrid::Shader::Stage::Vertex;
        vertexShaderDesc.entryPoint = "VSMain";
        Veldrid::Shader::Description fragmentShaderDesc{};
        fragmentShaderDesc.stage = Veldrid::Shader::Stage::Fragment;
        fragmentShaderDesc.entryPoint = "PSMain";

        fragmentShader = factory->CreateShader(fragmentShaderDesc, {(uint8_t*)fragSpv.data(), fragSpv.size()});
        vertexShader = factory->CreateShader(vertexShaderDesc, {(uint8_t*)vertexSpv.data(), vertexSpv.size()});

        delete pComplier;
    }

    void CreateBuffers() {
        auto factory = dev->GetResourceFactory();

        std::vector<VertexData> quadVertices
        {
            {{-0.75f, 0.75f},  {}, {0.f, 0.f, 1.f, 1.f}},
            {{0.75f, 0.75f},   {}, {0.f, 1.f, 0.f, 1.f}},
            {{-0.75f, -0.75f}, {}, {1.f, 0.f, 0.f, 1.f}},
            {{0.75f, -0.75f},  {}, {1.f, 1.f, 0.f, 1.f}}
        };

        std::vector<std::uint32_t> quadIndices { 0, 1, 2, 3 };

        Veldrid::Buffer::Description _vbDesc{};
        _vbDesc.sizeInBytes = 4 * sizeof(VertexData);
        _vbDesc.usage.vertexBuffer = 1;
        //_vbDesc.usage.staging = 1;
        vertexBuffer = factory->CreateBuffer(_vbDesc);
        UpdateBuffer(vertexBuffer, quadVertices);

        Veldrid::Buffer::Description _ibDesc{};
        _ibDesc.sizeInBytes = 4 * sizeof(std::uint32_t);
        _ibDesc.usage.indexBuffer = 1;
        //_ibDesc.usage.staging = 1;
        indexBuffer = factory->CreateBuffer(_ibDesc);
        UpdateBuffer(indexBuffer, quadIndices);

        Veldrid::Buffer::Description _ubDesc{};
        _ubDesc.sizeInBytes = sizeof(UniformBufferObject);
        _ubDesc.usage.uniformBuffer = 1;
        //_ubDesc.usage.staging = 1;
        _ubDesc.hostAccess = Veldrid::HostAccess::PreferWrite;
        uniformBuffer = factory->CreateBuffer(_ubDesc);

        Veldrid::Buffer::Description _tbDesc{};
        _tbDesc.sizeInBytes = 4 * sizeof(VertexData);
        _tbDesc.usage.structuredBufferReadOnly = 1;
        structBuffer = factory->CreateBuffer(_tbDesc);
    }

    void CreatePipeline() {

        auto factory = dev->GetResourceFactory();

        Veldrid::ResourceLayout::Description resLayoutDesc{};
        using ElemKind = Veldrid::IBindableResource::ResourceKind;
        resLayoutDesc.elements.resize(2, {});
        {
            resLayoutDesc.elements[0].name = "ObjectUniform";
            resLayoutDesc.elements[0].kind = ElemKind::UniformBuffer;
            resLayoutDesc.elements[0].stages = Veldrid::Shader::Stage::Vertex | Veldrid::Shader::Stage::Fragment;
        }

        {
            resLayoutDesc.elements[1].name = "Struct";
            resLayoutDesc.elements[1].kind = ElemKind::StorageBuffer;
            resLayoutDesc.elements[1].stages = Veldrid::Shader::Stage::Vertex | Veldrid::Shader::Stage::Fragment;
        }

        auto _layout = factory->CreateResourceLayout(resLayoutDesc);

        Veldrid::ResourceSet::Description resSetDesc{};
        resSetDesc.layout = _layout;
        resSetDesc.boundResources = {
            Veldrid::BufferRange::MakeByteBuffer(uniformBuffer), 
            Veldrid::BufferRange::MakeByteBuffer(structBuffer)
        };
        shaderResources = factory->CreateResourceSet(resSetDesc);
        
        Veldrid::GraphicsPipelineDescription pipelineDescription{};
        pipelineDescription.resourceLayout = _layout;
        pipelineDescription.blendState = {};
        pipelineDescription.blendState.attachments = { Veldrid::BlendStateDescription::Attachment::MakeOverrideBlend() };
        //pipelineDescription.blendState.attachments[0].blendEnabled = true;

        pipelineDescription.depthStencilState.depthTestEnabled = false;
        pipelineDescription.depthStencilState.depthWriteEnabled = true;
        pipelineDescription.depthStencilState.depthComparison = Veldrid::ComparisonKind::LessEqual;


        pipelineDescription.rasterizerState.cullMode = Veldrid::RasterizerStateDescription::FaceCullMode::Back;
        pipelineDescription.rasterizerState.fillMode = Veldrid::RasterizerStateDescription::PolygonFillMode::Solid;
        pipelineDescription.rasterizerState.frontFace = Veldrid::RasterizerStateDescription::FrontFace::Clockwise;
        pipelineDescription.rasterizerState.depthClipEnabled = true;
        pipelineDescription.rasterizerState.scissorTestEnabled = false;

        pipelineDescription.primitiveTopology = Veldrid::PrimitiveTopology::TriangleStrip;

        using VL = Veldrid::GraphicsPipelineDescription::ShaderSet::VertexLayout;
        pipelineDescription.shaderSet.vertexLayouts = { {} };
        pipelineDescription.shaderSet.vertexLayouts[0].SetElements({
            {"POSITION", {Veldrid::VertexInputSemantic::Name::Position, 0}, Veldrid::ShaderDataType::Float2},
            {"TEXCOORD", {Veldrid::VertexInputSemantic::Name::TextureCoordinate, 0}, Veldrid::ShaderDataType::Float2},
            {"COLOR", {Veldrid::VertexInputSemantic::Name::Color, 0}, Veldrid::ShaderDataType::Float4}
            });
        pipelineDescription.shaderSet.vertexShader = vertexShader;
        pipelineDescription.shaderSet.fragmentShader = fragmentShader;

        pipelineDescription.outputs = swapChain->GetBackBuffer()->GetOutputDescription();
        //pipelineDescription.outputs = fb->GetOutputDescription();
        pipeline = factory->CreateGraphicsPipeline(pipelineDescription);
    }

    void CreateSyncObjects() {
        auto factory = dev->GetResourceFactory();
        renderFinishFence = factory->CreateFence();
    }

    void* ubMapped = nullptr;
    void OnAppStart(
        Veldrid::SwapChainSource* swapChainSrc,
        unsigned surfaceWidth,
        unsigned surfaceHeight
    ) override {
        Veldrid::GraphicsDevice::Options opt{};
        opt.debug = true;
        opt.preferStandardClipSpaceYDirection = true;
        //dev = Veldrid::CreateVulkanGraphicsDevice(opt, swapChainSrc);
        //dev = Veldrid::CreateVulkanGraphicsDevice(opt);
        dev = Veldrid::CreateDX12GraphicsDevice(opt);
        
        //CreateResources
        std::cout << "Creating swapchain...\n";
        CreateSwapChain(swapChainSrc, surfaceWidth, surfaceHeight);
        std::cout << "Compiling shaders...\n";
        CreateShaders();
        std::cout << "Creating buffers...\n";
        CreateBuffers();
        std::cout << "Creating pipeline...\n";
        CreatePipeline();
        std::cout << "Creating sync objects...\n";
        CreateSyncObjects();

        ubMapped = uniformBuffer->MapToCPU();
    }

    virtual void OnAppExit() override {
        dev->WaitForIdle();

        uniformBuffer->UnMap();
    }

    virtual bool OnAppUpdate(float) override {
        UniformBufferObject ubo{};
        ubo.model = glm::rotate(
            glm::mat4(1.0f),
            timeElapsedSec * glm::radians(90.0f),
            glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(
            glm::vec3(2.0f, 2.0f, 2.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(
            glm::radians(45.0f), 
            swapChain->GetWidth() / (float)swapChain->GetHeight(),
            0.1f, 10.0f);

        int currWidth, currHeight;
        GetFramebufferSize(currWidth, currHeight);
        if( (currWidth != initialWidth) || (currHeight != initialHeight) ) {
            while (currWidth == 0 || currHeight == 0) {
                GetFramebufferSize(currWidth, currHeight);
                glfwWaitEvents();
            }
            dev->WaitForIdle();
            swapChain->Resize(currWidth, currHeight);
            initialWidth = currWidth;
            initialHeight = currHeight;
        }

        /* Render here */
        auto factory = dev->GetResourceFactory();
        //Get one drawable
        auto backBuffer = swapChain->GetBackBuffer();
        


        auto _commandList = factory->CreateCommandList();
        //Record command buffer
        _commandList->Begin();

        _commandList->BeginRenderPass(swapChain->GetBackBuffer());
        //_commandList->BeginRenderPass(fb);
        _commandList->SetPipeline(pipeline);
        _commandList->SetFullViewports();
        _commandList->SetFullScissorRects();

        {
            // Indicate that the back buffer will be used as a render target.
            //auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            //m_commandList->ResourceBarrier(1, &barrier);
            alloy::BarrierDescription desc{
                .stagesBefore = alloy::PipelineStage::BottomOfPipe,
                .stagesAfter = alloy::PipelineStage::RENDER_TARGET,
                //.barriers = { texBarrier, dsBarrier }
            };


            alloy::TextureBarrierDescription texBarrier{};
            texBarrier.accessBefore = alloy::ResourceAccess::COMMON;
            texBarrier.accessAfter = alloy::ResourceAccess::RENDER_TARGET;
            texBarrier.fromLayout = alloy::TextureLayout::COMMON;
            texBarrier.toLayout = alloy::TextureLayout::RENDER_TARGET;
            texBarrier.resource = swapChain->GetBackBuffer()->GetDesc().colorTargets[0].target;
            desc.barriers.push_back(texBarrier);

            //if(_isFirstSubmission) {
                alloy::TextureBarrierDescription dsBarrier{};
                dsBarrier.accessBefore = alloy::ResourceAccess::COMMON;
                dsBarrier.accessAfter = alloy::ResourceAccess::DEPTH_STENCIL_WRITE;
                dsBarrier.fromLayout = alloy::TextureLayout::COMMON;
                dsBarrier.toLayout = alloy::TextureLayout::DEPTH_STENCIL_WRITE;
                dsBarrier.resource = swapChain->GetBackBuffer()->GetDesc().depthTarget.target;
                desc.barriers.push_back(dsBarrier);
                //_isFirstSubmission = false;
            //}
            
            _commandList->Barrier(desc);
        }

        _commandList->ClearDepthStencil(0, 0);
        _commandList->ClearColorTarget(0, 0.9, 0.1, 0.3, 1);
        _commandList->SetVertexBuffer(0, vertexBuffer);
        _commandList->SetIndexBuffer(indexBuffer, Veldrid::IndexFormat::UInt32);
        _commandList->SetGraphicsResourceSet(shaderResources);
        //_commandList->Draw(3);
        _commandList->DrawIndexed(
            /*indexCount:    */4,
            /*instanceCount: */1,
            /*indexStart:    */0,
            /*vertexOffset:  */0,
            /*instanceStart: */0);

        // Indicate that the back buffer will now be used to present.
        {
            //auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            //m_commandList->ResourceBarrier(1, &barrier);
            alloy::TextureBarrierDescription texBarrier{};
            texBarrier.accessBefore = alloy::ResourceAccess::RENDER_TARGET;
            texBarrier.accessAfter = alloy::ResourceAccess::COMMON;
            texBarrier.fromLayout = alloy::TextureLayout::RENDER_TARGET;
            texBarrier.toLayout = alloy::TextureLayout::COMMON;
            texBarrier.resource = swapChain->GetBackBuffer()->GetDesc().colorTargets[0].target;

            alloy::TextureBarrierDescription dsBarrier{};
            dsBarrier.accessBefore = alloy::ResourceAccess::DEPTH_STENCIL_WRITE;
            dsBarrier.accessAfter = alloy::ResourceAccess::COMMON;
            dsBarrier.fromLayout = alloy::TextureLayout::DEPTH_STENCIL_WRITE;
            dsBarrier.toLayout = alloy::TextureLayout::COMMON;
            dsBarrier.resource = swapChain->GetBackBuffer()->GetDesc().depthTarget.target;

            alloy::BarrierDescription desc{
                .stagesBefore = alloy::PipelineStage::RENDER_TARGET,
                .stagesAfter = alloy::PipelineStage::BottomOfPipe,
                .barriers = { texBarrier, dsBarrier }
            };
            _commandList->Barrier(desc);
        }

        _commandList->EndRenderPass();
        _commandList->End();

        //Wait for previous render to complete
        //renderFinishFence->WaitFromCPU(renderFinishFenceValue);
        //renderFinishFenceValue++;
        renderFinishFenceValue++;
        //Update uniformbuffer
        memcpy(ubMapped, &ubo, sizeof(ubo));
        
        dev->GetGfxCommandQueue()->SubmitCommand(_commandList.get());
        dev->GetGfxCommandQueue()->EncodeSignalFence(renderFinishFence.get(), renderFinishFenceValue );
        // /cmd = _commandList;
        //Wait for render to complete
        renderFinishFence->WaitFromCPU(renderFinishFenceValue);

        //StopCapture();

        /* Swap front and back buffers */
        //glfwSwapBuffers(window);
        dev->PresentToSwapChain(
            {  },
            swapChain.get());

        return true;
    }

public:
    UniformApp() : AppBase(800, 600, "Uniform Buffers") {}

};

int main() {
	UniformApp app;
	app.Run();
}
