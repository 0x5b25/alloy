#include "GPUCapture.hpp"

#include <iostream>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef USE_PIX
#include <WinPixEventRuntime/pix3.h>
#else
#include "./renderdoc_app.h"
#endif

using namespace alloy;


#ifdef USE_PIX
using PFN_PIXBeginCapture2
    = HRESULT (WINAPI *) (DWORD , _In_opt_ const PPIXCaptureParameters );
using PFN_PIXEndCapture
    = HRESULT (WINAPI *) (BOOL);

static PFN_PIXBeginCapture2 pfnPIXBeginCapture2 = nullptr;
static PFN_PIXEndCapture pfnPIXEndCapture = nullptr;

void SetupCaptureEvent(common::sp<IGraphicsDevice> gd) {
    auto hPixRuntimeDll = LoadLibraryA("WinPixEventRuntime.dll");
    if(hPixRuntimeDll) {
        pfnPIXBeginCapture2 
            = (PFN_PIXBeginCapture2)GetProcAddress(hPixRuntimeDll, "PIXBeginCapture2");
        pfnPIXEndCapture 
            = (PFN_PIXEndCapture)GetProcAddress(hPixRuntimeDll, "PIXEndCapture");

        if (pfnPIXBeginCapture2 && pfnPIXEndCapture) {
            std::cout << "PIX debug enabled.\n";
        }  
    }

}

void StartCapture() {
    if(pfnPIXBeginCapture2) {
        PIXCaptureParameters params {};
        params.GpuCaptureParameters.FileName = L"Capture";
        pfnPIXBeginCapture2(PIX_CAPTURE_GPU, &params);
    }
}

void StopCapture() {
    if(pfnPIXEndCapture)
        pfnPIXEndCapture(false);
}
#elif defined(USE_RENDERDOC)

static RENDERDOC_API_1_1_2* rdoc_api = nullptr;
void SetupCaptureEvent(common::sp<IGraphicsDevice> gd) {

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

#else

void SetupCaptureEvent(alloy::common::sp<alloy::IGraphicsDevice> gd) { }
void StartCapture() { }
void StopCapture() { }

#endif

