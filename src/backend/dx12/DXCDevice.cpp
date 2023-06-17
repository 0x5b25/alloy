#include "DXCDevice.hpp"

//3rd-party headers

//veldrid public headers
#include "veldrid/common/Common.hpp"
#include "veldrid/backend/Backends.hpp"

//standard library headers

//backend specific headers

//platform specific headers
//#include <dxgi1_6.h>
#include <dxgi.h>
#include <wrl.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h> // For HRESULT

// From DXSampleHelper.h 
// Source: https://github.com/Microsoft/DirectX-Graphics-Samples
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}


IDXGIAdapter4* GetAdapter(bool useWarp) {
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
 
    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));
    IDXGIAdapter1* dxgiAdapter1;
    IDXGIAdapter4* dxgiAdapter4;
 
    if (useWarp) {
        ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
        ThrowIfFailed(dxgiAdapter1->QueryInterface(IID_PPV_ARGS(&dxgiAdapter4)));
    } else
    {
        SIZE_T maxDedicatedVideoMemory = 0;
        for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
            dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);
 
            // Check to see if the adapter can create a D3D12 device without actually 
            // creating it. The adapter with the largest dedicated video memory
            // is favored.
            if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), 
                    D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) && 
                dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory )
            {
                maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                ThrowIfFailed(dxgiAdapter1->QueryInterface(IID_PPV_ARGS(&dxgiAdapter4)));
            }
        }
    }
 
    return dxgiAdapter4;
}


/*
typedef 
enum D3D_FEATURE_LEVEL
    {
        D3D_FEATURE_LEVEL_1_0_CORE	= 0x1000,
        D3D_FEATURE_LEVEL_9_1	= 0x9100,
        D3D_FEATURE_LEVEL_9_2	= 0x9200,
        D3D_FEATURE_LEVEL_9_3	= 0x9300,
        D3D_FEATURE_LEVEL_10_0	= 0xa000,
        D3D_FEATURE_LEVEL_10_1	= 0xa100,
        D3D_FEATURE_LEVEL_11_0	= 0xb000,
        D3D_FEATURE_LEVEL_11_1	= 0xb100,
        D3D_FEATURE_LEVEL_12_0	= 0xc000,
        D3D_FEATURE_LEVEL_12_1	= 0xc100,
        D3D_FEATURE_LEVEL_12_2	= 0xc200
    } 	D3D_FEATURE_LEVEL;
*/
bool QueryLatestDXCFeatLvl(IDXGIAdapter* adp, D3D_FEATURE_LEVEL* outLvl){
    auto _QueryFeatLvl = [&](D3D_FEATURE_LEVEL lvl)->bool{
        auto res = D3D12CreateDevice(adp, lvl, __uuidof(ID3D12Device), nullptr);
        return SUCCEEDED(res);
    };

    static const D3D_FEATURE_LEVEL LVL_LIST [] {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };

    bool supportDX12 = false;
    for(auto& lvl : LVL_LIST){
        if(_QueryFeatLvl(lvl)){
            supportDX12 = true;
            *outLvl = lvl;
            break;
        }
    }

    return supportDX12;
}


HRESULT GetAdapter(bool useWarp, std::vector<IDXGIAdapter*>& outAdps) {

/*
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
  */

    IDXGIFactory* dxgiFactory;
    auto status = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
    if(FAILED(status)) {
        return status;
    } else {

        IDXGIAdapter* dxgiAdapter;
        for (UINT i = 0; dxgiFactory->EnumAdapters(i, &dxgiAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC dxgiAdapterDesc;
            dxgiAdapter->GetDesc(&dxgiAdapterDesc);
 
            // Check to see if the adapter can create a D3D12 device without actually 
            // creating it. The adapter with the largest dedicated video memory
            // is favored.
            if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), 
                    D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) && 
                dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory )
            {
                maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                ThrowIfFailed(dxgiAdapter1->QueryInterface(IID_PPV_ARGS(&dxgiAdapter4)));
            }
        }



        dxgiFactory->Release();
    }
 
   
 
    if (useWarp) {
        IDXGIAdapter1* dxgiAdapter1;

        status = dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1));
        if(FAILED(status)) {return status;}

        IDXGIAdapter4* dxgiAdapter4;
        status = dxgiAdapter1->QueryInterface(IID_PPV_ARGS(&dxgiAdapter4));
        if(FAILED(status)) {return status;}

        outAdps.push_back(dxgiAdapter4);
    } else
    {
        SIZE_T maxDedicatedVideoMemory = 0;

        
    }
 
    return dxgiAdapter4;
}


namespace Veldrid {


    sp<GraphicsDevice> CreateDX12GraphicsDevice(
        const GraphicsDevice::Options& options,
        SwapChainSource* swapChainSource
    ) {
        return DXCDevice::Make(options, swapChainSource);
    }




    sp<GraphicsDevice> DXCDevice::Make(const GraphicsDevice::Options &options, SwapChainSource *swapChainSource)
    {
        return sp<GraphicsDevice>();
    }

}
