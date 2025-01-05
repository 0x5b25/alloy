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
StructuredBuffer<SceneDescriptor> sceneDesc : register(t0);

Texture2D<float4> tex1 : register(t1);
sampler samp1 : register(s0);

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
    result.color = color + sceneDesc[0].offset;
    result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{

    return tex1.Sample(samp1, input.uv.xy);// + input.color;
    //return saturate(input.color);
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

    Veldrid::sp<Veldrid::TextureView> tex1;
    Veldrid::sp<Veldrid::Sampler> samp1;

    Veldrid::sp<Veldrid::ResourceSet> shaderResources;

    Veldrid::sp<Veldrid::Pipeline> pipeline;

    Veldrid::sp<Veldrid::Fence> renderFinishFence;

    Veldrid::sp<Veldrid::CommandList> cmd;

    uint64_t renderFinishFenceValue = 0;

    //Swapchain backbuffers requires image layout transitions
    //from undefined to target layout. This counter corresponds
    //to swapchain backbuffer count
    uint32_t _initSubmissionCnt = 2;

    template<typename T>
    void UpdateBuffer(
        const Veldrid::sp<Veldrid::Buffer>& buffer,
        const T* data,
        size_t elementCnt
    ) {
        const unsigned stagingBufferSizeInBytes = 512;
        const auto transferSizeInBytes = sizeof(T) * elementCnt;

        Veldrid::Buffer::Description desc{};
        desc.sizeInBytes = stagingBufferSizeInBytes;
        desc.hostAccess = Veldrid::HostAccess::PreferWrite;
        //desc.usage.staging = 1;
        auto transferBuffer = dev->GetResourceFactory()->CreateBuffer(desc);

        auto fence = dev->GetResourceFactory()->CreateFence();
        uint64_t signalValue = 1;

        unsigned remainingSize = transferSizeInBytes;
        auto readPtr = (const std::uint8_t*)data;
        auto writePtr = transferBuffer->MapToCPU();
        assert(writePtr != nullptr);

        while (remainingSize > 0)
        {
            //prepare the tranfer buffer
            auto transferedSize = transferSizeInBytes - remainingSize;
            auto batchSize = std::min(stagingBufferSizeInBytes, remainingSize);
            memcpy(writePtr, readPtr + transferedSize, batchSize);
            
            //Record the command buffer
            auto cmd = dev->GetCopyCommandQueue()->CreateCommandList();
            cmd->Begin();
            cmd->CopyBuffer(transferBuffer, 0, buffer, transferedSize, batchSize);
            cmd->End();

            //submit and wait
            dev->GetCopyCommandQueue()->SubmitCommand(cmd.get());
            dev->GetCopyCommandQueue()->EncodeSignalFence(fence.get(), signalValue);
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
        swapChainDesc.backBufferCnt = _initSubmissionCnt;
        swapChain = dev->GetResourceFactory()->CreateSwapChain(swapChainDesc);
    }

    void CreateShaders() {
        auto pComplier = IHLSLCompiler::Create();

        //auto spvCompiler = Veldrid::IGLSLCompiler::Get();
        std::string compileInfo;

        std::cout << "Compiling vertex shader..." << std::endl;
        std::vector<std::uint8_t> vertexSpv;
        
        std::vector<std::uint8_t> fragSpv;

        try {
            vertexSpv = pComplier->Compile(HLSLCode, L"VSMain", ShaderType::VS);
        } catch (const ShaderCompileError& e){
            std::cout << "Vertex shader compilation failed\n";
            std::cout << "    ERROR: " << e.what() << std::endl;
            throw e;
        }

        
        std::cout << "Compiling fragment shader..." << std::endl;

        try {
            fragSpv = pComplier->Compile(HLSLCode, L"PSMain", ShaderType::PS);
        } catch (const ShaderCompileError& e){
            std::cout << "Fragment shader compilation failed\n";
            std::cout << "    ERROR: " << e.what() << std::endl;
            throw e;
        }

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
            {{-0.75f, 0.75f},  {0, 0}, {0.f, 0.f, 1.f, 1.f}},
            {{0.75f, 0.75f},   {1, 0}, {0.f, 1.f, 0.f, 1.f}},
            {{-0.75f, -0.75f}, {0, 1}, {1.f, 0.f, 0.f, 1.f}},
            {{0.75f, -0.75f},  {1, 1}, {1.f, 1.f, 0.f, 1.f}}
        };

        std::vector<std::uint32_t> quadIndices { 0, 1, 2, 3 };

        struct SceneDescriptor
        {
            float offset[4];
            float padding[15 * 4];
        } sceneDescriptor {{ 0.1f, 0.2f, 0.3f, 1.0f }};

        Veldrid::Buffer::Description _vbDesc{};
        _vbDesc.sizeInBytes = 4 * sizeof(VertexData);
        _vbDesc.usage.vertexBuffer = 1;
        //_vbDesc.usage.staging = 1;
        vertexBuffer = factory->CreateBuffer(_vbDesc);
        UpdateBuffer(vertexBuffer, quadVertices.data(), quadVertices.size());

        Veldrid::Buffer::Description _ibDesc{};
        _ibDesc.sizeInBytes = 4 * sizeof(std::uint32_t);
        _ibDesc.usage.indexBuffer = 1;
        //_ibDesc.usage.staging = 1;
        indexBuffer = factory->CreateBuffer(_ibDesc);
        UpdateBuffer(indexBuffer, quadIndices.data(), quadIndices.size());

        Veldrid::Buffer::Description _ubDesc{};
        _ubDesc.sizeInBytes = sizeof(UniformBufferObject);
        _ubDesc.usage.uniformBuffer = 1;
        //_ubDesc.usage.staging = 1;
        _ubDesc.hostAccess = Veldrid::HostAccess::PreferWrite;
        uniformBuffer = factory->CreateBuffer(_ubDesc);

        Veldrid::Buffer::Description _tbDesc{};
        _tbDesc.sizeInBytes = sizeof(SceneDescriptor);
        _tbDesc.usage.structuredBufferReadOnly = 1;
        structBuffer = factory->CreateBuffer(_tbDesc);
        UpdateBuffer(structBuffer, &sceneDescriptor, 1);
        
    }

    void CreateTextureResource() {
        auto factory = dev->GetResourceFactory();

        Veldrid::Sampler::Description samp1Desc {};
        samp1 = factory->CreateSampler(samp1Desc);


        Veldrid::Texture::Description tex1ImgDesc{};
                    
        tex1ImgDesc.width = 256;
        tex1ImgDesc.height = 256;
        tex1ImgDesc.depth = 1;
        tex1ImgDesc.mipLevels = 1;
        tex1ImgDesc.arrayLayers = 1;
        tex1ImgDesc.format = Veldrid::PixelFormat::R8_G8_B8_A8_UNorm;
        tex1ImgDesc.usage.sampled = 1;
        tex1ImgDesc.type = Veldrid::Texture::Description::Type::Texture2D;
        tex1ImgDesc.sampleCount = Veldrid::SampleCount::x1;
        tex1ImgDesc.hostAccess = Veldrid::HostAccess::PreferDeviceMemory;
        auto tex1Img = factory->CreateTexture(tex1ImgDesc);

        tex1 = factory->CreateTextureView(tex1Img);

        auto fence = dev->GetResourceFactory()->CreateFence();
        uint64_t signalValue = 1;

        //auto readPtr = (const std::uint8_t*)data;
        //auto writePtr = tex1Img->MapToCPU();

        //Record the command buffer
        auto cmd = dev->GetGfxCommandQueue()->CreateCommandList();
        cmd->Begin();
        {
            alloy::BarrierDescription barrier{
                .memBarrier = {
                    .stagesBefore = alloy::PipelineStages{},
                    .stagesAfter = alloy::PipelineStage::All,
                    .accessBefore = alloy::ResourceAccesses{},
                    .accessAfter = alloy::ResourceAccess::COMMON,
                },

                .resourceInfo = alloy::TextureBarrierResource {
                    .fromLayout = alloy::TextureLayout::UNDEFINED,
                    .toLayout = alloy::TextureLayout::COMMON,
                    .resource = tex1Img
                }
                //.barriers = { texBarrier, dsBarrier }
            };
            cmd->Barrier({barrier});
        }

        cmd->End();

        //submit and wait
        dev->GetGfxCommandQueue()->SubmitCommand(cmd.get());
        dev->GetGfxCommandQueue()->EncodeSignalFence(fence.get(), signalValue);
        fence->WaitFromCPU(signalValue);
        signalValue++;

        //Begin CPU write
        auto layout = tex1Img->GetSubresourceLayout(0,0);

        auto pCPUBuffer = new uint32_t[256*256];

        uint32_t* pPixel = pCPUBuffer;
        for(int y = 0; y < 256; y++) {
            bool flipY = (y / 64) & 1 != 0;
            for(int x = 0; x < 256; x++) {
                bool flipX = (x / 64) & 1 != 0;
                bool flip = flipY ^ flipX;
                pPixel[x] = flip ? 0xff223399 : 0x0;
            }
            pPixel += 256;
        }

        tex1Img->WriteSubresource(0,0,0,0,0,256,256,1,pCPUBuffer,256*4, 256*256*4);

        delete[] pCPUBuffer;

        cmd->Begin();
        {
            alloy::BarrierDescription barrier{
                .memBarrier = {
                    .stagesBefore = alloy::PipelineStage::All,
                    .stagesAfter = alloy::PipelineStage::All,
                    .accessBefore = alloy::ResourceAccess::COMMON,
                    .accessAfter = alloy::ResourceAccess::SHADER_RESOURCE,
                },

                .resourceInfo = alloy::TextureBarrierResource {
                    .fromLayout = alloy::TextureLayout::COMMON,
                    .toLayout = alloy::TextureLayout::SHADER_RESOURCE,
                    .resource = tex1Img
                }
                //.barriers = { texBarrier, dsBarrier }
            };
            cmd->Barrier({barrier});
        }
        cmd->End();
        //submit and wait
        dev->GetGfxCommandQueue()->SubmitCommand(cmd.get());
        dev->GetGfxCommandQueue()->EncodeSignalFence(fence.get(), signalValue);
        fence->WaitFromCPU(signalValue);

        //Advance counters

    }

    void CreatePipeline() {

        auto factory = dev->GetResourceFactory();

        Veldrid::ResourceLayout::Description resLayoutDesc{};
        using ElemKind = Veldrid::IBindableResource::ResourceKind;
        //resLayoutDesc.elements.resize(3, {});
        {
            auto& elem = resLayoutDesc.elements.emplace_back();
            elem.name = "ObjectUniform";
            elem.bindingSlot = 0;
            elem.kind = ElemKind::UniformBuffer;
            elem.stages = Veldrid::Shader::Stage::Vertex | Veldrid::Shader::Stage::Fragment;
        }

        {
            auto& elem = resLayoutDesc.elements.emplace_back();
            elem.name = "Struct";
            elem.bindingSlot = 0;
            elem.kind = ElemKind::StorageBuffer;
            elem.stages = Veldrid::Shader::Stage::Vertex | Veldrid::Shader::Stage::Fragment;
        }

        {
            auto& elem = resLayoutDesc.elements.emplace_back();
            elem.name = "tex1";
            elem.bindingSlot = 1;
            elem.kind = ElemKind::Texture;
            elem.stages = Veldrid::Shader::Stage::Fragment;
        }

        {
            auto& elem = resLayoutDesc.elements.emplace_back();
            elem.name = "samp1";
            elem.bindingSlot = 0;
            elem.kind = ElemKind::Sampler;
            elem.stages = Veldrid::Shader::Stage::Fragment;
        }

        auto _layout = factory->CreateResourceLayout(resLayoutDesc);

        Veldrid::ResourceSet::Description resSetDesc{};
        resSetDesc.layout = _layout;
        resSetDesc.boundResources = {
            Veldrid::BufferRange::MakeByteBuffer(uniformBuffer), 
            Veldrid::BufferRange::MakeByteBuffer(structBuffer),
            tex1,
            samp1
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
        dev = Veldrid::CreateVulkanGraphicsDevice(opt);
        //dev = Veldrid::CreateDX12GraphicsDevice(opt);

        auto& adpInfo = dev->GetAdapterInfo();

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
        std::cout << "Creating texture resources...\n";
        CreateTextureResource();
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
        auto factory = dev->GetResourceFactory();
        //Get one drawable
        auto backBuffer = swapChain->GetBackBuffer();

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
                    .resource = swapChain->GetBackBuffer()->GetDesc().colorTargets[0].target
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
                    .resource = swapChain->GetBackBuffer()->GetDesc().depthTarget.target
                }
            };
                //_isFirstSubmission = false;
            //}
            
            _commandList->Barrier({ texBarrier, dsBarrier });
        }

        _commandList->BeginRenderPass(swapChain->GetBackBuffer());
        //_commandList->BeginRenderPass(fb);
        _commandList->SetPipeline(pipeline);
        _commandList->SetFullViewports();
        _commandList->SetFullScissorRects();

        _commandList->ClearDepthStencil(0, 0);
        _commandList->ClearColorTarget(0, 0.9, 0.1, 0.3, 1);
        _commandList->SetVertexBuffer(0, vertexBuffer);
        _commandList->SetIndexBuffer(indexBuffer, Veldrid::IndexFormat::UInt32);
        _commandList->SetGraphicsResourceSet(shaderResources);

        _commandList->DrawIndexed(
            /*indexCount:    */4,
            /*instanceCount: */1,
            /*indexStart:    */0,
            /*vertexOffset:  */0,
            /*instanceStart: */0);

        _commandList->EndRenderPass();
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
                    .resource = swapChain->GetBackBuffer()->GetDesc().colorTargets[0].target
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
                    .resource = swapChain->GetBackBuffer()->GetDesc().depthTarget.target
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
        gfxQ->EncodeSignalFence(renderFinishFence.get(), renderFinishFenceValue );
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
