#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <veldrid/backend/Backends.hpp>
#include <veldrid/SwapChainSources.hpp>
#include <veldrid/GraphicsDevice.hpp>
#include <veldrid/Pipeline.hpp>
#include <veldrid/Shader.hpp>

#include <string>
#include <iostream>

#include "./renderdoc_app.h"
#include <Windows.h>

RENDERDOC_API_1_5_0* rdoc_api = NULL;
void EnableRenderDoc() {

    // At init, on windows
    if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_5_0, (void**)&rdoc_api);
        assert(ret == 1);
    }

    if (rdoc_api) {
        std::cout << "RenderDoc debug enabled.\n";
    }

    // At init, on linux/android.
    // For android replace librenderdoc.so with libVkLayer_GLES_RenderDoc.so
    //if (void* mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
    //{
    //    pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
    //    int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api);
    //    assert(ret == 1);
    //}

    // To start a frame capture, call StartFrameCapture.
    // You can specify NULL, NULL for the device to capture on if you have only one device and
    // either no windows at all or only one window, and it will capture from that device.
    // See the documentation below for a longer explanation
    //if (rdoc_api) rdoc_api->StartFrameCapture(NULL, NULL);

    // Your rendering should happen here

    // stop the capture
    //if (rdoc_api) rdoc_api->EndFrameCapture(NULL, NULL);
}

void StartCapture() {
    if (rdoc_api) rdoc_api->StartFrameCapture(NULL, NULL);
}

void StopCapture() {
    if (rdoc_api) rdoc_api->EndFrameCapture(NULL, NULL);
}

struct VertexData
{
    float position[2]; // This is the position, in normalized device coordinates.
    float color[4]; // This is the color of the vertex.
};

const std::string VertexCode = R"(
#version 450

layout(location = 0) in vec2 Position;
layout(location = 1) in vec4 Color;

layout(location = 0) out vec4 fsin_Color;

void main()
{
    gl_Position = vec4(Position, 0.5, 1);
    fsin_Color = Color;
}
//vec2 positions[3] = vec2[](
//    vec2(0.0, -0.5),
//    vec2(0.5, 0.5),
//    vec2(-0.5, 0.5)
//);
//
//void main() {
//    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
//}
)";

const std::string FragmentCode = R"(
#version 450

layout(location = 0) in vec4 fsin_Color;
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

int main(void){
    EnableRenderDoc();
    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    auto hWnd = glfwGetWin32Window(window);
    auto hInst = GetModuleHandle(NULL);
    {
        Veldrid::Win32SwapchainSource scSrc{ hWnd, nullptr };

        //return 0;
        Veldrid::GraphicsDevice::Options opt{};
        opt.preferStandardClipSpaceYDirection = true;
        auto dev = Veldrid::CreateVulkanGraphicsDevice(opt, &scSrc);
        Veldrid::SwapChain::Description swapChainDesc{};
        swapChainDesc.initialWidth = 640;
        swapChainDesc.initialHeight = 480;
        swapChainDesc.depthFormat = Veldrid::PixelFormat::D24_UNorm_S8_UInt;
        auto swapChain = dev->GetResourceFactory()->CreateSwapChain(swapChainDesc);

        auto spvCompiler = Veldrid::IGLSLCompiler::Get();
        std::string compileInfo;

        std::cout << "Compiling vertex shader...\n";
        std::vector<std::uint32_t> vertexSpv;
        spvCompiler->CompileToSPIRV(
            Veldrid::Shader::Description::Stages::Vertex,
            VertexCode,
            "main",
            {},
            vertexSpv,
            compileInfo
        );
        std::cout << compileInfo << "\n";

        std::cout << "Compiling fragment shader...\n";
        std::vector<std::uint32_t> fragSpv;
        spvCompiler->CompileToSPIRV(
            Veldrid::Shader::Description::Stages::Fragment,
            FragmentCode,
            "main",
            {},
            fragSpv,
            compileInfo
        );
        std::cout << compileInfo << "\n";


        //CreateResources

        auto* factory = dev->GetResourceFactory();

        VertexData quadVertices[] =
        {
            {{-0.75f, 0.75f},  {1.f, 0.f, 0.f, 1.f}},
            {{0.75f, 0.75f},   {0.f, 1.f, 0.f, 1.f}},
            {{-0.75f, -0.75f}, {0.f, 0.f, 1.f, 1.f}},
            {{0.75f, -0.75f},  {1.f, 1.f, 0.f, 1.f}}
        };

        std::uint32_t quadIndices[] = { 0, 1, 2, 3 };

        Veldrid::Buffer::Description _vbDesc{};
        _vbDesc.sizeInBytes = 4 * sizeof(VertexData);
        _vbDesc.usage.vertexBuffer = 1;
        _vbDesc.usage.staging = 1;
        auto _vertexBuffer = factory->CreateBuffer(_vbDesc);

        Veldrid::Buffer::Description _ibDesc{};
        _ibDesc.sizeInBytes = 4 * sizeof(std::uint32_t);
        _ibDesc.usage.indexBuffer = 1;
        _ibDesc.usage.staging = 1;
        auto _indexBuffer = factory->CreateBuffer(_ibDesc);

        {
            auto ptr = _vertexBuffer->MapToCPU();
            memcpy(ptr, quadVertices, sizeof(quadVertices));
            _vertexBuffer->UnMap();
        }

        {
            auto ptr = _indexBuffer->MapToCPU();
            memcpy(ptr, quadIndices, sizeof(quadIndices));
            _indexBuffer->UnMap();
        }

        Veldrid::Texture::Description rtDesc{};
        rtDesc.width = 640;
        rtDesc.height = 480;
        rtDesc.depth = 1;
        rtDesc.mipLevels = 1;
        rtDesc.arrayLayers = 1;
        rtDesc.type = Veldrid::Texture::Description::Type::Texture2D;
        rtDesc.usage.renderTarget = 1;
        rtDesc.format = Veldrid::PixelFormat::R8_G8_B8_A8_UNorm;
        auto renderTgt = factory->CreateTexture(rtDesc);

        Veldrid::Framebuffer::Description fbDesc{};
        fbDesc.colorTargets.push_back({});
        auto& fbCT = fbDesc.colorTargets.back();
        fbCT.mipLevel = 0;
        fbCT.arrayLayer = 0;
        fbCT.target = renderTgt;
        auto fb = factory->CreateFramebuffer(fbDesc);


        Veldrid::Shader::Description vertexShaderDesc{};
        vertexShaderDesc.stage = Veldrid::Shader::Description::Stages::Vertex;
        vertexShaderDesc.entryPoint = "main";
        Veldrid::Shader::Description fragmentShaderDesc{};
        fragmentShaderDesc.stage = Veldrid::Shader::Description::Stages::Fragment;
        fragmentShaderDesc.entryPoint = "main";

        auto fragmentShader = factory->CreateShader(fragmentShaderDesc, fragSpv);
        auto vertexShader = factory->CreateShader(vertexShaderDesc, vertexSpv);

        Veldrid::GraphicsPipelineDescription pipelineDescription{};
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
        pipelineDescription.resourceLayouts = {};

        using VL = Veldrid::GraphicsPipelineDescription::ShaderSet::VertexLayout;
        pipelineDescription.shaderSet.vertexLayouts = { {} };
        pipelineDescription.shaderSet.vertexLayouts[0].SetElements({
            {"Position", VL::Element::Semantic::TextureCoordinate, Veldrid::ShaderDataType::Float2},
            {"Color", VL::Element::Semantic::TextureCoordinate, Veldrid::ShaderDataType::Float4}
            });
        pipelineDescription.shaderSet.shaders = { vertexShader, fragmentShader };

        pipelineDescription.outputs = swapChain->GetFramebuffer()->GetOutputDescription();
        //pipelineDescription.outputs = fb->GetOutputDescription();
        auto _pipeline = factory->CreateGraphicsPipeline(pipelineDescription);
        auto _fence = factory->CreateFence(false);

        /* Loop until the user closes the window */
        while (!glfwWindowShouldClose(window))
        {
            //StartCapture();
            /* Render here */

            auto _commandList = factory->CreateCommandList();
            //Record command buffer
            _commandList->Begin();

            _commandList->BeginRenderPass(swapChain->GetFramebuffer());
            //_commandList->BeginRenderPass(fb);
            //_commandList->ClearDepthStencil(0, 0);
            _commandList->ClearColorTarget(0, 0.1, 0.1, 0.3, 1);
            _commandList->SetVertexBuffer(0, _vertexBuffer);
            _commandList->SetIndexBuffer(_indexBuffer, Veldrid::IndexFormat::UInt32);
            _commandList->SetPipeline(_pipeline);
            _commandList->SetFullViewports();
            _commandList->SetFullScissorRects();
            //_commandList->Draw(3);
            _commandList->DrawIndexed(
                /*indexCount:    */4,
                /*instanceCount: */1,
                /*indexStart:    */0,
                /*vertexOffset:  */0,
                /*instanceStart: */0);
            _commandList->EndRenderPass();
            _commandList->End();

            dev->SubmitCommand(_commandList.get(), _fence.get());

            _fence->WaitForSignal();
            _fence->Reset();
            //StopCapture();

            /* Swap front and back buffers */
            //glfwSwapBuffers(window);
            auto res = dev->PresentToSwapChain(swapChain.get());
            switch (res)
            {
                case Veldrid::SwapChain::State::Optimal: {
                    swapChain->SwapBuffers();
                } break;
                case Veldrid::SwapChain::State::Suboptimal:
                case Veldrid::SwapChain::State::OutOfDate: {
                    int width = 0, height = 0;
                    glfwGetFramebufferSize(window, &width, &height);
                    while (width == 0 || height == 0) {
                        glfwGetFramebufferSize(window, &width, &height);
                        glfwWaitEvents();
                    }
                    swapChain->Resize(width, height);
                } break;
                case Veldrid::SwapChain::State::Error: {
                    return -1;
                    //assert(false);
                }
            }
            
            //dev->WaitForIdle();
            
            /* Poll for and process events */
            glfwPollEvents();
        }
    }

    glfwTerminate();
    return 0;

}
