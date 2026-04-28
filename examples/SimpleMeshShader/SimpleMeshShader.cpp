#include "IApp.hpp"
#include "alloy/Buffer.hpp"
#include "alloy/Types.hpp"

#include <glm/glm.hpp>
#include <glm/ext.hpp>

namespace QuadShader {
    #include "Shaders/QuadMeshShader_as_6_7.h"
    #include "Shaders/QuadMeshShader_ms_6_7.h"
    #include "Shaders/QuadMeshShader_ps_6_7.h"
}

struct SceneDescriptor
{
    glm::uvec2 chunkCnt;
    glm::vec2 chunkSize;
};

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

class SimpleMeshShader : public IApp {
    IAppRunner* _runner;

    alloy::common::sp<alloy::IMeshShaderPipeline> _pipeline;


    alloy::common::sp<alloy::IBuffer> uniformBuffer,
                                      structBuffer;
    void* pUniformBuffer;

    alloy::common::sp<alloy::ITextureView> tex1;
    alloy::common::sp<alloy::ISampler> samp1;


    alloy::common::sp<alloy::IResourceSet> _resSet;


    uint64_t _fenceVal = 0;
    alloy::common::sp<alloy::IEvent> _fence;


    template<typename T>
    void UpdateBuffer(
        const alloy::common::sp<alloy::IBuffer>& buffer,
        const T* data,
        size_t elementCnt
    ) {
        auto dev = _runner->GetRenderService()->GetDevice();
        const unsigned stagingBufferSizeInBytes = 512;
        const auto transferSizeInBytes = sizeof(T) * elementCnt;

        alloy::IBuffer::Description desc{};
        desc.sizeInBytes = stagingBufferSizeInBytes;
        desc.hostAccess = alloy::HostAccess::SystemMemoryPreferWrite;
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


    void _CreateResources();

    void _CreateBuffers();
    void _CreateTextures();
    void _CreatePipeline();

public:
    SimpleMeshShader(IAppRunner* runner)
        : _runner(runner)
    {
        _CreateResources();
    }

    virtual ~SimpleMeshShader() override {}

    virtual int GetExitCode() override { return 0; }


    virtual void FixedUpdate() override {}
    virtual void Update() override {}
    virtual void OnDrawGui() override {}

    virtual void OnRenderFrame(alloy::IRenderCommandEncoder& renderPass) override;

    virtual void OnFrameComplete(uint32_t frameIdx) {}
    virtual void OnFrameBegin(uint32_t frameIdx) {}
};

void SimpleMeshShader::_CreateResources() {
    _fence = _runner->GetRenderService()->GetDevice()->GetResourceFactory().CreateSyncEvent();
    _CreateBuffers();
    _CreateTextures();
    _CreatePipeline();
}


SceneDescriptor g_SceneDescriptor = SceneDescriptor{
    /*.patchCnt = */{ 16, 16},
    /*.patchSize = */{ 1.f, 1.f }
};

void SimpleMeshShader::_CreateBuffers() {
    auto& factory = _runner->GetRenderService()->GetDevice()->GetResourceFactory();


    alloy::IBuffer::Description _ubDesc{};
    _ubDesc.sizeInBytes = sizeof(UniformBufferObject);
    _ubDesc.usage.uniformBuffer = 1;
    _ubDesc.hostAccess = alloy::HostAccess::SystemMemoryPreferWrite;
    uniformBuffer = factory.CreateBuffer(_ubDesc);
    uniformBuffer->SetDebugName("Uniform Buffer");
    pUniformBuffer = uniformBuffer->MapToCPU();

    alloy::IBuffer::Description _tbDesc{};
    _tbDesc.sizeInBytes = sizeof(SceneDescriptor);
    _tbDesc.usage.structuredBufferReadOnly = 1;
    structBuffer = factory.CreateBuffer(_tbDesc);
    structBuffer->SetDebugName("Storage Buffer");
    UpdateBuffer(structBuffer, &g_SceneDescriptor, 1);
}

void SimpleMeshShader::_CreateTextures() {
    auto dev = _runner->GetRenderService()->GetDevice();
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
    tex1ImgDesc.type = alloy::ITexture::Description::Type::Texture2D;
    tex1ImgDesc.sampleCount = alloy::SampleCount::x1;
    tex1ImgDesc.hostAccess = alloy::HostAccess::PreferDeviceMemory;
    auto tex1Img = factory.CreateTexture(tex1ImgDesc);

    tex1 = factory.CreateTextureView(tex1Img);

    //Begin CPU write
    auto layout = tex1Img->GetSubresourceLayout(0,0);

    auto upload_pitch_src = tex1ImgDesc.width * 4;
    auto upload_pitch_dst = (upload_pitch_src + 256 - 1u) & ~(256 - 1u);
    auto upload_buffer_size = upload_pitch_dst * tex1ImgDesc.height;

    alloy::IBuffer::Description copyBufDesc { };
    copyBufDesc.sizeInBytes = upload_buffer_size;
    copyBufDesc.usage.structuredBufferReadOnly = 1;
    copyBufDesc.hostAccess = alloy::HostAccess::SystemMemoryPreferWrite;
    auto copyBuffer = alloy::BufferRange::MakeByteBuffer(factory.CreateBuffer(copyBufDesc));

    //Create a pattern
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

    auto copyDst = copyBuffer->MapToCPU();
    // Copy to upload buffer
    for (int y = 0; y < tex1ImgDesc.height; y++)
        memcpy((void*)((uintptr_t)copyDst + y * upload_pitch_dst),
                (void*)((uintptr_t)pCPUBuffer+ y * upload_pitch_src), upload_pitch_src);

    copyBuffer->UnMap();

    delete[] pCPUBuffer;

    auto cmdList = dev->GetGfxCommandQueue()->CreateCommandList();
    cmdList->Begin();
    auto& pass = cmdList->BeginTransferPass();

    pass.CopyBufferToTexture(
        copyBuffer, upload_pitch_dst, upload_buffer_size,
        tex1Img, {0, 0, 0}, 0, 0,
        {tex1ImgDesc.width, tex1ImgDesc.height, 1}
    );

    cmdList->EndPass();
    cmdList->End();

    //submit and wait
    dev->GetGfxCommandQueue()->SubmitCommand(cmdList.get());
    dev->GetGfxCommandQueue()->EncodeSignalEvent(_fence.get(), ++_fenceVal);
    _fence->WaitFromCPU(_fenceVal);

    //Advance counters

}

void SimpleMeshShader::_CreatePipeline() {
    auto* rndrSvc = _runner->GetRenderService();
    auto msaaSampleCount    = rndrSvc->GetFrameBufferSampleCount();
    auto renderTargetFormat = rndrSvc->GetFrameBufferColorFormat();
    auto depthStencilFormat = rndrSvc->GetFrameBufferDepthStencilFormat();

    auto& factory = rndrSvc->GetDevice()->GetResourceFactory();

    //Create pipeline
    alloy::IResourceLayout::Description resLayoutDesc{};
    using ElemKind = alloy::IBindableResource::ResourceKind;
    using alloy::common::operator|;
    //resLayoutDesc.elements.resize(3, {});
    {
        auto& elem = resLayoutDesc.shaderResources.emplace_back();
        //elem.name = "ObjectUniform";
        elem.bindingSlot = 0;
        elem.bindingCount = 1;
        elem.kind = ElemKind::UniformBuffer;
        elem.stages = alloy::IShader::Stage::Task
                    | alloy::IShader::Stage::Mesh
                    | alloy::IShader::Stage::Fragment;
    }

    {
        auto& elem = resLayoutDesc.shaderResources.emplace_back();
        //elem.name = "Struct";
        elem.bindingSlot = 0;
        elem.bindingCount = 1;
        elem.kind = ElemKind::StorageBuffer;
        elem.stages = alloy::IShader::Stage::Task
                    | alloy::IShader::Stage::Mesh
                    | alloy::IShader::Stage::Fragment;
    }

    {
        auto& elem = resLayoutDesc.shaderResources.emplace_back();
        //elem.name = "tex1";
        elem.bindingSlot = 1;
        elem.bindingCount = 1;
        elem.kind = ElemKind::Texture;
        elem.stages = alloy::IShader::Stage::Fragment;
    }

    {
        auto& elem = resLayoutDesc.shaderResources.emplace_back();
        //elem.name = "samp1";
        elem.bindingSlot = 0;
        elem.bindingCount = 1;
        elem.kind = ElemKind::Sampler;
        elem.stages = alloy::IShader::Stage::Fragment;
    }

    auto _layout = factory.CreateResourceLayout(resLayoutDesc);

    alloy::IResourceSet::Description resSetDesc{};
    resSetDesc.layout = _layout;
    resSetDesc.boundResources = {
        alloy::BufferRange::MakeByteBuffer(uniformBuffer),
        alloy::BufferRange::MakeByteBuffer(structBuffer),
        tex1,
        samp1
    };
    _resSet = factory.CreateResourceSet(resSetDesc);

    alloy::MeshShaderPipelineDescription pipelineDescription{};
    pipelineDescription.resourceLayout = _layout;
    pipelineDescription.attachmentState.colorAttachments = { alloy::AttachmentStateDescription::ColorAttachment::MakeOverrideBlend() };
    pipelineDescription.attachmentState.colorAttachments.front().format = renderTargetFormat;
    {
        alloy::AttachmentStateDescription::DepthStencilAttachment dsAttachment {};
        dsAttachment.depthStencilFormat = depthStencilFormat;

        pipelineDescription.attachmentState.depthStencilAttachment = dsAttachment;
    }
    //pipelineDescription.blendState.attachments[0].blendEnabled = true;
    pipelineDescription.attachmentState.sampleCount = msaaSampleCount;

    pipelineDescription.depthStencilState.depthTestEnabled = true;
    pipelineDescription.depthStencilState.depthWriteEnabled = true;
    pipelineDescription.depthStencilState.depthComparison = alloy::ComparisonKind::LessEqual;


    pipelineDescription.rasterizerState.cullMode = alloy::RasterizerStateDescription::FaceCullMode::None;
    pipelineDescription.rasterizerState.fillMode = alloy::RasterizerStateDescription::PolygonFillMode::Solid;
    pipelineDescription.rasterizerState.frontFace = alloy::RasterizerStateDescription::FrontFace::CounterClockwise;
    pipelineDescription.rasterizerState.depthClipEnabled = true;
    pipelineDescription.rasterizerState.scissorTestEnabled = false;

    alloy::IShader::Description taskShaderDesc{};
    taskShaderDesc.stage = alloy::IShader::Stage::Task;
    taskShaderDesc.entryPoint = "ASMain";
    alloy::IShader::Description meshShaderDesc{};
    meshShaderDesc.stage = alloy::IShader::Stage::Mesh;
    meshShaderDesc.entryPoint = "MSMain";
    alloy::IShader::Description fragmentShaderDesc{};
    fragmentShaderDesc.stage = alloy::IShader::Stage::Fragment;
    fragmentShaderDesc.entryPoint = "PSMain";

    auto taskShader = factory.CreateShader(taskShaderDesc, QuadShader::g_ASMain);
    auto meshShader = factory.CreateShader(meshShaderDesc, QuadShader::g_MSMain);
    auto fragmentShader = factory.CreateShader(fragmentShaderDesc, QuadShader::g_PSMain);

    pipelineDescription.taskShader = taskShader;
    pipelineDescription.meshShader = meshShader;
    pipelineDescription.fragmentShader = fragmentShader;

    //pipelineDescription.outputs = swapChain->GetBackBuffer()->GetDesc();
    //pipelineDescription.outputs = fb->GetOutputDescription();
    _pipeline = factory.CreateMeshShaderPipeline(pipelineDescription);
}


void SimpleMeshShader::OnRenderFrame(alloy::IRenderCommandEncoder& pass) {
    auto timeElapsedSec = _runner->GetTimeService()->GetElapsedSeconds();

    auto* rndrSvc = _runner->GetRenderService();
    uint32_t fbWidth, fbHeight;
    rndrSvc->GetFrameBufferSize(fbWidth, fbHeight);

    //Update UBO
    UniformBufferObject ubo{};
    ubo.model = glm::rotate(
        glm::mat4(1.0f),
        timeElapsedSec * glm::radians(10.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.view = glm::lookAt(
        glm::vec3(10.0f, 10.0f, 10.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.proj = glm::perspective(
        glm::radians(45.0f),
        fbWidth / (float)fbHeight,
        0.1f, 1000.0f);

    memcpy(pUniformBuffer, &ubo, sizeof(ubo));

    //_commandList->BeginRenderPass(fb);
    pass.SetPipeline(_pipeline);
    pass.SetFullViewports();
    pass.SetFullScissorRects();

    //_commandList->ClearDepthStencil(0, 0);
    //_commandList->ClearColorTarget(0, 0.9, 0.1, 0.3, 1);
    pass.SetGraphicsResourceSet(_resSet);

    pass.DispatchMesh(8, 1, 1);
}

int main() {
    auto runner = IAppRunner::Create(800, 600, "Simple Quad");
    auto app = new SimpleMeshShader(runner);
    auto res = runner->Run(app);
    delete app;
    delete runner;
    return res;
}
