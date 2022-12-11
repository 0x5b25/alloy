#include "App.hpp"

#include <string>
#include <iostream>
#include <cassert>
#include <chrono>

#include "./renderdoc_app.h"

#if defined(VLD_PLATFORM_WIN32)

#include <Windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#endif

RENDERDOC_API_1_1_2* rdoc_api = nullptr;
void EnableRenderDoc() {

    if (rdoc_api != nullptr){ return; }
    // At init, on windows
#if defined(VLD_PLATFORM_WIN32)
    if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api);
        assert(ret == 1);
    }
#elif defined(VLD_PLATFORM_LINUX)
    // At init, on linux/android.
    // For android replace librenderdoc.so with libVkLayer_GLES_RenderDoc.so
    if (void* mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api);
        assert(ret == 1);
    }
#elif defined(VLD_PLATFORM_ANDROID)
    // At init, on linux/android.
    // For android replace librenderdoc.so with libVkLayer_GLES_RenderDoc.so
    if (void* mod = dlopen("libVkLayer_GLES_RenderDoc.so", RTLD_NOW | RTLD_NOLOAD))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api);
        assert(ret == 1);
    }
#endif

    if (rdoc_api) {
        std::cout << "RenderDoc debug enabled.\n";
    }    

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



using _Clock = std::chrono::steady_clock;
using _TimePoint = _Clock::time_point;

AppBase::AppBase(unsigned width, unsigned height, const std::string& wndName){


    initialWidth = width;
    initialHeight = height;

    EnableRenderDoc();
    //startup
    if (!glfwInit())
        return;

    /* Create a windowed mode window and its OpenGL context */
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(initialWidth, initialHeight, wndName.c_str(), NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return;
    }
}

AppBase::~AppBase(){
    glfwTerminate();
}

void AppBase::GetFramebufferSize(int& width, int& height){
    glfwGetFramebufferSize(window, &width, &height);
}

template<typename T>
float _CastToSec(T&& dur) {

    auto deltaMs = std::chrono::duration_cast<
        std::chrono::milliseconds>(dur);

    auto deltaSec = deltaMs.count() / 1e3f;

    return deltaSec;
}

bool AppBase::Run(){
    

#if defined(VLD_PLATFORM_WIN32)
    auto hWnd = glfwGetWin32Window(window);
    auto hInst = GetModuleHandle(NULL);
    Veldrid::Win32SwapchainSource scSrc{ hWnd, nullptr };
#elif defined(VLD_PLATFORM_MACOS)

#endif
    OnAppStart(&scSrc, initialWidth, initialHeight);

    auto prevTime = _Clock::now();
    auto timeStart = prevTime;

    while (!glfwWindowShouldClose(window))
    {
        //Calculate deltatime
        auto currTime = _Clock::now();
        auto deltaSec = _CastToSec(currTime - prevTime);
        timeElapsedSec = _CastToSec(currTime - timeStart);

        prevTime = currTime;

        StartCapture();
        auto res = OnAppUpdate(deltaSec);
        StopCapture();

         /* Poll for and process events */
        glfwPollEvents();
        if(!res) break;
    }

    OnAppExit();


    return true;

}
