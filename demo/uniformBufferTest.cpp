#include <alloy/Context.hpp>
#include <alloy/SwapChainSources.hpp>
#include <alloy/SwapChain.hpp>
#include <alloy/BindableResource.hpp>
#include <alloy/GraphicsDevice.hpp>
#include <alloy/CommandQueue.hpp>
#include <alloy/ResourceFactory.hpp>
#include <alloy/Buffer.hpp>
#include <alloy/CommandList.hpp>
#include <alloy/Pipeline.hpp>
#include <alloy/Shader.hpp>

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

const char HLSLCode[] = R"AGAN(
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

    alloy::common::sp<alloy::IGraphicsDevice> dev;
    alloy::common::sp<alloy::ISwapChain> swapChain;

    alloy::common::sp<alloy::IShader> fragmentShader, vertexShader;
    alloy::common::sp<alloy::IBuffer> vertexBuffer, indexBuffer;
    alloy::common::sp<alloy::IBuffer> uniformBuffer, structBuffer;

    alloy::common::sp<alloy::IResourceSet> shaderResources;

    alloy::common::sp<alloy::IGfxPipeline> pipeline;

    alloy::common::sp<alloy::IEvent> renderFinishFence;

    alloy::common::sp<alloy::ICommandList> cmd;

    uint64_t renderFinishFenceValue = 0;

    //Swapchain backbuffers requires image layout transitions
    //from undefined to target layout. This counter corresponds
    //to swapchain backbuffer count
    uint32_t _initSubmissionCnt = 2;

    template<typename T>
    void UpdateBuffer(
        const alloy::common::sp<alloy::IBuffer>& buffer,
        const std::vector<T>& data
    ) {
        const unsigned stagingBufferSizeInBytes = 512;
        const auto transferSizeInBytes = sizeof(T) * data.size();

        alloy::IBuffer::Description desc{};
        desc.sizeInBytes = stagingBufferSizeInBytes;
        desc.hostAccess = alloy::HostAccess::PreferWrite;
        //desc.usage.staging = 1;
        auto transferBuffer = dev->GetResourceFactory().CreateBuffer(desc);

        auto fence = dev->GetResourceFactory().CreateSyncEvent();
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
            auto cmd = dev->GetGfxCommandQueue()->CreateCommandList();
            cmd->Begin();
            auto& pass = cmd->BeginTransferPass();
            pass.CopyBuffer(
                alloy::BufferRange::MakeByteBuffer(transferBuffer),
                alloy::BufferRange::MakeByteBuffer(buffer, transferedSize, batchSize),
                batchSize);
            cmd->EndPass();
            cmd->End();

            //submit and wait
            dev->GetGfxCommandQueue()->SubmitCommand(cmd.get());
            dev->GetGfxCommandQueue()->EncodeSignalEvent(fence.get(), signalValue);
            fence->WaitFromCPU(signalValue);
            signalValue++;

            //Advance counters
            remainingSize -= batchSize;
        }

        transferBuffer->UnMap();

    }

    void CreateSwapChain(
        alloy::SwapChainSource* swapChainSrc,
        unsigned surfaceWidth,
        unsigned surfaceHeight
    ) {
        alloy::ISwapChain::Description swapChainDesc{};
        swapChainDesc.source = swapChainSrc;
        swapChainDesc.initialWidth = surfaceWidth;
        swapChainDesc.initialHeight = surfaceHeight;
        swapChainDesc.depthFormat = alloy::PixelFormat::D24_UNorm_S8_UInt;
        swapChainDesc.backBufferCnt = _initSubmissionCnt;
        swapChain = dev->GetResourceFactory().CreateSwapChain(swapChainDesc);
    }

    void CreateShaders() {
        auto pComplier = IHLSLCompiler::Create();

        //auto spvCompiler = alloy::IGLSLCompiler::Get();
        std::string compileInfo;

        std::cout << "Compiling vertex shader...\n";
        std::vector<std::uint8_t> vertexSpv;
        
        std::vector<std::uint8_t> fragSpv;

        try {
            vertexSpv = pComplier->Compile(HLSLCode, L"VSMain", ShaderType::VS);
        } catch (const ShaderCompileError& e){
            std::cout << "Vertex shader compilation failed\n";
            std::cout << "    ERROR: " << e.what() << std::endl;
            throw e;
        }

        try {
            fragSpv = pComplier->Compile(HLSLCode, L"PSMain", ShaderType::PS);
        } catch (const ShaderCompileError& e){
            std::cout << "Fragment shader compilation failed\n";
            std::cout << "    ERROR: " << e.what() << std::endl;
            throw e;
        }
/*
        alloy::Shader::Stage stage = alloy::Shader::Stage::Vertex;
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

        stage = alloy::Shader::Stage::Fragment;
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
        auto& factory = dev->GetResourceFactory();
        alloy::IShader::Description vertexShaderDesc{};
        vertexShaderDesc.stage = alloy::IShader::Stage::Vertex;
        vertexShaderDesc.entryPoint = "VSMain";
        alloy::IShader::Description fragmentShaderDesc{};
        fragmentShaderDesc.stage = alloy::IShader::Stage::Fragment;
        fragmentShaderDesc.entryPoint = "PSMain";

        fragmentShader = factory.CreateShader(fragmentShaderDesc, {(uint8_t*)fragSpv.data(), fragSpv.size()});
        vertexShader = factory.CreateShader(vertexShaderDesc, {(uint8_t*)vertexSpv.data(), vertexSpv.size()});

        delete pComplier;
    }

    void CreateBuffers() {
        auto& factory = dev->GetResourceFactory();

        std::vector<VertexData> quadVertices
        {
            {{-0.75f, 0.75f},  {}, {0.f, 0.f, 1.f, 1.f}},
            {{0.75f, 0.75f},   {}, {0.f, 1.f, 0.f, 1.f}},
            {{-0.75f, -0.75f}, {}, {1.f, 0.f, 0.f, 1.f}},
            {{0.75f, -0.75f},  {}, {1.f, 1.f, 0.f, 1.f}}
        };

        std::vector<std::uint32_t> quadIndices { 0, 1, 2, 3 };

        alloy::IBuffer::Description _vbDesc{};
        _vbDesc.sizeInBytes = 4 * sizeof(VertexData);
        _vbDesc.usage.vertexBuffer = 1;
        //_vbDesc.usage.staging = 1;
        vertexBuffer = factory.CreateBuffer(_vbDesc);
        UpdateBuffer(vertexBuffer, quadVertices);

        alloy::IBuffer::Description _ibDesc{};
        _ibDesc.sizeInBytes = 4 * sizeof(std::uint32_t);
        _ibDesc.usage.indexBuffer = 1;
        //_ibDesc.usage.staging = 1;
        indexBuffer = factory.CreateBuffer(_ibDesc);
        UpdateBuffer(indexBuffer, quadIndices);

        alloy::IBuffer::Description _ubDesc{};
        _ubDesc.sizeInBytes = sizeof(UniformBufferObject);
        _ubDesc.usage.uniformBuffer = 1;
        //_ubDesc.usage.staging = 1;
        _ubDesc.hostAccess = alloy::HostAccess::PreferWrite;
        uniformBuffer = factory.CreateBuffer(_ubDesc);

        alloy::IBuffer::Description _tbDesc{};
        _tbDesc.sizeInBytes = 4 * sizeof(VertexData);
        _tbDesc.usage.structuredBufferReadOnly = 1;
        structBuffer = factory.CreateBuffer(_tbDesc);
    }

    void CreatePipeline() {

        auto& factory = dev->GetResourceFactory();

        alloy::IResourceLayout::Description resLayoutDesc{};
        using ElemKind = alloy::IBindableResource::ResourceKind;
        using alloy::common::operator|;
        resLayoutDesc.elements.resize(2, {});
        {
            resLayoutDesc.elements[0].name = "ObjectUniform";
            resLayoutDesc.elements[0].kind = ElemKind::UniformBuffer;
            resLayoutDesc.elements[0].stages = alloy::IShader::Stage::Vertex | alloy::IShader::Stage::Fragment;
        }

        {
            resLayoutDesc.elements[1].name = "Struct";
            resLayoutDesc.elements[1].kind = ElemKind::StorageBuffer;
            resLayoutDesc.elements[1].stages = alloy::IShader::Stage::Vertex | alloy::IShader::Stage::Fragment;
        }

        auto _layout = factory.CreateResourceLayout(resLayoutDesc);

        alloy::IResourceSet::Description resSetDesc{};
        resSetDesc.layout = _layout;
        resSetDesc.boundResources = {
            alloy::BufferRange::MakeByteBuffer(uniformBuffer), 
            alloy::BufferRange::MakeByteBuffer(structBuffer)
        };
        shaderResources = factory.CreateResourceSet(resSetDesc);

        auto outputDesc = swapChain->GetBackBuffer()->GetDesc();
        
        alloy::GraphicsPipelineDescription pipelineDescription{};
        pipelineDescription.resourceLayout = _layout;
        pipelineDescription.attachmentState = {};
        pipelineDescription.attachmentState.colorAttachments.front().format = outputDesc.colorAttachments.front()->GetTexture().GetTextureObject()->GetDesc().format;
        if(outputDesc.depthAttachment) {
            alloy::AttachmentStateDescription::DepthStencilAttachment dsAttachment {};
            dsAttachment.depthStencilFormat =
                outputDesc.depthAttachment->GetTexture().GetTextureObject()->GetDesc().format;
            
            pipelineDescription.attachmentState.depthStencilAttachment = dsAttachment;
        }//pipelineDescription.blendState.attachments[0].blendEnabled = true;

        pipelineDescription.depthStencilState.depthTestEnabled = false;
        pipelineDescription.depthStencilState.depthWriteEnabled = true;
        pipelineDescription.depthStencilState.depthComparison = alloy::ComparisonKind::LessEqual;


        pipelineDescription.rasterizerState.cullMode = alloy::RasterizerStateDescription::FaceCullMode::Back;
        pipelineDescription.rasterizerState.fillMode = alloy::RasterizerStateDescription::PolygonFillMode::Solid;
        pipelineDescription.rasterizerState.frontFace = alloy::RasterizerStateDescription::FrontFace::Clockwise;
        pipelineDescription.rasterizerState.depthClipEnabled = true;
        pipelineDescription.rasterizerState.scissorTestEnabled = false;

        pipelineDescription.primitiveTopology = alloy::PrimitiveTopology::TriangleStrip;

        using VL = alloy::VertexLayout;
        pipelineDescription.shaderSet.vertexLayouts = { {} };
        pipelineDescription.shaderSet.vertexLayouts[0].SetElements({
            {"POSITION", {alloy::VertexInputSemantic::Name::Position, 0}, alloy::ShaderDataType::Float2},
            {"TEXCOORD", {alloy::VertexInputSemantic::Name::TextureCoordinate, 0}, alloy::ShaderDataType::Float2},
            {"COLOR", {alloy::VertexInputSemantic::Name::Color, 0}, alloy::ShaderDataType::Float4}
            });
        pipelineDescription.shaderSet.vertexShader = vertexShader;
        pipelineDescription.shaderSet.fragmentShader = fragmentShader;

        pipelineDescription.outputs = swapChain->GetBackBuffer()->GetDesc();
        //pipelineDescription.outputs = fb->GetOutputDescription();
        pipeline = factory.CreateGraphicsPipeline(pipelineDescription);
    }

    void CreateSyncObjects() {
        auto& factory = dev->GetResourceFactory();
        renderFinishFence = factory.CreateSyncEvent();
    }

    void* ubMapped = nullptr;
    void OnAppStart(
        alloy::SwapChainSource* swapChainSrc,
        unsigned surfaceWidth,
        unsigned surfaceHeight
    ) override {
        
        auto ctx = alloy::IContext::CreateDefault();

        alloy::IGraphicsDevice::Options opt{};
        opt.debug = true;
        opt.preferStandardClipSpaceYDirection = true;
        //dev = alloy::CreateVulkanGraphicsDevice(opt, swapChainSrc);
        dev = ctx->CreateDefaultDevice(opt);
        //dev = alloy::CreateDX12GraphicsDevice(opt);

        auto& adpInfo = dev->GetAdapter().GetAdapterInfo();

        std::cout << "Picked device:\n";
        std::cout << "    Vendor ID   : " << std::hex << adpInfo.vendorID << "\n";
        std::cout << "    Device ID   : " << std::hex << adpInfo.deviceID << "\n";
        std::cout << "    Name        : " <<             adpInfo.deviceName << "\n";
        std::cout << "    API version : " << std::dec << adpInfo.apiVersion.major;
                                    std::cout << "." << adpInfo.apiVersion.minor;
                                    std::cout << "." << adpInfo.apiVersion.subminor;
                                    std::cout << "." << adpInfo.apiVersion.patch << std::endl;

        
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

        
        std::cout << "Resource creation completed." << std::endl;

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
            _initSubmissionCnt = 2;
        }

        /* Render here */
        auto& factory = dev->GetResourceFactory();
        //Get one drawable
        auto backBuffer = swapChain->GetBackBuffer();
        
        auto backBufferDesc = backBuffer->GetDesc();

        auto gfxQ = dev->GetGfxCommandQueue();
        auto _commandList = gfxQ->CreateCommandList();
        //Record command buffer
        _commandList->Begin();

        

        {
            auto initialLayout = alloy::TextureLayout::UNDEFINED;
            
            bool isInitSubmission = false;
            if(_initSubmissionCnt > 0) {
                _initSubmissionCnt--;
                isInitSubmission = true;
            }

            // Indicate that the back buffer will be used as a render target.
            //auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            //m_commandList->ResourceBarrier(1, &barrier);
            alloy::BarrierDescription texBarrier{
                .memBarrier = {
                    .stagesBefore = isInitSubmission ? alloy::PipelineStages{} : alloy::PipelineStage::All,
                    .stagesAfter = alloy::PipelineStage::RENDER_TARGET,
                    .accessBefore = isInitSubmission ? alloy::ResourceAccesses{} : alloy::ResourceAccess::COMMON,
                    .accessAfter = alloy::ResourceAccess::RENDER_TARGET,
                },

                .resourceInfo = alloy::TextureBarrierResource {
                    .fromLayout = isInitSubmission? initialLayout : alloy::TextureLayout::PRESENT,
                    .toLayout = alloy::TextureLayout::RENDER_TARGET,
                    .resource = backBufferDesc.colorAttachments[0]->GetTexture().GetTextureObject()
                }
                //.barriers = { texBarrier, dsBarrier }
            };


            //if(_isFirstSubmission) {
            alloy::BarrierDescription dsBarrier{
                .memBarrier = {
                    .stagesBefore = isInitSubmission ? alloy::PipelineStages{} : alloy::PipelineStage::All,
                    .stagesAfter = alloy::PipelineStage::DEPTH_STENCIL,
                    .accessBefore = isInitSubmission ? alloy::ResourceAccesses{} : alloy::ResourceAccess::COMMON,
                    .accessAfter = alloy::ResourceAccess::DEPTH_STENCIL_WRITE,
                },

                .resourceInfo = alloy::TextureBarrierResource {
                    .fromLayout = isInitSubmission? initialLayout : alloy::TextureLayout::COMMON,
                    .toLayout = alloy::TextureLayout::DEPTH_STENCIL_WRITE,
                    .resource = backBufferDesc.depthAttachment->GetTexture().GetTextureObject()
                }
            };
                //_isFirstSubmission = false;
            //}
            
            _commandList->Barrier({ texBarrier, dsBarrier });
        }
        
        auto fbDesc = swapChain->GetBackBuffer()->GetDesc();
        
        alloy::RenderPassAction passAction{};
        auto& ctAct = passAction.colorTargetActions.emplace_back();
        ctAct.loadAction = alloy::LoadAction::Clear;
        ctAct.storeAction = alloy::StoreAction::Store;
        ctAct.clearColor = {0.9, 0.1, 0.3, 1};
        ctAct.target = fbDesc.colorAttachments.front();

        auto& dtAct = passAction.depthTargetAction.emplace();
        dtAct.loadAction = alloy::LoadAction::Clear;
        dtAct.storeAction = alloy::StoreAction::DontCare;
        dtAct.target = fbDesc.depthAttachment;
        
        auto& stAct = passAction.stencilTargetAction.emplace();
        stAct.loadAction = alloy::LoadAction::Load;
        stAct.storeAction = alloy::StoreAction::Store;
        stAct.target = fbDesc.depthAttachment;

        auto& pass = _commandList->BeginRenderPass(passAction);
        //_commandList->BeginRenderPass(fb);
        pass.SetPipeline(pipeline);
        pass.SetFullViewports();
        pass.SetFullScissorRects();

        pass.SetVertexBuffer(0, alloy::BufferRange::MakeByteBuffer( vertexBuffer ));
        pass.SetIndexBuffer(alloy::BufferRange::MakeByteBuffer(indexBuffer), alloy::IndexFormat::UInt32);
        pass.SetGraphicsResourceSet(shaderResources);
        //_commandList->Draw(3);
        pass.DrawIndexed(
            /*indexCount:    */4,
            /*instanceCount: */1,
            /*indexStart:    */0,
            /*vertexOffset:  */0,
            /*instanceStart: */0);

        _commandList->EndPass();
        // Indicate that the back buffer will now be used to present.
        {
            //auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            //m_commandList->ResourceBarrier(1, &barrier);
            alloy::BarrierDescription texBarrier{
                .memBarrier = {
                    .stagesBefore = alloy::PipelineStage::RENDER_TARGET,
                    .stagesAfter = alloy::PipelineStage::All,
                    .accessBefore = alloy::ResourceAccess::RENDER_TARGET,
                    .accessAfter = alloy::ResourceAccess::COMMON,
                },

                .resourceInfo = alloy::TextureBarrierResource {
                    .fromLayout = alloy::TextureLayout::RENDER_TARGET,
                    .toLayout = alloy::TextureLayout::PRESENT,
                    .resource = backBufferDesc.colorAttachments[0]->GetTexture().GetTextureObject()
                }
                //.barriers = { texBarrier, dsBarrier }
            };


            //if(_isFirstSubmission) {
            alloy::BarrierDescription dsBarrier{
                .memBarrier = {
                    .stagesBefore = alloy::PipelineStage::DEPTH_STENCIL,
                    .stagesAfter = alloy::PipelineStage::All,
                    .accessBefore = alloy::ResourceAccess::DEPTH_STENCIL_WRITE,
                    .accessAfter = alloy::ResourceAccess::COMMON,
                },

                .resourceInfo = alloy::TextureBarrierResource {
                    .fromLayout = alloy::TextureLayout::DEPTH_STENCIL_WRITE,
                    .toLayout = alloy::TextureLayout::COMMON,
                    .resource = backBufferDesc.depthAttachment->GetTexture().GetTextureObject()
                }
            };

            _commandList->Barrier({ texBarrier, dsBarrier });
        }

        _commandList->End();

        //Wait for previous render to complete
        //renderFinishFence->WaitFromCPU(renderFinishFenceValue);
        //renderFinishFenceValue++;
        renderFinishFenceValue++;
        //Update uniformbuffer
        memcpy(ubMapped, &ubo, sizeof(ubo));
        
        gfxQ->SubmitCommand(_commandList.get());
        gfxQ->EncodeSignalEvent(renderFinishFence.get(), renderFinishFenceValue );
        // /cmd = _commandList;
        //Wait for render to complete
        renderFinishFence->WaitFromCPU(renderFinishFenceValue);

        //StopCapture();

        /* Swap front and back buffers */
        //glfwSwapBuffers(window);
        dev->PresentToSwapChain(
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
