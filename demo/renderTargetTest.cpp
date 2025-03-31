//#include <alloy/backend/Backends.hpp>
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
#include <chrono>
#include <thread>

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


const char HLSLRectDrawer[] = R"AGAN(


struct VSQuadOut{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

// ouputs a full screen quad with tex coords
VSQuadOut VSMain(uint VertexID: SV_VertexID){
    VSQuadOut result;
    result.texcoord = float2( (VertexID << 1) & 2, VertexID & 2 );
    result.position = float4( result.texcoord * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f), 0.0f, 1.0f );
    return result;
}

float4 PSMain(VSQuadOut input) : SV_TARGET
{
    return float4(input.texcoord.xy, 1, 1);
}

)AGAN";


using namespace alloy;

class UniformApp : public AppBase {

    alloy::common::sp<alloy::IGraphicsDevice> dev;
    alloy::common::sp<alloy::ISwapChain> swapChain;

    alloy::common::sp<alloy::IShader> fragmentShader, vertexShader;
    alloy::common::sp<alloy::IBuffer> vertexBuffer, indexBuffer;
    alloy::common::sp<alloy::IBuffer> uniformBuffer, structBuffer;

    alloy::common::sp<alloy::IShader> rectDrawFragShader, rectDrawVertShader;

    alloy::common::sp<alloy::ITextureView> tex1;
    alloy::common::sp<alloy::ISampler> samp1;
    alloy::common::sp<alloy::IRenderTarget> rt1;

    alloy::common::sp<alloy::IResourceSet> shaderResources;

    alloy::common::sp<alloy::IGfxPipeline> pipeline;
    alloy::common::sp<alloy::IGfxPipeline> rectDrawPipeline;

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
        const T* data,
        size_t elementCnt
    ) {
        const unsigned stagingBufferSizeInBytes = 512;
        const auto transferSizeInBytes = sizeof(T) * elementCnt;

        alloy::IBuffer::Description desc{};
        desc.sizeInBytes = stagingBufferSizeInBytes;
        desc.hostAccess = alloy::HostAccess::PreferWrite;
        //desc.usage.staging = 1;
        auto transferBuffer = dev->GetResourceFactory().CreateBuffer(desc);

        auto fence = dev->GetResourceFactory().CreateSyncEvent();
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
            auto& pass = cmd->BeginTransferPass();
            pass.CopyBuffer(
                alloy::BufferRange::MakeByteBuffer(transferBuffer),
                alloy::BufferRange::MakeByteBuffer(buffer, transferedSize, batchSize),
                batchSize);

            cmd->EndPass();
            cmd->End();

            //submit and wait
            dev->GetCopyCommandQueue()->SubmitCommand(cmd.get());
            dev->GetCopyCommandQueue()->EncodeSignalEvent(fence.get(), signalValue);
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

        auto& factory = dev->GetResourceFactory();
        alloy::IShader::Description vertexShaderDesc{};
        vertexShaderDesc.stage = alloy::IShader::Stage::Vertex;
        vertexShaderDesc.entryPoint = "VSMain";
        alloy::IShader::Description fragmentShaderDesc{};
        fragmentShaderDesc.stage = alloy::IShader::Stage::Fragment;
        fragmentShaderDesc.entryPoint = "PSMain";

        fragmentShader = factory.CreateShader(fragmentShaderDesc, {(uint8_t*)fragSpv.data(), fragSpv.size()});
        vertexShader = factory.CreateShader(vertexShaderDesc, {(uint8_t*)vertexSpv.data(), vertexSpv.size()});

        std::cout << "Compiling rect drawer vertex shader..." << std::endl;

        try {
            vertexSpv = pComplier->Compile(HLSLRectDrawer, L"VSMain", ShaderType::VS);
        } catch (const ShaderCompileError& e){
            std::cout << "Vertex shader compilation failed\n";
            std::cout << "    ERROR: " << e.what() << std::endl;
            throw e;
        }

        
        std::cout << "Compiling rect drawer fragment shader..." << std::endl;

        try {
            fragSpv = pComplier->Compile(HLSLRectDrawer, L"PSMain", ShaderType::PS);
        } catch (const ShaderCompileError& e){
            std::cout << "Fragment shader compilation failed\n";
            std::cout << "    ERROR: " << e.what() << std::endl;
            throw e;
        }

        rectDrawFragShader = factory.CreateShader(fragmentShaderDesc, {(uint8_t*)fragSpv.data(), fragSpv.size()});
        rectDrawVertShader = factory.CreateShader(vertexShaderDesc, {(uint8_t*)vertexSpv.data(), vertexSpv.size()});

        delete pComplier;
    }

    void CreateBuffers() {
        auto& factory = dev->GetResourceFactory();

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

        alloy::IBuffer::Description _vbDesc{};
        _vbDesc.sizeInBytes = 4 * sizeof(VertexData);
        _vbDesc.usage.vertexBuffer = 1;
        _vbDesc.usage.indexBuffer = 1;
        _vbDesc.usage.uniformBuffer = 1;
        //_vbDesc.usage.staging = 1;
        vertexBuffer = factory.CreateBuffer(_vbDesc);
        vertexBuffer->SetDebugName("Vertex Buffer");
        UpdateBuffer(vertexBuffer, quadVertices.data(), quadVertices.size());

        alloy::IBuffer::Description _ibDesc{};
        _ibDesc.sizeInBytes = 4 * sizeof(std::uint32_t);
        _ibDesc.usage.indexBuffer = 1;
        //_ibDesc.usage.staging = 1;
        indexBuffer = factory.CreateBuffer(_ibDesc);
        indexBuffer->SetDebugName("Index Buffer");
        UpdateBuffer(indexBuffer, quadIndices.data(), quadIndices.size());

        alloy::IBuffer::Description _ubDesc{};
        _ubDesc.sizeInBytes = sizeof(UniformBufferObject);
        _ubDesc.usage.uniformBuffer = 1;
        //_ubDesc.usage.staging = 1;
        _ubDesc.hostAccess = alloy::HostAccess::PreferWrite;
        uniformBuffer = factory.CreateBuffer(_ubDesc);
        uniformBuffer->SetDebugName("Uniform Buffer");

        alloy::IBuffer::Description _tbDesc{};
        _tbDesc.sizeInBytes = sizeof(SceneDescriptor);
        _tbDesc.usage.structuredBufferReadOnly = 1;
        structBuffer = factory.CreateBuffer(_tbDesc);
        structBuffer->SetDebugName("Storage Buffer");
        UpdateBuffer(structBuffer, &sceneDescriptor, 1);
        
    }

    void CreateTextureResource() {
        auto& factory = dev->GetResourceFactory();

        alloy::ISampler::Description samp1Desc {};
        samp1Desc.maximumAnisotropy = 1;
        samp1 = factory.CreateSampler(samp1Desc);


        alloy::ITexture::Description tex1ImgDesc{};
                    
        tex1ImgDesc.width = 256;
        tex1ImgDesc.height = 256;
        tex1ImgDesc.depth = 1;
        tex1ImgDesc.mipLevels = 1;
        tex1ImgDesc.arrayLayers = 1;
        tex1ImgDesc.format = alloy::PixelFormat::R8_G8_B8_A8_UNorm;
        tex1ImgDesc.usage.sampled = 1;
        tex1ImgDesc.usage.renderTarget = 1;
        tex1ImgDesc.type = alloy::ITexture::Description::Type::Texture2D;
        tex1ImgDesc.sampleCount = alloy::SampleCount::x1;
        tex1ImgDesc.hostAccess = alloy::HostAccess::PreferDeviceMemory;
        auto tex1Img = factory.CreateTexture(tex1ImgDesc);

        tex1 = factory.CreateTextureView(tex1Img);
        rt1 = factory.CreateRenderTarget(tex1);
#if 0
        auto fence = dev->GetResourceFactory().CreateSyncEvent();
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
                    .accessAfter = alloy::ResourceAccess::SHADER_RESOURCE,
                },

                .resourceInfo = alloy::TextureBarrierResource {
                    .fromLayout = alloy::TextureLayout::UNDEFINED,
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
        dev->GetGfxCommandQueue()->EncodeSignalEvent(fence.get(), signalValue);
        fence->WaitFromCPU(signalValue);
#endif
    }

    void CreatePipeline() {

        auto& factory = dev->GetResourceFactory();

        {
            alloy::IResourceLayout::Description resLayoutDesc{};
            using ElemKind = alloy::IBindableResource::ResourceKind;
            using alloy::common::operator|;
            //resLayoutDesc.elements.resize(3, {});
            //{
            //    auto& elem = resLayoutDesc.shaderResources.emplace_back();
            //    //elem.name = "ObjectUniform";
            //    elem.bindingSlot = 0;
            //    elem.kind = ElemKind::UniformBuffer;
            //    elem.stages = alloy::IShader::Stage::Vertex | alloy::IShader::Stage::Fragment;
            //}
            {
                auto& pc = resLayoutDesc.pushConstants.emplace_back();
                pc.bindingSlot = 0;
                pc.bindingSpace = 0;
                pc.sizeInDwords = (sizeof(UniformBufferObject) + 3) / 4;
            }

            {
                auto& elem = resLayoutDesc.shaderResources.emplace_back();
                //elem.name = "Struct";
                elem.bindingSlot = 0;
                elem.kind = ElemKind::StorageBuffer;
                elem.stages = alloy::IShader::Stage::Vertex | alloy::IShader::Stage::Fragment;
            }

            {
                auto& elem = resLayoutDesc.shaderResources.emplace_back();
                //elem.name = "tex1";
                elem.bindingSlot = 1;
                elem.kind = ElemKind::Texture;
                elem.stages = alloy::IShader::Stage::Fragment;
            }

            {
                auto& elem = resLayoutDesc.shaderResources.emplace_back();
                //elem.name = "samp1";
                elem.bindingSlot = 0;
                elem.kind = ElemKind::Sampler;
                elem.stages = alloy::IShader::Stage::Fragment;
            }

            auto _layout = factory.CreateResourceLayout(resLayoutDesc);

            alloy::IResourceSet::Description resSetDesc{};
            resSetDesc.layout = _layout;
            resSetDesc.boundResources = {
                //alloy::BufferRange::MakeByteBuffer(uniformBuffer), 
                alloy::BufferRange::MakeByteBuffer(structBuffer),
                tex1,
                samp1
            };
            shaderResources = factory.CreateResourceSet(resSetDesc);

            auto outputDesc = swapChain->GetBackBuffer()->GetDesc();

            alloy::GraphicsPipelineDescription pipelineDescription{};
            pipelineDescription.resourceLayout = _layout;
            pipelineDescription.attachmentState.colorAttachments = { alloy::AttachmentStateDescription::ColorAttachment::MakeOverrideBlend() };
            pipelineDescription.attachmentState.colorAttachments.front().format = outputDesc.colorAttachments.front()->GetTexture().GetTextureObject()->GetDesc().format;
            if(outputDesc.depthAttachment) {
                alloy::AttachmentStateDescription::DepthStencilAttachment dsAttachment {};
                dsAttachment.depthStencilFormat =
                    outputDesc.depthAttachment->GetTexture().GetTextureObject()->GetDesc().format;

                pipelineDescription.attachmentState.depthStencilAttachment = dsAttachment;
            }
            //pipelineDescription.blendState.attachments[0].blendEnabled = true;
            pipelineDescription.attachmentState.sampleCount = SampleCount::x1;

            pipelineDescription.depthStencilState.depthTestEnabled = false;
            pipelineDescription.depthStencilState.depthWriteEnabled = false;
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

            //pipelineDescription.outputs = swapChain->GetBackBuffer()->GetDesc();
            //pipelineDescription.outputs = fb->GetOutputDescription();
            pipeline = factory.CreateGraphicsPipeline(pipelineDescription);
        }

        {
            
            alloy::GraphicsPipelineDescription pipelineDescription{};
            pipelineDescription.attachmentState.colorAttachments = { alloy::AttachmentStateDescription::ColorAttachment::MakeOverrideBlend() };
            pipelineDescription.attachmentState.colorAttachments.front().format = tex1->GetTextureObject()->GetDesc().format;
            pipelineDescription.attachmentState.sampleCount = SampleCount::x1;
            
            //pipelineDescription.blendState.attachments[0].blendEnabled = true;

            pipelineDescription.depthStencilState.depthTestEnabled = false;
            pipelineDescription.depthStencilState.depthWriteEnabled = true;
            pipelineDescription.depthStencilState.depthComparison = alloy::ComparisonKind::LessEqual;


            pipelineDescription.rasterizerState.cullMode = alloy::RasterizerStateDescription::FaceCullMode::Back;
            pipelineDescription.rasterizerState.fillMode = alloy::RasterizerStateDescription::PolygonFillMode::Solid;
            pipelineDescription.rasterizerState.frontFace = alloy::RasterizerStateDescription::FrontFace::Clockwise;
            pipelineDescription.rasterizerState.depthClipEnabled = true;
            pipelineDescription.rasterizerState.scissorTestEnabled = false;

            pipelineDescription.primitiveTopology = alloy::PrimitiveTopology::TriangleStrip;
            pipelineDescription.shaderSet.vertexShader = rectDrawVertShader;
            pipelineDescription.shaderSet.fragmentShader = rectDrawFragShader;

            OutputDescription outputs {};
            outputs.colorAttachments.push_back(rt1);

            //pipelineDescription.outputs = swapChain->GetBackBuffer()->GetDesc();
            //pipelineDescription.outputs = fb->GetOutputDescription();
            rectDrawPipeline = factory.CreateGraphicsPipeline(pipelineDescription);
        }
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
        alloy::IGraphicsDevice::Options opt{};
        opt.debug = true;
        opt.preferStandardClipSpaceYDirection = true;

        //dev = alloy::CreateVulkanGraphicsDevice(opt, swapChainSrc);
        #if 0//defined( _WIN32 )
            alloy::Backend backend = alloy::Backend::DX12;
        #elif __APPLE__
            alloy::Backend backend = alloy::Backend::Metal;
        #else
            alloy::Backend backend = alloy::Backend::Vulkan;
        #endif
        auto ctx = alloy::IContext::Create(backend);
        auto adp = ctx->EnumerateAdapters().front();
        dev = adp->RequestDevice(opt);

        auto& adpInfo = adp->GetAdapterInfo();

        std::cout << "Picked device:\n";
        std::cout << "    Vendor ID   : " << std::hex << adpInfo.vendorID << "\n";
        std::cout << "    Device ID   : " << std::hex << adpInfo.deviceID << "\n";
        std::cout << "    Name        : " <<             adpInfo.deviceName << "\n";
        std::cout << "    API version : " << std::dec << (std::string)(adpInfo.apiVersion) << std::endl;

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
        std::vector<uint32_t> ubo{};
        ubo.resize(sizeof(UniformBufferObject) / 4);

        auto pubo = (UniformBufferObject*)ubo.data();
        pubo->model = glm::rotate(
            glm::mat4(1.0f),
            timeElapsedSec * glm::radians(90.0f),
            glm::vec3(0.0f, 0.0f, 1.0f));
        pubo->view = glm::lookAt(
            glm::vec3(2.0f, 2.0f, 2.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f));
        pubo->proj = glm::perspective(
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
        //Render target pass
        {
            //Transition to render target state
            {
                alloy::BarrierDescription texBarrier{
                    .memBarrier = {
                        .stagesBefore = alloy::PipelineStage::All,
                        .stagesAfter = alloy::PipelineStage::RENDER_TARGET,
                        .accessBefore =  alloy::ResourceAccess::SHADER_RESOURCE,
                        .accessAfter = alloy::ResourceAccess::RENDER_TARGET,
                    },

                    .resourceInfo = alloy::TextureBarrierResource {
                        .fromLayout = alloy::TextureLayout::SHADER_RESOURCE,
                        .toLayout = alloy::TextureLayout::RENDER_TARGET,
                        .resource = tex1->GetTextureObject()
                    }
                    //.barriers = { texBarrier, dsBarrier }
                };

                //_commandList->Barrier({ texBarrier });
            }
            alloy::RenderPassAction passAction{};
            auto& ctAct = passAction.colorTargetActions.emplace_back();
            ctAct.loadAction = alloy::LoadAction::DontCare;
            ctAct.storeAction = alloy::StoreAction::Store;
            //ctAct.clearColor = {0.5, 0.5, 0.5, 1};
            ctAct.target = rt1;

            auto& pass = _commandList->BeginRenderPass(passAction);
            pass.SetPipeline(rectDrawPipeline);
            pass.SetFullViewports();
            pass.SetFullScissorRects();

            pass.Draw(
                /*vertexCount:   */3,
                /*instanceCount: */1,
                /*vertexStart:   */0,
                /*instanceStart: */0);

            _commandList->EndPass();

            {
                alloy::BarrierDescription texBarrier{
                    .memBarrier = {
                        .stagesBefore = alloy::PipelineStage::RENDER_TARGET,
                        .stagesAfter = alloy::PipelineStage::All,
                        .accessBefore =  alloy::ResourceAccess::RENDER_TARGET,
                        .accessAfter = alloy::ResourceAccess::SHADER_RESOURCE,
                    },

                    .resourceInfo = alloy::TextureBarrierResource {
                        .fromLayout = alloy::TextureLayout::RENDER_TARGET,
                        .toLayout = alloy::TextureLayout::SHADER_RESOURCE,
                        .resource = tex1->GetTextureObject()
                    }
                    //.barriers = { texBarrier, dsBarrier }
                };

                //_commandList->Barrier({ texBarrier });                
            }
        }

        //Render pass
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
                    .accessAfter = alloy::ResourceAccess::DepthStencilWritable,
                },

                .resourceInfo = alloy::TextureBarrierResource {
                    .fromLayout = isInitSubmission? initialLayout : alloy::TextureLayout::COMMON,
                    .toLayout = alloy::TextureLayout::DEPTH_STENCIL_WRITE,
                    .resource = backBufferDesc.depthAttachment->GetTexture().GetTextureObject()
                }
            };
                //_isFirstSubmission = false;
            //}
            
            //_commandList->Barrier({ texBarrier, dsBarrier });
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

        //_commandList->ClearDepthStencil(0, 0);
        //_commandList->ClearColorTarget(0, 0.9, 0.1, 0.3, 1);
        pass.SetVertexBuffer(0, alloy::BufferRange::MakeByteBuffer(vertexBuffer) );
        pass.SetIndexBuffer(alloy::BufferRange::MakeByteBuffer(indexBuffer), alloy::IndexFormat::UInt32);
        pass.SetGraphicsResourceSet(shaderResources);
        pass.SetPushConstants(0, ubo, 0);

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
                    .accessBefore = alloy::ResourceAccess::DepthStencilWritable,
                    .accessAfter = alloy::ResourceAccess::COMMON,
                },

                .resourceInfo = alloy::TextureBarrierResource {
                    .fromLayout = alloy::TextureLayout::DEPTH_STENCIL_WRITE,
                    .toLayout = alloy::TextureLayout::COMMON,
                    .resource = backBufferDesc.depthAttachment->GetTexture().GetTextureObject()
                }
            };

            //_commandList->Barrier({ texBarrier, dsBarrier });
        }

        _commandList->End();

        //Wait for previous render to complete
        //renderFinishFence->WaitFromCPU(renderFinishFenceValue);
        //renderFinishFenceValue++;
        renderFinishFenceValue++;
        //Update uniformbuffer
        //memcpy(ubMapped, &ubo, sizeof(ubo));
        
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
    
    
#if defined(VLD_PLATFORM_MACOS)
    //Workaround xcode bug of multiple instances opened when debug
    //: Sleep for 3 seconds on startup
    std::this_thread::sleep_for(std::chrono::seconds(1));
#endif
    
	UniformApp app;
	app.Run();
}
