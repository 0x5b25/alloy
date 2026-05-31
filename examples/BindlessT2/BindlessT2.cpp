#include "IApp.hpp"
#include "alloy/Buffer.hpp"
#include "alloy/Types.hpp"

#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

namespace BindlessT2Shader {
#include "shaders/BindlessT2_ps_6_6.h"
#include "shaders/BindlessT2_vs_6_6.h"
}

namespace {

struct VertexData {
    float position[2];
    float uv[2];
    float color[4];
};

struct FrameConstants {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct PushConstants {
    std::uint32_t frameConstantsIndex;
    std::uint32_t textureIndex;
    std::uint32_t samplerIndex;
    std::uint32_t padding;
};

static constexpr std::uint32_t TextureViewCount = 3;
static constexpr std::uint32_t TextureWidth = 256;
static constexpr std::uint32_t TextureHeight = 256;

static constexpr std::uint32_t ResourceHeapCapacity = 8;
static constexpr std::uint32_t SamplerHeapCapacity = 1;

static constexpr alloy::ResourceDescriptorIndex FrameConstantsDescriptor{0};
static constexpr alloy::ResourceDescriptorIndex StableTextureDescriptor{1};
static constexpr alloy::ResourceDescriptorIndex DynamicTextureDescriptor{3};
static constexpr alloy::SamplerDescriptorIndex LinearSamplerDescriptor{0};

static constexpr std::uint32_t StableTextureView = 0;
static constexpr std::uint32_t DynamicTextureViewA = 1;
static constexpr std::uint32_t DynamicTextureViewB = 2;

static constexpr std::uint32_t PushConstantDwordCount =
    sizeof(PushConstants) / sizeof(std::uint32_t);

static_assert(sizeof(PushConstants) % sizeof(std::uint32_t) == 0);

class BindlessT2 : public IApp {
    IAppRunner* _runner;

    alloy::common::sp<alloy::IGfxPipeline> _pipeline;
    alloy::common::sp<alloy::IResourceLayout> _resourceLayout;

    alloy::common::sp<alloy::IBuffer> vertexBuffer;
    alloy::common::sp<alloy::IBuffer> indexBuffer;
    alloy::common::sp<alloy::IBuffer> frameConstantsBuffer;
    void* pFrameConstantsBuffer = nullptr;

    std::array<alloy::common::sp<alloy::ITextureView>, TextureViewCount> textureViews;
    alloy::common::sp<alloy::ISampler> sampler;
    alloy::common::sp<alloy::IResourceDescriptorHeap> resourceHeap;
    alloy::common::sp<alloy::ISamplerDescriptorHeap> samplerHeap;

    std::uint64_t _fenceVal = 0;
    alloy::common::sp<alloy::IEvent> _fence;

    static std::uint32_t PackRGBA(
        std::uint8_t r,
        std::uint8_t g,
        std::uint8_t b,
        std::uint8_t a = 0xff);

    template<typename T>
    void UpdateBuffer(
        const alloy::common::sp<alloy::IBuffer>& buffer,
        const T* data,
        size_t elementCnt);

    void _RequireT2();
    void _CreateResources();
    void _CreateBuffers();
    void _CreateTextures();
    void _CreateDescriptorHeaps();
    void _CreatePipeline();

    alloy::common::sp<alloy::ITextureView> _CreateCheckerTexture(
        std::uint32_t colorA,
        std::uint32_t colorB,
        std::uint32_t blockSize,
        const char* debugName);

public:
    BindlessT2(IAppRunner* runner);
    virtual ~BindlessT2() override;

    virtual int GetExitCode() override { return 0; }

    virtual void FixedUpdate() override {}
    virtual void Update() override {}

    virtual void OnDrawGui() override {}
    virtual void OnRenderFrame(alloy::IRenderCommandEncoder& renderPass) override;

    virtual void OnFrameComplete(std::uint32_t frameIdx) override {}
    virtual void OnFrameBegin(std::uint32_t frameIdx) override {}
};

std::uint32_t BindlessT2::PackRGBA(
    std::uint8_t r,
    std::uint8_t g,
    std::uint8_t b,
    std::uint8_t a
) {
    return static_cast<std::uint32_t>(r)
        | (static_cast<std::uint32_t>(g) << 8)
        | (static_cast<std::uint32_t>(b) << 16)
        | (static_cast<std::uint32_t>(a) << 24);
}

BindlessT2::BindlessT2(IAppRunner* runner)
    : _runner(runner)
{
    _CreateResources();
}

BindlessT2::~BindlessT2() {
    if(frameConstantsBuffer) {
        frameConstantsBuffer->UnMap();
    }
}

template<typename T>
void BindlessT2::UpdateBuffer(
    const alloy::common::sp<alloy::IBuffer>& buffer,
    const T* data,
    size_t elementCnt
) {
    constexpr std::uint32_t stagingBufferSizeInBytes = 512;

    auto dev = _runner->GetRenderService()->GetDevice();

    alloy::IBuffer::Description desc{};
    desc.sizeInBytes = stagingBufferSizeInBytes;
    desc.hostAccess = alloy::HostAccess::SystemMemoryPreferWrite;

    auto transferBuffer = dev->GetResourceFactory().CreateBuffer(desc);
    auto fence = dev->GetResourceFactory().CreateSyncEvent();

    auto writePtr = static_cast<std::uint8_t*>(transferBuffer->MapToCPU());
    auto readPtr = reinterpret_cast<const std::uint8_t*>(data);

    size_t transferredSize = 0;
    size_t dataSize = sizeof(T) * elementCnt;
    std::uint64_t signalValue = 0;

    while(transferredSize < dataSize) {
        const size_t batchSize = std::min<size_t>(
            stagingBufferSizeInBytes,
            dataSize - transferredSize);

        std::memcpy(writePtr, readPtr + transferredSize, batchSize);

        auto cmd = dev->GetCopyCommandQueue()->CreateCommandList();
        cmd->Begin();

        auto& pass = cmd->BeginTransferPass();
        pass.CopyBuffer(
            alloy::BufferRange::MakeByteBuffer(transferBuffer),
            alloy::BufferRange::MakeByteBuffer(buffer, transferredSize, batchSize),
            batchSize);

        cmd->EndPass();
        cmd->End();

        dev->GetCopyCommandQueue()->SubmitCommand(cmd.get());
        dev->GetCopyCommandQueue()->EncodeSignalEvent(fence.get(), ++signalValue);
        fence->WaitFromCPU(signalValue);

        transferredSize += batchSize;
    }

    transferBuffer->UnMap();
}

void BindlessT2::_RequireT2() {
    auto dev = _runner->GetRenderService()->GetDevice();
    const auto& adapterInfo = dev->GetAdapter().GetAdapterInfo();

    if(adapterInfo.resourceBindingModel != alloy::ResourceBindingModel::T2) {
        throw std::runtime_error(
            "BindlessT2 requires T2 descriptor heap bindless support.");
    }
}

void BindlessT2::_CreateResources() {
    _RequireT2();

    _fence = _runner->GetRenderService()->GetDevice()
        ->GetResourceFactory().CreateSyncEvent();

    _CreateBuffers();
    _CreateTextures();
    _CreateDescriptorHeaps();
    _CreatePipeline();
}

void BindlessT2::_CreateBuffers() {
    auto& factory = _runner->GetRenderService()->GetDevice()->GetResourceFactory();

    std::vector<VertexData> quadVertices
    {
        {{-0.75f, 0.75f},  {0, 0}, {1.f, 1.f, 1.f, 1.f}},
        {{0.75f, 0.75f},   {1, 0}, {1.f, 1.f, 1.f, 1.f}},
        {{-0.75f, -0.75f}, {0, 1}, {1.f, 1.f, 1.f, 1.f}},
        {{0.75f, -0.75f},  {1, 1}, {1.f, 1.f, 1.f, 1.f}}
    };

    std::vector<std::uint32_t> quadIndices { 0, 1, 2, 3 };

    alloy::IBuffer::Description vbDesc{};
    vbDesc.sizeInBytes = quadVertices.size() * sizeof(VertexData);
    vbDesc.usage.vertexBuffer = 1;
    vertexBuffer = factory.CreateBuffer(vbDesc);
    vertexBuffer->SetDebugName("BindlessT2 Vertex Buffer");
    UpdateBuffer(vertexBuffer, quadVertices.data(), quadVertices.size());

    alloy::IBuffer::Description ibDesc{};
    ibDesc.sizeInBytes = quadIndices.size() * sizeof(std::uint32_t);
    ibDesc.usage.indexBuffer = 1;
    indexBuffer = factory.CreateBuffer(ibDesc);
    indexBuffer->SetDebugName("BindlessT2 Index Buffer");
    UpdateBuffer(indexBuffer, quadIndices.data(), quadIndices.size());

    alloy::IBuffer::Description fcDesc{};
    fcDesc.sizeInBytes = sizeof(FrameConstants);
    fcDesc.usage.uniformBuffer = 1;
    fcDesc.hostAccess = alloy::HostAccess::SystemMemoryPreferWrite;
    frameConstantsBuffer = factory.CreateBuffer(fcDesc);
    frameConstantsBuffer->SetDebugName("BindlessT2 Frame Constants");
    pFrameConstantsBuffer = frameConstantsBuffer->MapToCPU();
}

alloy::common::sp<alloy::ITextureView> BindlessT2::_CreateCheckerTexture(
    std::uint32_t colorA,
    std::uint32_t colorB,
    std::uint32_t blockSize,
    const char* debugName
) {
    auto dev = _runner->GetRenderService()->GetDevice();
    auto& factory = dev->GetResourceFactory();

    alloy::ITexture::Description texDesc{};
    texDesc.width = TextureWidth;
    texDesc.height = TextureHeight;
    texDesc.depth = 1;
    texDesc.mipLevels = 1;
    texDesc.arrayLayers = 1;
    texDesc.format = alloy::PixelFormat::R8_G8_B8_A8_UNorm;
    texDesc.usage.sampled = 1;
    texDesc.type = alloy::ITexture::Description::Type::Texture2D;
    texDesc.sampleCount = alloy::SampleCount::x1;
    texDesc.hostAccess = alloy::HostAccess::PreferDeviceMemory;

    auto texture = factory.CreateTexture(texDesc);
    texture->SetDebugName(debugName);
    auto texView = factory.CreateTextureView(texture);

    const auto uploadPitchSrc = texDesc.width * 4;
    const auto uploadPitchDst = (uploadPitchSrc + 256 - 1u) & ~(256 - 1u);
    const auto uploadBufferSize = uploadPitchDst * texDesc.height;

    alloy::IBuffer::Description uploadDesc{};
    uploadDesc.sizeInBytes = uploadBufferSize;
    uploadDesc.usage.structuredBufferReadOnly = 1;
    uploadDesc.hostAccess = alloy::HostAccess::SystemMemoryPreferWrite;

    auto uploadBuffer = factory.CreateBuffer(uploadDesc);
    auto uploadPtr = static_cast<std::uint8_t*>(uploadBuffer->MapToCPU());

    for(std::uint32_t y = 0; y < texDesc.height; ++y) {
        auto row = reinterpret_cast<std::uint32_t*>(uploadPtr + y * uploadPitchDst);

        for(std::uint32_t x = 0; x < texDesc.width; ++x) {
            const bool useA = (((x / blockSize) + (y / blockSize)) & 1u) == 0;
            row[x] = useA ? colorA : colorB;
        }
    }

    uploadBuffer->UnMap();

    auto uploadRange = alloy::BufferRange::MakeByteBuffer(uploadBuffer);

    alloy::BarrierOp barrier[1] = {{alloy::TextureBarrierOp{
        .texture = texView,
        .from = {
            .stages = {},
            .access = {},
            .layout = alloy::TextureLayout::Undefined,
        },
        .to = {
            .stages = alloy::PipelineStage::Copy,
            .access = alloy::ResourceAccess::CopyDest,
            .layout = alloy::TextureLayout::CopyDest,
        }
    }}};

    auto cmdList = dev->GetGfxCommandQueue()->CreateCommandList();
    cmdList->Begin();
    cmdList->Barrier(barrier);

    auto& pass = cmdList->BeginTransferPass();
    pass.CopyBufferToTexture(
        uploadRange,
        uploadPitchDst,
        uploadBufferSize,
        texView,
        {0, 0, 0},
        0,
        0,
        {texDesc.width, texDesc.height, 1});

    cmdList->EndPass();

    barrier[0] = {alloy::TextureBarrierOp{
        .texture = texView,
        .from = {
            .stages = alloy::PipelineStage::Copy,
            .access = alloy::ResourceAccess::CopyDest,
            .layout = alloy::TextureLayout::CopyDest,
        },
        .to = {
            .stages = alloy::PipelineStage::AllCommands,
            .access = alloy::ResourceAccess::ShaderResourceRead,
            .layout = alloy::TextureLayout::ShaderReadOnly,
        }
    }};

    cmdList->Barrier(barrier);
    cmdList->End();

    dev->GetGfxCommandQueue()->SubmitCommand(cmdList.get());
    dev->GetGfxCommandQueue()->EncodeSignalEvent(_fence.get(), ++_fenceVal);
    _fence->WaitFromCPU(_fenceVal);

    return texView;
}

void BindlessT2::_CreateTextures() {
    auto& factory = _runner->GetRenderService()->GetDevice()->GetResourceFactory();

    alloy::ISampler::Description samplerDesc {};
    samplerDesc.maximumAnisotropy = 1;
    sampler = factory.CreateSampler(samplerDesc);
    sampler->SetDebugName("BindlessT2 Linear Sampler");

    textureViews[StableTextureView] = _CreateCheckerTexture(
        PackRGBA(20, 170, 225),
        PackRGBA(8, 34, 64),
        32,
        "BindlessT2 Texture View 0");

    textureViews[DynamicTextureViewA] = _CreateCheckerTexture(
        PackRGBA(250, 210, 70),
        PackRGBA(120, 30, 165),
        16,
        "BindlessT2 Texture View 1");

    textureViews[DynamicTextureViewB] = _CreateCheckerTexture(
        PackRGBA(70, 230, 120),
        PackRGBA(105, 40, 25),
        8,
        "BindlessT2 Texture View 2");
}

void BindlessT2::_CreateDescriptorHeaps() {
    auto& factory = _runner->GetRenderService()->GetDevice()->GetResourceFactory();

    alloy::IResourceDescriptorHeap::Description resourceHeapDesc{};
    resourceHeapDesc.capacity = ResourceHeapCapacity;
    resourceHeap = factory.CreateResourceDescriptorHeap(resourceHeapDesc);

    alloy::ISamplerDescriptorHeap::Description samplerHeapDesc{};
    samplerHeapDesc.capacity = SamplerHeapCapacity;
    samplerHeap = factory.CreateSamplerDescriptorHeap(samplerHeapDesc);

    resourceHeap->Write(
        FrameConstantsDescriptor,
        alloy::UniformBufferDescriptor{
            alloy::BufferRange::MakeByteBuffer(frameConstantsBuffer)
        });

    resourceHeap->Write(
        StableTextureDescriptor,
        alloy::SampledTextureDescriptor{
            textureViews[StableTextureView]
        });

    resourceHeap->Write(
        DynamicTextureDescriptor,
        alloy::SampledTextureDescriptor{
            textureViews[DynamicTextureViewA]
        });

    samplerHeap->Write(LinearSamplerDescriptor, sampler);
}

void BindlessT2::_CreatePipeline() {
    auto* rndrSvc = _runner->GetRenderService();
    auto& factory = rndrSvc->GetDevice()->GetResourceFactory();

    alloy::IResourceLayout::Description resLayoutDesc{};
    resLayoutDesc.useGlobalHeaps = true;
    resLayoutDesc.pushConstants.push_back({
        .bindingSlot = 0,
        .bindingSpace = 0,
        .sizeInDwords = PushConstantDwordCount,
    });

    _resourceLayout = factory.CreateResourceLayout(resLayoutDesc);

    auto msaaSampleCount = rndrSvc->GetFrameBufferSampleCount();
    auto renderTargetFormat = rndrSvc->GetFrameBufferColorFormat();
    auto depthStencilFormat = rndrSvc->GetFrameBufferDepthStencilFormat();

    alloy::GraphicsPipelineDescription pipelineDescription{};
    pipelineDescription.resourceLayout = _resourceLayout;

    pipelineDescription.attachmentState.colorAttachments = {
        alloy::AttachmentStateDescription::ColorAttachment::MakeOverrideBlend()
    };
    pipelineDescription.attachmentState.colorAttachments.front().format = renderTargetFormat;

    alloy::AttachmentStateDescription::DepthStencilAttachment dsAttachment {};
    dsAttachment.depthStencilFormat = depthStencilFormat;
    pipelineDescription.attachmentState.depthStencilAttachment = dsAttachment;

    pipelineDescription.attachmentState.sampleCount = msaaSampleCount;

    pipelineDescription.depthStencilState.depthTestEnabled = false;
    pipelineDescription.depthStencilState.depthWriteEnabled = true;
    pipelineDescription.depthStencilState.depthComparison = alloy::ComparisonKind::LessEqual;

    pipelineDescription.rasterizerState.cullMode =
        alloy::RasterizerStateDescription::FaceCullMode::Back;
    pipelineDescription.rasterizerState.fillMode =
        alloy::RasterizerStateDescription::PolygonFillMode::Solid;
    pipelineDescription.rasterizerState.frontFace =
        alloy::RasterizerStateDescription::FrontFace::Clockwise;
    pipelineDescription.rasterizerState.depthClipEnabled = true;
    pipelineDescription.rasterizerState.scissorTestEnabled = false;

    pipelineDescription.primitiveTopology = alloy::PrimitiveTopology::TriangleStrip;

    pipelineDescription.shaderSet.vertexLayouts = { {} };
    pipelineDescription.shaderSet.vertexLayouts[0].SetElements({
        {"POSITION", {alloy::VertexInputSemantic::Name::Position, 0}, alloy::ShaderDataType::Float2},
        {"TEXCOORD", {alloy::VertexInputSemantic::Name::TextureCoordinate, 0}, alloy::ShaderDataType::Float2},
        {"COLOR", {alloy::VertexInputSemantic::Name::Color, 0}, alloy::ShaderDataType::Float4}
    });

    alloy::IShader::Description vertexShaderDesc{};
    vertexShaderDesc.stage = alloy::IShader::Stage::Vertex;
    vertexShaderDesc.entryPoint = "VSMain";

    alloy::IShader::Description fragmentShaderDesc{};
    fragmentShaderDesc.stage = alloy::IShader::Stage::Fragment;
    fragmentShaderDesc.entryPoint = "PSMain";

    auto fragmentShader = factory.CreateShader(
        fragmentShaderDesc,
        BindlessT2Shader::g_PSMain);
    auto vertexShader = factory.CreateShader(
        vertexShaderDesc,
        BindlessT2Shader::g_VSMain);

    pipelineDescription.shaderSet.vertexShader = vertexShader;
    pipelineDescription.shaderSet.fragmentShader = fragmentShader;

    _pipeline = factory.CreateGraphicsPipeline(pipelineDescription);
}

void BindlessT2::OnRenderFrame(alloy::IRenderCommandEncoder& pass) {
    auto timeElapsedSec = _runner->GetTimeService()->GetElapsedSeconds();

    auto* rndrSvc = _runner->GetRenderService();
    std::uint32_t fbWidth, fbHeight;
    rndrSvc->GetFrameBufferSize(fbWidth, fbHeight);

    FrameConstants frameConstants{};
    frameConstants.model = glm::rotate(
        glm::mat4(1.0f),
        timeElapsedSec * glm::radians(90.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));

    frameConstants.view = glm::lookAt(
        glm::vec3(2.0f, 2.0f, 2.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));

    frameConstants.proj = glm::perspective(
        glm::radians(45.0f),
        fbWidth / static_cast<float>(fbHeight),
        0.1f,
        10.0f);

    std::memcpy(pFrameConstantsBuffer, &frameConstants, sizeof(frameConstants));

    const auto tick = static_cast<std::uint32_t>(timeElapsedSec);
    const auto dynamicTextureView =
        ((tick / 2u) & 1u) == 0
            ? DynamicTextureViewA
            : DynamicTextureViewB;

    resourceHeap->Write(
        DynamicTextureDescriptor,
        alloy::SampledTextureDescriptor{
            textureViews[dynamicTextureView]
        });

    PushConstants pushConstants{};
    pushConstants.frameConstantsIndex = FrameConstantsDescriptor.value;
    pushConstants.textureIndex =
        (tick & 1u) == 0
            ? StableTextureDescriptor.value
            : DynamicTextureDescriptor.value;
    pushConstants.samplerIndex = LinearSamplerDescriptor.value;

    std::array<std::uint32_t, PushConstantDwordCount> pushConstantData {
        pushConstants.frameConstantsIndex,
        pushConstants.textureIndex,
        pushConstants.samplerIndex,
        pushConstants.padding,
    };

    pass.SetDescriptorHeaps(resourceHeap, samplerHeap);
    
    pass.SetPipeline(_pipeline);
    pass.SetFullViewport();
    pass.SetFullScissorRect();

    pass.SetVertexBuffer(0, alloy::BufferRange::MakeByteBuffer(vertexBuffer));
    pass.SetIndexBuffer(
        alloy::BufferRange::MakeByteBuffer(indexBuffer),
        alloy::IndexFormat::UInt32);

    pass.SetPushConstants(0, pushConstantData, 0);

    pass.DrawIndexed(
        4,
        1,
        0,
        0,
        0);
}

}

int main() {
    auto runner = IAppRunner::Create(800, 600, "Bindless T2");
    auto app = new BindlessT2(runner);
    auto res = runner->Run(app);
    delete app;
    delete runner;
    return res;
}
