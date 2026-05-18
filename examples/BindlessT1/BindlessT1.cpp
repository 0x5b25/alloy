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

namespace BindlessT1Shader {
    #include "Shaders/BindlessT1_ps_6_0.h"
    #include "Shaders/BindlessT1_vs_6_0.h"
}

struct VertexData
{
    float position[2];
    float uv[2];
    float color[4];
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    std::uint32_t textureIndex;
    float padding[3];
};

class BindlessT1 : public IApp {
    static constexpr std::uint32_t TextureCapacity = 4;
    static constexpr std::uint32_t TextureWidth = 256;
    static constexpr std::uint32_t TextureHeight = 256;
    static constexpr std::uint32_t TextureSlotA = 1;
    static constexpr std::uint32_t TextureSlotB = 3;

    enum LayoutSlot : std::uint32_t {
        UniformLayoutSlot = 0,
        TextureArrayLayoutSlot = 1,
        SamplerLayoutSlot = 2
    };

    IAppRunner* _runner;

    alloy::common::sp<alloy::IGfxPipeline> _pipeline;
    alloy::common::sp<alloy::IBuffer> vertexBuffer;
    alloy::common::sp<alloy::IBuffer> indexBuffer;
    alloy::common::sp<alloy::IBuffer> uniformBuffer;
    void* pUniformBuffer = nullptr;

    std::array<alloy::common::sp<alloy::ITextureView>, TextureCapacity> textureViews;
    alloy::common::sp<alloy::ISampler> sampler;
    alloy::common::sp<alloy::IMutableResourceSet> _resSet;

    uint64_t _fenceVal = 0;
    alloy::common::sp<alloy::IEvent> _fence;

    static std::uint32_t PackRGBA(
        std::uint8_t r,
        std::uint8_t g,
        std::uint8_t b,
        std::uint8_t a = 0xff
    ) {
        return static_cast<std::uint32_t>(r)
            | (static_cast<std::uint32_t>(g) << 8)
            | (static_cast<std::uint32_t>(b) << 16)
            | (static_cast<std::uint32_t>(a) << 24);
    }

    template<typename T>
    void UpdateBuffer(
        const alloy::common::sp<alloy::IBuffer>& buffer,
        const T* data,
        size_t elementCnt
    ) {
        auto dev = _runner->GetRenderService()->GetDevice();
        constexpr std::uint32_t stagingBufferSizeInBytes = 512;
        const auto transferSizeInBytes =
            static_cast<std::uint32_t>(sizeof(T) * elementCnt);

        alloy::IBuffer::Description desc{};
        desc.sizeInBytes = stagingBufferSizeInBytes;
        desc.hostAccess = alloy::HostAccess::SystemMemoryPreferWrite;
        auto transferBuffer = dev->GetResourceFactory().CreateBuffer(desc);

        auto fence = dev->GetResourceFactory().CreateSyncEvent();
        uint64_t signalValue = 1;

        auto remainingSize = transferSizeInBytes;
        auto readPtr = reinterpret_cast<const std::uint8_t*>(data);
        auto writePtr = static_cast<std::uint8_t*>(transferBuffer->MapToCPU());
        assert(writePtr != nullptr);

        while (remainingSize > 0)
        {
            auto transferredSize = transferSizeInBytes - remainingSize;
            auto batchSize = std::min(stagingBufferSizeInBytes, remainingSize);
            memcpy(writePtr, readPtr + transferredSize, batchSize);

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
            dev->GetCopyCommandQueue()->EncodeSignalEvent(fence.get(), signalValue);
            fence->WaitFromCPU(signalValue);
            signalValue++;

            remainingSize -= batchSize;
        }

        transferBuffer->UnMap();
    }

    void _RequireVulkanT1();
    void _CreateResources();
    void _CreateBuffers();
    void _CreateTextures();
    void _CreatePipeline();

    alloy::common::sp<alloy::ITextureView> _CreateCheckerTexture(
        std::uint32_t colorA,
        std::uint32_t colorB,
        std::uint32_t blockSize,
        const char* debugName);

public:
    BindlessT1(IAppRunner* runner)
        : _runner(runner)
    {
        _CreateResources();
    }

    virtual ~BindlessT1() override {}

    virtual int GetExitCode() override { return 0; }

    virtual void FixedUpdate() override {}
    virtual void Update() override {}
    virtual void OnDrawGui() override {}

    virtual void OnRenderFrame(alloy::IRenderCommandEncoder& renderPass) override;

    virtual void OnFrameComplete(uint32_t frameIdx) {}
    virtual void OnFrameBegin(uint32_t frameIdx) {}
};

void BindlessT1::_RequireVulkanT1() {
    auto dev = _runner->GetRenderService()->GetDevice();
    const auto& adapterInfo = dev->GetAdapter().GetAdapterInfo();

    if(adapterInfo.apiVersion.backend != alloy::Backend::Vulkan) {
        throw std::runtime_error(
            "BindlessT1 currently targets Vulkan. Select the Vulkan backend before running this example.");
    }

    if(adapterInfo.resourceBindingModel == alloy::ResourceBindingModel::FixedBindings) {
        throw std::runtime_error(
            "BindlessT1 requires Vulkan descriptor indexing support.");
    }
}

void BindlessT1::_CreateResources() {
    _RequireVulkanT1();
    _fence = _runner->GetRenderService()->GetDevice()->GetResourceFactory().CreateSyncEvent();
    _CreateBuffers();
    _CreateTextures();
    _CreatePipeline();
}

void BindlessT1::_CreateBuffers() {
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
    vbDesc.sizeInBytes = 4 * sizeof(VertexData);
    vbDesc.usage.vertexBuffer = 1;
    vertexBuffer = factory.CreateBuffer(vbDesc);
    vertexBuffer->SetDebugName("BindlessT1 Vertex Buffer");
    UpdateBuffer(vertexBuffer, quadVertices.data(), quadVertices.size());

    alloy::IBuffer::Description ibDesc{};
    ibDesc.sizeInBytes = 4 * sizeof(std::uint32_t);
    ibDesc.usage.indexBuffer = 1;
    indexBuffer = factory.CreateBuffer(ibDesc);
    indexBuffer->SetDebugName("BindlessT1 Index Buffer");
    UpdateBuffer(indexBuffer, quadIndices.data(), quadIndices.size());

    alloy::IBuffer::Description ubDesc{};
    ubDesc.sizeInBytes = sizeof(UniformBufferObject);
    ubDesc.usage.uniformBuffer = 1;
    ubDesc.hostAccess = alloy::HostAccess::SystemMemoryPreferWrite;
    uniformBuffer = factory.CreateBuffer(ubDesc);
    uniformBuffer->SetDebugName("BindlessT1 Uniform Buffer");
    pUniformBuffer = uniformBuffer->MapToCPU();
}

alloy::common::sp<alloy::ITextureView> BindlessT1::_CreateCheckerTexture(
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

    const auto uploadPitchSrc = texDesc.width * 4;
    const auto uploadPitchDst = (uploadPitchSrc + 256 - 1u) & ~(256 - 1u);
    const auto uploadBufferSize = uploadPitchDst * texDesc.height;

    alloy::IBuffer::Description uploadDesc{};
    uploadDesc.sizeInBytes = uploadBufferSize;
    uploadDesc.usage.structuredBufferReadOnly = 1;
    uploadDesc.hostAccess = alloy::HostAccess::SystemMemoryPreferWrite;
    auto uploadBuffer = factory.CreateBuffer(uploadDesc);
    auto uploadRange = alloy::BufferRange::MakeByteBuffer(uploadBuffer);

    std::vector<std::uint32_t> pixels(TextureWidth * TextureHeight);
    for(std::uint32_t y = 0; y < TextureHeight; y++) {
        for(std::uint32_t x = 0; x < TextureWidth; x++) {
            const bool flipX = ((x / blockSize) & 1u) != 0;
            const bool flipY = ((y / blockSize) & 1u) != 0;
            pixels[y * TextureWidth + x] = (flipX ^ flipY) ? colorA : colorB;
        }
    }

    auto copyDst = static_cast<std::uint8_t*>(uploadBuffer->MapToCPU());
    assert(copyDst != nullptr);
    for(std::uint32_t y = 0; y < texDesc.height; y++) {
        memcpy(
            copyDst + y * uploadPitchDst,
            pixels.data() + y * texDesc.width,
            uploadPitchSrc);
    }
    uploadBuffer->UnMap();

    auto cmdList = dev->GetGfxCommandQueue()->CreateCommandList();
    cmdList->Begin();
    auto& pass = cmdList->BeginTransferPass();

    pass.CopyBufferToTexture(
        uploadRange,
        uploadPitchDst,
        uploadBufferSize,
        texture,
        {0, 0, 0},
        0,
        0,
        {texDesc.width, texDesc.height, 1});

    cmdList->EndPass();
    cmdList->End();

    dev->GetGfxCommandQueue()->SubmitCommand(cmdList.get());
    dev->GetGfxCommandQueue()->EncodeSignalEvent(_fence.get(), ++_fenceVal);
    _fence->WaitFromCPU(_fenceVal);

    return factory.CreateTextureView(texture);
}

void BindlessT1::_CreateTextures() {
    auto& factory = _runner->GetRenderService()->GetDevice()->GetResourceFactory();

    alloy::ISampler::Description samplerDesc {};
    samplerDesc.maximumAnisotropy = 1;
    sampler = factory.CreateSampler(samplerDesc);

    textureViews[TextureSlotA] = _CreateCheckerTexture(
        PackRGBA(20, 170, 225),
        PackRGBA(8, 34, 64),
        32,
        "BindlessT1 Texture Slot 1");

    textureViews[TextureSlotB] = _CreateCheckerTexture(
        PackRGBA(250, 210, 70),
        PackRGBA(120, 30, 165),
        16,
        "BindlessT1 Texture Slot 3");
}

void BindlessT1::_CreatePipeline() {
    auto* rndrSvc = _runner->GetRenderService();
    auto msaaSampleCount    = rndrSvc->GetFrameBufferSampleCount();
    auto renderTargetFormat = rndrSvc->GetFrameBufferColorFormat();
    auto depthStencilFormat = rndrSvc->GetFrameBufferDepthStencilFormat();

    auto& factory = rndrSvc->GetDevice()->GetResourceFactory();

    alloy::IResourceLayout::Description resLayoutDesc{};
    using ElemKind = alloy::IBindableResource::ResourceKind;
    using alloy::common::operator|;

    {
        auto& elem = resLayoutDesc.shaderResources.emplace_back();
        elem.bindingSlot = 0;
        elem.bindingCount = 1;
        elem.kind = ElemKind::UniformBuffer;
        elem.stages = alloy::IShader::Stage::Vertex | alloy::IShader::Stage::Fragment;
    }

    {
        auto& elem = resLayoutDesc.shaderResources.emplace_back();
        elem.bindingSlot = 1;
        elem.bindingCount = TextureCapacity;
        elem.kind = ElemKind::Texture;
        elem.stages = alloy::IShader::Stage::Fragment;
        elem.options.descriptorArray = 1;
    }

    {
        auto& elem = resLayoutDesc.shaderResources.emplace_back();
        elem.bindingSlot = 0;
        elem.bindingCount = 1;
        elem.kind = ElemKind::Sampler;
        elem.stages = alloy::IShader::Stage::Fragment;
    }

    auto layout = factory.CreateResourceLayout(resLayoutDesc);

    alloy::IMutableResourceSet::Description resSetDesc{};
    resSetDesc.layout = layout;
    {
        auto& write = resSetDesc.initialWrites.emplace_back();
        write.layoutSlot = UniformLayoutSlot;
        write.firstArrayElement = 0;
        write.resources.push_back(alloy::BufferRange::MakeByteBuffer(uniformBuffer));
    }
    {
        auto& write = resSetDesc.initialWrites.emplace_back();
        write.layoutSlot = TextureArrayLayoutSlot;
        write.firstArrayElement = TextureSlotA;
        write.resources.push_back(textureViews[TextureSlotA]);
    }
    {
        auto& write = resSetDesc.initialWrites.emplace_back();
        write.layoutSlot = SamplerLayoutSlot;
        write.firstArrayElement = 0;
        write.resources.push_back(sampler);
    }
    _resSet = factory.CreateMutableResourceSet(resSetDesc);

    alloy::IMutableResourceSet::WriteBinding textureUpdate{};
    textureUpdate.layoutSlot = TextureArrayLayoutSlot;
    textureUpdate.firstArrayElement = TextureSlotB;
    textureUpdate.resources.push_back(textureViews[TextureSlotB]);
    _resSet->Update(
        std::span<const alloy::IMutableResourceSet::WriteBinding>(&textureUpdate, 1));

    alloy::GraphicsPipelineDescription pipelineDescription{};
    pipelineDescription.resourceLayout = layout;
    pipelineDescription.attachmentState.colorAttachments = {
        alloy::AttachmentStateDescription::ColorAttachment::MakeOverrideBlend()
    };
    pipelineDescription.attachmentState.colorAttachments.front().format = renderTargetFormat;
    {
        alloy::AttachmentStateDescription::DepthStencilAttachment dsAttachment {};
        dsAttachment.depthStencilFormat = depthStencilFormat;

        pipelineDescription.attachmentState.depthStencilAttachment = dsAttachment;
    }
    pipelineDescription.attachmentState.sampleCount = msaaSampleCount;

    pipelineDescription.depthStencilState.depthTestEnabled = false;
    pipelineDescription.depthStencilState.depthWriteEnabled = true;
    pipelineDescription.depthStencilState.depthComparison = alloy::ComparisonKind::LessEqual;

    pipelineDescription.rasterizerState.cullMode = alloy::RasterizerStateDescription::FaceCullMode::Back;
    pipelineDescription.rasterizerState.fillMode = alloy::RasterizerStateDescription::PolygonFillMode::Solid;
    pipelineDescription.rasterizerState.frontFace = alloy::RasterizerStateDescription::FrontFace::Clockwise;
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

    auto fragmentShader = factory.CreateShader(fragmentShaderDesc, BindlessT1Shader::g_PSMain);
    auto vertexShader = factory.CreateShader(vertexShaderDesc, BindlessT1Shader::g_VSMain);

    pipelineDescription.shaderSet.vertexShader = vertexShader;
    pipelineDescription.shaderSet.fragmentShader = fragmentShader;

    _pipeline = factory.CreateGraphicsPipeline(pipelineDescription);
}

void BindlessT1::OnRenderFrame(alloy::IRenderCommandEncoder& pass) {
    auto timeElapsedSec = _runner->GetTimeService()->GetElapsedSeconds();

    auto* rndrSvc = _runner->GetRenderService();
    uint32_t fbWidth, fbHeight;
    rndrSvc->GetFrameBufferSize(fbWidth, fbHeight);

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(
        glm::mat4(1.0f),
        timeElapsedSec * glm::radians(90.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.view = glm::lookAt(
        glm::vec3(2.0f, 2.0f, 2.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.proj = glm::perspective(
        glm::radians(45.0f),
        fbWidth / static_cast<float>(fbHeight),
        0.1f,
        10.0f);
    ubo.textureIndex =
        (static_cast<std::uint32_t>(timeElapsedSec) & 1u) == 0
            ? TextureSlotA
            : TextureSlotB;

    memcpy(pUniformBuffer, &ubo, sizeof(ubo));

    pass.SetPipeline(_pipeline);
    pass.SetFullViewport();
    pass.SetFullScissorRect();

    pass.SetVertexBuffer(0, alloy::BufferRange::MakeByteBuffer(vertexBuffer));
    pass.SetIndexBuffer(alloy::BufferRange::MakeByteBuffer(indexBuffer), alloy::IndexFormat::UInt32);
    pass.SetGraphicsMutableResourceSet(_resSet);

    pass.DrawIndexed(
        /*indexCount:    */4,
        /*instanceCount: */1,
        /*indexStart:    */0,
        /*vertexOffset:  */0,
        /*instanceStart: */0);
}

int main() {
    auto runner = IAppRunner::Create(800, 600, "Bindless T1");
    auto app = new BindlessT1(runner);
    auto res = runner->Run(app);
    delete app;
    delete runner;
    return res;
}
