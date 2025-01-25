#include "ImGuiBackend.hpp"

#include <alloy/Pipeline.hpp>
#include <alloy/CommandList.hpp>
#include <alloy/CommandQueue.hpp>
#include <alloy/ResourceFactory.hpp>
#include <alloy/Texture.hpp>
#include "HLSLCompiler.hpp"

using namespace alloy;

struct VERTEX_CONSTANT_BUFFER_DX12
{
    float   mvp[4][4];
};

const char HLSLCode[] = R"AGAN(

cbuffer vertexBuffer : register(b0)
{
    float4x4 ProjectionMatrix;
};
struct VS_INPUT
{
    float2 pos : POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

SamplerState sampler0 : register(s0);
Texture2D texture0 : register(t0);

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));
    output.col = input.col;
    output.uv  = input.uv;
    return output;
}

float4 PSMain(PS_INPUT input) : SV_Target
{
    float4 out_col = input.col * texture0.Sample(sampler0, input.uv);
    return out_col;
}

)AGAN";

static std::vector<std::uint8_t> CompileShader(
    const std::string& src,
    const std::wstring& entry,
    ShaderType shaderType
) {
    auto pComplier = IHLSLCompiler::Create();

    std::vector<std::uint8_t> dxil;

    try {
        dxil = pComplier->Compile(src, entry, shaderType);
    } catch (const ShaderCompileError& e){
        throw e;
    }
    
    delete pComplier;

    return dxil;
}


// Functions


ImGuiAlloyBackend::ImGuiAlloyBackend(
    const alloy::common::sp<alloy::IGraphicsDevice>& gd
) 
    : gd(gd)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();

    _CreateDeviceObjects();

    io.BackendRendererUserData = (void*)this;
    io.BackendRendererName = "imgui_impl_dx12";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

}


ImGuiAlloyBackend::~ImGuiAlloyBackend() {
    ImGuiIO& io = ImGui::GetIO();

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
}

void ImGuiAlloyBackend::_SetupRenderState(
    ImDrawData* draw_data,
    alloy::IRenderCommandEncoder* command_list
)  {

    // Setup orthographic projection matrix into our constant buffer
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).
    // Setup orthographic projection matrix into our constant buffer
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).
    {
        float L = draw_data->DisplayPos.x;
        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        float T = draw_data->DisplayPos.y;
        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
        float mvp[4][4] =
        {
            { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
            { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
            { 0.0f,         0.0f,           0.5f,       0.0f },
            { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
        };

        auto pMvpBuf = mvpBuffer->MapToCPU();

        memcpy(pMvpBuf, mvp, sizeof(mvp));
        mvpBuffer->UnMap();
    }

    // Setup viewport
    alloy::Viewport vp[] = {{
        .x = 0,
        .y = 0,
        .width = draw_data->DisplaySize.x,
        .height = draw_data->DisplaySize.y,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    }};
    command_list->SetViewports(vp);

    // Bind shader and vertex buffers
    command_list->SetPipeline(pipeline);
    command_list->SetGraphicsResourceSet(fontRS);
    //command_list->SetGraphicsRoot32BitConstants(0, 16, &vertex_constant_buffer, 0);

    command_list->SetVertexBuffer(0, BufferRange::MakeByteBuffer(vertBuffer));

    auto indexFormat = sizeof(ImDrawIdx) == 2 ? IndexFormat::UInt16 : IndexFormat::UInt32;
    command_list->SetIndexBuffer(BufferRange::MakeByteBuffer(indexBuffer), indexFormat);
}


void ImGuiAlloyBackend::RenderDrawData(
    ImDrawData* draw_data, 
    const alloy::common::sp<alloy::IRenderTarget>& rt,
    alloy::ICommandList* command_list
) {
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    // FIXME: We are assuming that this only gets called once per frame!
    //ImGui_ImplDX12_Data* bd = ImGui_ImplDX12_GetBackendData();
    //bd->frameIndex = bd->frameIndex + 1;
    //ImGui_ImplDX12_RenderBuffers* fr = &bd->pFrameResources[bd->frameIndex % bd->numFramesInFlight];

    // Create and grow vertex/index buffers if needed
    auto requiredVertBufferSize = draw_data->TotalVtxCount * sizeof(ImDrawVert);
    if (!vertBuffer || vertBuffer->GetDesc().sizeInBytes < requiredVertBufferSize)
    {
        vertBuffer.reset();
        auto nextSize = requiredVertBufferSize + 5000* sizeof(ImDrawVert);

        alloy::IBuffer::Description vbDesc{};
        vbDesc.sizeInBytes = nextSize;
        vbDesc.usage.vertexBuffer = 1;
        vbDesc.hostAccess = alloy::HostAccess::PreferWrite;
        vertBuffer = gd->GetResourceFactory().CreateBuffer(vbDesc);
        vertBuffer->SetDebugName("Vertex Buffer");
    }

    auto requiredIndexBufferSize = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
    if (!indexBuffer || indexBuffer->GetDesc().sizeInBytes < requiredIndexBufferSize)
    {
        indexBuffer.reset();
        auto nextSize = requiredIndexBufferSize + 10000* sizeof(ImDrawIdx);

        alloy::IBuffer::Description ibDesc{};
        ibDesc.sizeInBytes = nextSize;
        ibDesc.usage.indexBuffer = 1;
        ibDesc.hostAccess = alloy::HostAccess::PreferWrite;
        indexBuffer = gd->GetResourceFactory().CreateBuffer(ibDesc);
        indexBuffer->SetDebugName("Index Buffer");
    }

    // Upload vertex/index data into a single contiguous GPU buffer
    // During Map() we specify a null read range (as per DX12 API, this is informational and for tooling only)
    void* vtx_resource, *idx_resource;
    if ((vtx_resource = vertBuffer->MapToCPU()) == nullptr)
        return;
    if ((idx_resource = indexBuffer->MapToCPU()) == nullptr)
        return;
    ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource;
    ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* draw_list = draw_data->CmdLists[n];
        memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += draw_list->VtxBuffer.Size;
        idx_dst += draw_list->IdxBuffer.Size;
    }

    vertBuffer->UnMap();
    indexBuffer->UnMap();

    //Begin renderpass
    RenderPassAction passAction {};
    auto& ctAct = passAction.colorTargetActions.emplace_back();
    ctAct.loadAction = alloy::LoadAction::Load;
    ctAct.storeAction = alloy::StoreAction::Store;
    ctAct.clearColor = {0.1, 0.1, 0.1, 1};
    ctAct.target = rt;
    auto& enc = command_list->BeginRenderPass(passAction);

    // Setup desired DX state
    _SetupRenderState(draw_data, &enc);

    // Setup render state structure (for callbacks and custom texture bindings)
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    ImGui_ImplAlloy_RenderState render_state { };
    render_state.pBackend = this;
    render_state.pCmdList = &enc;
    platform_io.Renderer_RenderState = &render_state;

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    ImVec2 clip_off = draw_data->DisplayPos;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* draw_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    _SetupRenderState(draw_data, &enc);
                else
                    pcmd->UserCallback(draw_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
                ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                alloy::Rect r[] = {{ clip_min.x, clip_min.y, clip_max.x - clip_min.x, clip_max.y - clip_min.y}};
                enc.SetScissorRects(r);

                // Bind texture, Draw
                //D3D12_GPU_DESCRIPTOR_HANDLE texture_handle = {};
                auto texRS = (alloy::IResourceSet*)pcmd->GetTexID();
                enc.SetGraphicsResourceSet(common::ref_sp(texRS));
                enc.DrawIndexed(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
            }
        }
        global_idx_offset += draw_list->IdxBuffer.Size;
        global_vtx_offset += draw_list->VtxBuffer.Size;
    }

    command_list->EndPass();
    platform_io.Renderer_RenderState = nullptr;
}


void ImGuiAlloyBackend::_CreateRenderPipeline() {

    alloy::IResourceLayout::Description resLayoutDesc{};
    using ElemKind = alloy::IBindableResource::ResourceKind;
    using alloy::common::operator|;
    //resLayoutDesc.elements.resize(3, {});
    {
        auto& elem = resLayoutDesc.elements.emplace_back();
        elem.name = "ObjectUniform";
        elem.bindingSlot = 0;
        elem.kind = ElemKind::UniformBuffer;
        elem.stages = alloy::IShader::Stage::Vertex | alloy::IShader::Stage::Fragment;
    }
    //
    //{
    //    auto& elem = resLayoutDesc.elements.emplace_back();
    //    elem.name = "Struct";
    //    elem.bindingSlot = 0;
    //    elem.kind = ElemKind::StorageBuffer;
    //    elem.stages = alloy::IShader::Stage::Vertex | alloy::IShader::Stage::Fragment;
    //}

    {
        auto& elem = resLayoutDesc.elements.emplace_back();
        elem.name = "tex1";
        elem.bindingSlot = 0;
        elem.kind = ElemKind::Texture;
        elem.stages = alloy::IShader::Stage::Fragment;
    }

    {
        auto& elem = resLayoutDesc.elements.emplace_back();
        elem.name = "samp1";
        elem.bindingSlot = 0;
        elem.kind = ElemKind::Sampler;
        elem.stages = alloy::IShader::Stage::Fragment;
    }

    pipelineLayout =  gd->GetResourceFactory().CreateResourceLayout(resLayoutDesc);

    GraphicsPipelineDescription psoDesc { };
    psoDesc.primitiveTopology =  PrimitiveTopology::TriangleList;
    psoDesc.resourceLayout = pipelineLayout;
    //psoDesc.RTVFormats[0] = bd->RTVFormat;
    //psoDesc.DSVFormat = bd->DSVFormat;
    psoDesc.outputs.sampleCount = SampleCount::x1;

    // Create the vertex shader
    {
        auto dxil = CompileShader(HLSLCode, L"VSMain", ShaderType::VS);
        IShader::Description sd {};
        sd.stage = IShader::Stage::Vertex;
        sd.entryPoint = "VSMain";
        sd.enableDebug = true;
        auto vs = gd->GetResourceFactory().CreateShader(sd, dxil);

        // Create the input layout
        psoDesc.shaderSet.vertexLayouts = { {} };
        psoDesc.shaderSet.vertexLayouts[0].SetElements({
                {"POSITION", {alloy::VertexInputSemantic::Name::Position, 0},          alloy::ShaderDataType::Float2},
                {"TEXCOORD", {alloy::VertexInputSemantic::Name::TextureCoordinate, 0}, alloy::ShaderDataType::Float2},
                {"COLOR",    {alloy::VertexInputSemantic::Name::Color, 0},             alloy::ShaderDataType::Byte4_Norm}
            });
        psoDesc.shaderSet.vertexShader = vs;
    }

    // Create the pixel shader
    {
        auto dxil = CompileShader(HLSLCode, L"PSMain", ShaderType::PS);
        IShader::Description sd {};
        sd.stage = IShader::Stage::Fragment;
        sd.entryPoint = "PSMain";
        sd.enableDebug = true;
        auto fs = gd->GetResourceFactory().CreateShader(sd, dxil);

        psoDesc.shaderSet.fragmentShader = fs;
    }

    // Create the blending setup
    {
        auto& desc = psoDesc.attachmentState;
        desc.alphaToCoverageEnabled = false;
        desc.colorAttachments = {
            AttachmentStateDescription::ColorAttachment::MakeAlphaBlend()
        };
        desc.colorAttachments[0].format = PixelFormat::R8_G8_B8_A8_UNorm;
        //desc.depthStencilAttachment
    }

    // Create the rasterizer state
    {
        auto& desc = psoDesc.rasterizerState;
        desc.fillMode = RasterizerStateDescription::PolygonFillMode::Solid;
        desc.cullMode = RasterizerStateDescription::FaceCullMode::None;
        desc.frontFace = RasterizerStateDescription::FrontFace::CounterClockwise;
        //desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        //desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        //desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        desc.depthClipEnabled = true;
        //desc.MultisampleEnable = FALSE;
        //desc.AntialiasedLineEnable = FALSE;
        //desc.ForcedSampleCount = 0;
        //desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    }

    // Create depth-stencil State
    {
        auto& desc = psoDesc.depthStencilState;
        desc.depthTestEnabled = false;
        //desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        desc.depthComparison = ComparisonKind::Always;
        desc.stencilTestEnabled = false;
        desc.stencilFront.fail = desc.stencilFront.depthFail = desc.stencilFront.pass 
            = DepthStencilStateDescription::StencilBehavior::Operation::Keep;
        desc.stencilFront.comparison = ComparisonKind::Always;
        desc.stencilBack = desc.stencilFront;
    }

    pipeline = gd->GetResourceFactory().CreateGraphicsPipeline(psoDesc);
    
    assert(pipeline);
}


void ImGuiAlloyBackend::_CreateBuffers() {
    auto& factory = gd->GetResourceFactory();

    //std::vector<VertexData> quadVertices
    //{
    //    {{-0.75f, 0.75f},  {0, 0}, {0.f, 0.f, 1.f, 1.f}},
    //    {{0.75f, 0.75f},   {1, 0}, {0.f, 1.f, 0.f, 1.f}},
    //    {{-0.75f, -0.75f}, {0, 1}, {1.f, 0.f, 0.f, 1.f}},
    //    {{0.75f, -0.75f},  {1, 1}, {1.f, 1.f, 0.f, 1.f}}
    //};
    //
    //std::vector<std::uint32_t> quadIndices { 0, 1, 2, 3 };
    //
    //
    //alloy::IBuffer::Description _vbDesc{};
    //_vbDesc.sizeInBytes = 4 * sizeof(VertexData);
    //_vbDesc.usage.vertexBuffer = 1;
    ////_vbDesc.usage.staging = 1;
    //vertexBuffer = factory.CreateBuffer(_vbDesc);
    //vertexBuffer->SetDebugName("Vertex Buffer");
    //UpdateBuffer(vertexBuffer, quadVertices.data(), quadVertices.size());
    //
    //alloy::IBuffer::Description _ibDesc{};
    //_ibDesc.sizeInBytes = 4 * sizeof(std::uint32_t);
    //_ibDesc.usage.indexBuffer = 1;
    ////_ibDesc.usage.staging = 1;
    //indexBuffer = factory.CreateBuffer(_ibDesc);
    //indexBuffer->SetDebugName("Index Buffer");
    //UpdateBuffer(indexBuffer, quadIndices.data(), quadIndices.size());

    alloy::IBuffer::Description _ubDesc{};
    _ubDesc.sizeInBytes = sizeof(VERTEX_CONSTANT_BUFFER_DX12);
    _ubDesc.usage.uniformBuffer = 1;
    //_ubDesc.usage.staging = 1;
    _ubDesc.hostAccess = alloy::HostAccess::PreferWrite;
    mvpBuffer = factory.CreateBuffer(_ubDesc);
    mvpBuffer->SetDebugName("Uniform Buffer");
}

void ImGuiAlloyBackend::_CreateFontsTexture() {
    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    //ImGui_ImplDX12_Data* bd = ImGui_ImplDX12_GetBackendData();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    //Create the texture
    common::sp<ITexture> fontTex;
    {
        ITexture::Description desc {};

        desc.type = alloy::ITexture::Description::Type::Texture2D;
        desc.width = width;
        desc.height = height;
        desc.depth = 1;
        desc.mipLevels = 1;
        desc.arrayLayers = 1;
        desc.sampleCount = alloy::SampleCount::x1;
        desc.hostAccess = alloy::HostAccess::PreferDeviceMemory;
        desc.usage.sampled = 1;
        desc.format = alloy::PixelFormat::R8_G8_B8_A8_UNorm;

        fontTex = gd->GetResourceFactory().CreateTexture(desc);
    }

    //Command list sync objects
    auto fence = gd->GetResourceFactory().CreateSyncEvent();
    uint64_t signalValue = 1;

    //Layout transition to CPU writable
    {
        auto cmd = gd->GetGfxCommandQueue()->CreateCommandList();
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
                    .resource = fontTex
                }
                //.barriers = { texBarrier, dsBarrier }
            };
            cmd->Barrier({barrier});
        }

        cmd->End();

        //submit and wait
        gd->GetGfxCommandQueue()->SubmitCommand(cmd.get());
        gd->GetGfxCommandQueue()->EncodeSignalEvent(fence.get(), signalValue);
        fence->WaitFromCPU(signalValue);
        signalValue++;
    }

    //Upload resource to texture
    {
        fontTex->WriteSubresource(
            0, 0,   //mip level, array layer
            {0,0,0}, //dst origin
            {(uint32_t)width, (uint32_t)height, 1}, //{ w, h, d }
            pixels,
            width * 4,
            width * height * 4
        );
    }

    //Layout transition to SRV
    {
        auto cmd = gd->GetGfxCommandQueue()->CreateCommandList();
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
                    .resource = fontTex
                }
                //.barriers = { texBarrier, dsBarrier }
            };
            cmd->Barrier({barrier});
        }
        cmd->End();
        //submit and wait
        gd->GetGfxCommandQueue()->SubmitCommand(cmd.get());
        gd->GetGfxCommandQueue()->EncodeSignalEvent(fence.get(), signalValue);
        fence->WaitFromCPU(signalValue);
    }

    //Create sampler
    common::sp<ISampler> samp;
    {
        // Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling.
        ISampler::Description desc {};
        desc.filter = ISampler::Description::SamplerFilter::MinLinear_MagLinear_MipLinear;
        desc.addressModeU = ISampler::Description::AddressMode::Clamp;
        desc.addressModeV = ISampler::Description::AddressMode::Clamp;
        desc.addressModeW = ISampler::Description::AddressMode::Clamp;
        desc.lodBias = 0.f;
        desc.maximumAnisotropy = 0;
        //desc.comparisonKind = ComparisonKind::Always;
        desc.borderColor = ISampler::Description::BorderColor::TransparentBlack;
        desc.minimumLod = 0.f;
        desc.maximumLod = 0.f;

        samp = gd->GetResourceFactory().CreateSampler(desc);
    }

    // Create texture view
    {

        IResourceSet::Description rsDesc {};
        rsDesc.layout = pipelineLayout;
        rsDesc.boundResources = {
            BufferRange::MakeByteBuffer(mvpBuffer),
            gd->GetResourceFactory().CreateTextureView(fontTex),
            samp
        };

        fontRS = gd->GetResourceFactory().CreateResourceSet(rsDesc);
    }

    

    // Store our identifier
    io.Fonts->SetTexID((ImTextureID)fontRS.get());
}


void ImGuiAlloyBackend::_CreateDeviceObjects() {
    _CreateRenderPipeline();
    _CreateBuffers();
    _CreateFontsTexture();
}
