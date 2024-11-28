#include <veldrid/backend/Backends.hpp>
#include <veldrid/SwapChainSources.hpp>
#include <veldrid/GraphicsDevice.hpp>
#include <veldrid/Pipeline.hpp>
#include <veldrid/Shader.hpp>

#include <string>
#include <iostream>

#include "app/App.hpp"

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

class DemoApp : public AppBase{

    Veldrid::sp<Veldrid::GraphicsDevice> dev;
    Veldrid::sp<Veldrid::SwapChain> swapChain;

    Veldrid::sp<Veldrid::Shader> fragmentShader, vertexShader;
    Veldrid::sp<Veldrid::Buffer> vertexBuffer, indexBuffer;

    Veldrid::sp<Veldrid::Pipeline> pipeline;

    Veldrid::sp<Veldrid::Semaphore> renderFinishSemaphore;
    Veldrid::sp<Veldrid::Fence> renderFinishFence;

    Veldrid::sp<Veldrid::CommandList> cmd;

    void CreateSwapChain(unsigned surfaceWidth, unsigned surfaceHeight){
        
        Veldrid::SwapChain::Description swapChainDesc{};
        swapChainDesc.initialWidth = surfaceWidth;
        swapChainDesc.initialHeight = surfaceHeight;
        swapChainDesc.depthFormat = Veldrid::PixelFormat::D24_UNorm_S8_UInt;
        swapChain = dev->GetResourceFactory()->CreateSwapChain(swapChainDesc);
    }

    void CreateShaders(){
        auto spvCompiler = Veldrid::IGLSLCompiler::Get();
        std::string compileInfo;

        std::cout << "Compiling vertex shader...\n";
        std::vector<std::uint32_t> vertexSpv;

        Veldrid::Shader::Stage stage{};
        stage.vertex = 1;
        if(!spvCompiler->CompileToSPIRV(
            stage,
            VertexCode,
            "main",
            {},
            vertexSpv,
            compileInfo
        )){
            std::cout << compileInfo << "\n";
        }

        std::cout << "Compiling fragment shader...\n";
        std::vector<std::uint32_t> fragSpv;

        stage = {};
        stage.fragment = 1;
        if(!spvCompiler->CompileToSPIRV(
            stage,
            FragmentCode,
            "main",
            {},
            fragSpv,
            compileInfo
        )){
            std::cout << compileInfo << "\n";
        }

        auto factory = dev->GetResourceFactory();
        Veldrid::Shader::Description vertexShaderDesc{};
        vertexShaderDesc.stage.vertex = 1;
        vertexShaderDesc.entryPoint = "main";
        Veldrid::Shader::Description fragmentShaderDesc{};
        fragmentShaderDesc.stage.fragment = 1;
        fragmentShaderDesc.entryPoint = "main";

        fragmentShader = factory->CreateShader(fragmentShaderDesc, fragSpv);
        vertexShader = factory->CreateShader(vertexShaderDesc, vertexSpv);

    }

    void CreateBuffers(){
        auto factory = dev->GetResourceFactory();

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
        vertexBuffer = factory->CreateBuffer(_vbDesc);

        Veldrid::Buffer::Description _ibDesc{};
        _ibDesc.sizeInBytes = 4 * sizeof(std::uint32_t);
        _ibDesc.usage.indexBuffer = 1;
        _ibDesc.usage.staging = 1;
        indexBuffer = factory->CreateBuffer(_ibDesc);

        {
            auto ptr = vertexBuffer->MapToCPU();
            memcpy(ptr, quadVertices, sizeof(quadVertices));
            vertexBuffer->UnMap();
        }

        {
            auto ptr = indexBuffer->MapToCPU();
            memcpy(ptr, quadIndices, sizeof(quadIndices));
            indexBuffer->UnMap();
        }
    }

    void CreatePipeline(){

        auto factory = dev->GetResourceFactory();

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
        pipeline = factory->CreateGraphicsPipeline(pipelineDescription);
    }

    void CreateSyncObjects(){
        auto factory = dev->GetResourceFactory();
        renderFinishFence = factory->CreateFence(true);
        renderFinishSemaphore = factory->CreateDeviceSemaphore();
    }

	void OnAppStart(
        Veldrid::SwapChainSource* swapChainSrc,
		unsigned surfaceWidth,
		unsigned surfaceHeight
    ) override{
        Veldrid::GraphicsDevice::Options opt{};
        opt.preferStandardClipSpaceYDirection = true;
        dev = Veldrid::CreateDefaultGraphicsDevice(opt, swapChainSrc);

        CreateSwapChain(surfaceWidth, surfaceHeight);
        CreateShaders();
        CreateBuffers();
        CreatePipeline();
        CreateSyncObjects();
        //CreateResources        
    }

	virtual void OnAppExit() override{
        dev->WaitForIdle();
    }

	virtual bool OnAppUpdate(float) override{
        /* Render here */
        auto factory = dev->GetResourceFactory();
        //Get one drawable
        auto res = swapChain->SwitchToNextFrameBuffer();
        switch (res)
        {
        case Veldrid::SwapChain::State::Optimal: {
            
        } break;
        case Veldrid::SwapChain::State::Suboptimal:
        case Veldrid::SwapChain::State::OutOfDate: {
            int width = 0, height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            while (width == 0 || height == 0) {
                glfwGetFramebufferSize(window, &width, &height);
                glfwWaitEvents();
            }
            dev->WaitForIdle();
            swapChain->Resize(width, height);
            swapChain->SwitchToNextFrameBuffer();
        } break;
        case Veldrid::SwapChain::State::Error: {
            return false;
            //assert(false);
        }
        }


        auto _commandList = factory->CreateCommandList();
        //Record command buffer
        _commandList->Begin();

        _commandList->BeginRenderPass(swapChain->GetFramebuffer());
        //_commandList->BeginRenderPass(fb);
        _commandList->ClearDepthStencil(0, 0);
        _commandList->ClearColorTarget(0, 0.1, 0.1, 0.3, 1);
        _commandList->SetVertexBuffer(0, vertexBuffer);
        _commandList->SetIndexBuffer(indexBuffer, Veldrid::IndexFormat::UInt32);
        _commandList->SetPipeline(pipeline);
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

        //Wait for previous render to complete
        renderFinishFence->WaitForSignal();
        renderFinishFence->Reset();
        dev->SubmitCommand(
            {
                {
                    { _commandList.get() },
                    {},
                    { renderFinishSemaphore.get()},
                }
            },
            renderFinishFence.get());
        cmd = _commandList;
        
        //StopCapture();

        /* Swap front and back buffers */
        //glfwSwapBuffers(window);
        res = dev->PresentToSwapChain(
            { renderFinishSemaphore.get() },
            swapChain.get());
        
        return true;
    }

public:
    DemoApp() : AppBase(800, 600, "DemoApp") {}

};

int main(void){        
    DemoApp app;
    app.Run();
}
