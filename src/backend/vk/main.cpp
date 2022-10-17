
#include <cassert>
#include <iostream>
#include <vector>

#include <volk.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "Device.hpp"
#include "Shader.hpp"
#include "Renderer.hpp"

VkInstance CreateInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    createInfo.enabledLayerCount = 0;

    VkInstance instance;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    

    return instance;
}


VkShaderModule CreateShaderModule(VkDevice device, const vkb::ShaderModule& mod)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = mod.get_binary().size() * sizeof(uint32_t);
    createInfo.pCode = reinterpret_cast<const uint32_t*>(mod.get_binary().data());

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));
    return shaderModule;
}


int main(int, char**) {
    std::cout << "Hello, world!\n";
    //Init vulkan loader
    auto res = volkInitialize();

    if(res == VK_SUCCESS) std::cout << "Volk init success.\n";
    else std::cout << "Volk init failed.\n";

    

    const std::vector<Vertex> vertices = {
        {{0.0f, -0.5f}, {.0f,.0f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f}, {.0f,1.0f},{0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {1.0f,1.0f},{0.0f, 0.0f, 1.0f}}
    };


    //Init glfw
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    auto window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    //Instance creation

    auto inst = CreateInstance();
    volkLoadInstance(inst);

    VkSurfaceKHR surface;
    VK_CHECK(glfwCreateWindowSurface(inst, window, NULL, &surface));

    {
        auto dev = Device::Make(inst, surface,
            {"VK_KHR_swapchain"}
        );
        auto swapChain = SwapChain::Make(dev);
        auto imageCount = swapChain->ImageCount();

        //Asset prep
        auto vertShader = vkb::ShaderModule::Make(
            dev, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT,
            R"V(#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}
    	)V"
        );

        auto fragShader = vkb::ShaderModule::Make(
            dev,
           VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT,
           R"V(#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
    	)V"
        );

        //Upload vertex data
        auto vertexBuffer = dev->AllocateBuffer(
            sizeof(Vertex) * vertices.size(), 
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU);

        auto ptr = vertexBuffer->MapToCPU();
        memcpy(ptr, vertices.data(), vertexBuffer->Size());
        vertexBuffer->UnMap();

        //Lets get shaders going
        //auto vertShaderModule = CreateShaderModule(dev->Handle(), vertShader);
        //auto fragShaderModule = CreateShaderModule(dev->Handle(), fragShader);
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShader->Handle();
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShader->Handle();
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        //Pipeline per vertex data layout
        auto bindingDescription = Vertex::BindingDescription();
        auto attributeDescriptions = Vertex::AttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        //Input render element(triangles) layout
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        //And viewport
        //VkViewport viewport{};
        //viewport.x = 0.0f;
        //viewport.y = 0.0f;
        //viewport.width = (float)swapChain->Extent().width;
        //viewport.height = (float)swapChain->Extent().height;
        //viewport.minDepth = 0.0f;
        //viewport.maxDepth = 1.0f;
        //VkRect2D scissor{};
        //scissor.offset = { 0, 0 };
        //scissor.extent = swapChain->Extent();
        //VkPipelineViewportStateCreateInfo viewportState{};
        //viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        //viewportState.viewportCount = 1;
        //viewportState.pViewports = &viewport;
        //viewportState.scissorCount = 1;
        //viewportState.pScissors = &scissor;

        //Rasterizer settings
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        //Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        //Color blending
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional
        
        //Render pass
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChain->Format().format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPass renderPass;
        //Pipeline layouts
        VkPipelineLayout pipelineLayout;
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // Optional
        pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

        VK_CHECK(vkCreatePipelineLayout(dev->Handle(), &pipelineLayoutInfo, nullptr, &pipelineLayout));

        //Render passes
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        VK_CHECK(vkCreateRenderPass(dev->Handle(), &renderPassInfo, nullptr, &renderPass));

        //Dynamic states
        //Dynamic states
        VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            //VK_DYNAMIC_STATE_LINE_WIDTH
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;

        //Graphics pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = nullptr;//&viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = nullptr; // Optional
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional
        VkPipeline graphicsPipeline;
        VK_CHECK(vkCreateGraphicsPipelines(dev->Handle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline));

        //Framebuffers
        //Create framebuffers
        std::vector<VkFramebuffer> frameBuffers(imageCount);
        for (size_t i = 0; i < imageCount; i++) {
            VkImageView attachments[] = {
                 swapChain->ImageViews()[i]
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChain->Extent().width;
            framebufferInfo.height = swapChain->Extent().height;
            framebufferInfo.layers = 1;

            VK_CHECK(vkCreateFramebuffer(dev->Handle(), &framebufferInfo, nullptr, &frameBuffers[i]));
        }

        //Setup command buffers
        VkCommandBufferAllocateInfo cmd = {};
        cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd.pNext = nullptr;
        cmd.commandPool = dev->CmdPool();
        cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd.commandBufferCount = 1;

        VkCommandBuffer cmdBuf;
        VK_CHECK(vkAllocateCommandBuffers(dev->Handle(), &cmd, &cmdBuf));

        auto semaphore_imgReady = Semaphore::Make(dev);
        auto semaphore_renderDone = Semaphore::Make(dev);
        auto fence = Fence::Make(dev, true);
        
        //Main loop
    	while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            //Acquire image
            uint32_t frameID;
            vkAcquireNextImageKHR(
                dev->Handle(), 
                swapChain->Handle(), 
                UINT64_MAX, semaphore_imgReady->Handle(),
                VK_NULL_HANDLE, 
                &frameID);

            //Record render commands
            {
                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = 0; // Optional
                beginInfo.pInheritanceInfo = nullptr; // Optional

                VK_CHECK(vkBeginCommandBuffer(cmdBuf, &beginInfo));

                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = (float)swapChain->Extent().width;
                viewport.height = (float)swapChain->Extent().height;
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

                VkRect2D scissor{};
                scissor.offset = { 0, 0 };
                scissor.extent = swapChain->Extent();
                vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = renderPass;
                renderPassInfo.framebuffer = frameBuffers[frameID];
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = swapChain->Extent();

                VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
                renderPassInfo.clearValueCount = 1;
                renderPassInfo.pClearValues = &clearColor;
                vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

                vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
                VkBuffer vertexBuffers[] = { vertexBuffer->Handle() };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(cmdBuf, 0, 1, vertexBuffers, offsets);
                vkCmdDraw(cmdBuf, static_cast<uint32_t>(vertices.size()), 1, 0, 0);

                vkCmdEndRenderPass(cmdBuf);

                VK_CHECK(vkEndCommandBuffer(cmdBuf));
            }
            {
                //Wait for previous work finishes
                VK_CHECK(vkWaitForFences(dev->Handle(), 1, &fence->Handle(), VK_TRUE, UINT64_MAX));
                VK_CHECK(vkResetFences(dev->Handle(), 1, &fence->Handle()));

                //Submit command buffer
                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

                VkSemaphore waitSemaphores[] = { semaphore_imgReady->Handle() }; //Wait for image available
                VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
                submitInfo.waitSemaphoreCount = 1;
                submitInfo.pWaitSemaphores = waitSemaphores;
                submitInfo.pWaitDstStageMask = waitStages;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &cmdBuf;
                submitInfo.signalSemaphoreCount = 1;
                submitInfo.pSignalSemaphores = &semaphore_renderDone->Handle();

                vkQueueSubmit(dev->GraphicsQueue(), 1, &submitInfo, fence->Handle());
            }
            {
                //Present
                VkPresentInfoKHR presentInfo{};
                presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

                presentInfo.waitSemaphoreCount = 1;
                presentInfo.pWaitSemaphores = &semaphore_renderDone->Handle();
                VkSwapchainKHR swapChains[] = { swapChain->Handle() };
                presentInfo.swapchainCount = 1;
                presentInfo.pSwapchains = swapChains;
                presentInfo.pImageIndices = &frameID;
                presentInfo.pResults = nullptr; // Optional
                VK_CHECK(vkQueuePresentKHR(dev->GraphicsQueue(), &presentInfo));
            }

        }

        vkDeviceWaitIdle(dev->Handle());

		//Cleanup
        vkDestroyPipeline(dev->Handle(), graphicsPipeline, nullptr);

        //vkDestroyShaderModule(dev->Handle(), fragShaderModule, nullptr);
        //vkDestroyShaderModule(dev->Handle(), vertShaderModule, nullptr);

        vkDestroyPipelineLayout(dev->Handle(), pipelineLayout, nullptr);
        vkDestroyRenderPass(dev->Handle(), renderPass, nullptr);

        for (auto& framebuffer : frameBuffers) {
            vkDestroyFramebuffer(dev->Handle(), framebuffer, nullptr);
        }
    }
    vkDestroySurfaceKHR(inst, surface, nullptr);
    vkDestroyInstance(inst, nullptr);

    glfwDestroyWindow(window);

    glfwTerminate();


}
