#pragma once



//3rd-party headers
#include <d3d12.h>
#include <D3D12MemAlloc.h>

//alloy public headers
#include "alloy/Context.hpp"

//standard library headers
#include <string>
#include <vector>
#include <memory>
//backend specific headers

//platform specific headers
#include <dxgi1_4.h> //Guaranteed by DX12

//Local headers

namespace alloy::dxc {

    class DXCDevice;
    class DXCContext;

    class DllLoader {
    protected:
        void* dllHandle;

    public:
        DllLoader(const char* dllName);
        virtual ~DllLoader();

        DllLoader(const DllLoader&) = delete;
        DllLoader& operator=(const DllLoader&) = delete;
        
        DllLoader(DllLoader&& other)
            : dllHandle(other.dllHandle)
        {
            other.dllHandle = nullptr;
        }
        
        DllLoader& operator=(DllLoader&& other) {
            if(dllHandle) {
                FreeLibrary((HMODULE)dllHandle);
            }

            dllHandle = other.dllHandle;
            other.dllHandle = nullptr;
        }


        void* GetFunctionEntry(const char* name);
    };

    
    class D3D12DllLoader : public DllLoader {
    public:
        PFN_D3D12_CREATE_DEVICE            pfnD3D12CreateDevice;
        PFN_D3D12_GET_DEBUG_INTERFACE      pfnD3D12GetDebugInterface;
        PFN_D3D12_SERIALIZE_ROOT_SIGNATURE pfnD3D12SerializeRootSignature;
        PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE pfnD3D12SerializeVersionedRootSignature;

        D3D12DllLoader();
    };

    class DxgiDllLoader : public DllLoader {
    public:
        using PFN_CreateDXGIFactory2 = HRESULT (WINAPI* )(UINT ,REFIID , _COM_Outptr_ void** );
        using PFN_DXGIGetDebugInterface1 = HRESULT (WINAPI *)(UINT ,REFIID, _COM_Outptr_ void **);

        PFN_CreateDXGIFactory2 pfnCreateDXGIFactory2;
        PFN_DXGIGetDebugInterface1 pfnDXGIGetDebugInterface1;

        DxgiDllLoader();
    };

    //Special variant of DLL loader. Due to no direct usage of those dlls
    // from D3D12Core.dll & d3d12SDKLayers.dll, we just open these without getting
    // any function entries. Normally the agility SDK co-distributed with 
    // the app is often newer that the OS's itself, we'll first try loading
    // it in <App exe binary path>/D3D12/****.dll, if that fails then from OS
    // default path
    class AgilitySDKLoader {
        void* d3d12CoreDllHandle,* d3d12SDKLayersDllHandle;
        uint32_t d3d12CoreVersion, d3d12SDKLayersVersion;
    public:
        AgilitySDKLoader(bool loadDebugLayer = false);

        ~AgilitySDKLoader();

        AgilitySDKLoader(const AgilitySDKLoader&) = delete;
        AgilitySDKLoader& operator=(const AgilitySDKLoader&) = delete;

        
        AgilitySDKLoader(AgilitySDKLoader&& other)
            : d3d12CoreDllHandle(other.d3d12CoreDllHandle)
            , d3d12SDKLayersDllHandle(other.d3d12SDKLayersDllHandle)
            , d3d12CoreVersion(other.d3d12CoreVersion)
            , d3d12SDKLayersVersion(other.d3d12SDKLayersVersion)
        {
            other.d3d12CoreDllHandle = nullptr;
            other.d3d12SDKLayersDllHandle = nullptr;
        }
        AgilitySDKLoader& operator=(AgilitySDKLoader&& other) {
            if(d3d12CoreDllHandle) {
                FreeLibrary((HMODULE)d3d12CoreDllHandle);
            }
            if(d3d12SDKLayersDllHandle) {
                FreeLibrary((HMODULE)d3d12SDKLayersDllHandle);
            }

            d3d12CoreDllHandle      = other.d3d12CoreDllHandle;
            d3d12SDKLayersDllHandle = other.d3d12SDKLayersDllHandle;
            d3d12CoreVersion        = other.d3d12CoreVersion;
            d3d12SDKLayersVersion   = other.d3d12SDKLayersVersion;

            other.d3d12CoreDllHandle = nullptr;
            other.d3d12SDKLayersDllHandle = nullptr;
        }

        uint32_t GetSDKVersion() const { return d3d12CoreVersion; }
        uint32_t GetDebugLayerVersion() const { return d3d12SDKLayersVersion; }
    };

      
    struct D3D12DevCaps{
        D3D_FEATURE_LEVEL feature_level;
        D3D_SHADER_MODEL shader_model;
        D3D_ROOT_SIGNATURE_VERSION root_sig_version;
        D3D12_FEATURE_DATA_ARCHITECTURE1 architecture;
        D3D12_FEATURE_DATA_D3D12_OPTIONS options;
        D3D12_FEATURE_DATA_D3D12_OPTIONS1 options1;
        D3D12_FEATURE_DATA_D3D12_OPTIONS2 options2;
        D3D12_FEATURE_DATA_D3D12_OPTIONS3 options3;
        D3D12_FEATURE_DATA_D3D12_OPTIONS4 options4;
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5;
        D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7;
        D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12;
        D3D12_FEATURE_DATA_D3D12_OPTIONS13 options13;
        D3D12_FEATURE_DATA_D3D12_OPTIONS14 options14;
        D3D12_FEATURE_DATA_D3D12_OPTIONS15 options15;
        D3D12_FEATURE_DATA_D3D12_OPTIONS16 options16;
        D3D12_FEATURE_DATA_D3D12_OPTIONS17 options17;
        D3D12_FEATURE_DATA_D3D12_OPTIONS19 options19;

        float timestamp_period;
        bool support_a4b4g4r4;
        uint32_t maxMSAASampleCount;

        void ReadFromDevice(ID3D12Device* pdev);

        bool SupportEnhancedBarrier() const { return options12.EnhancedBarriersSupported; }
        bool SupportMeshShader() const {return options7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;}
        bool SupportRayTracing() const {return options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED; }
        bool SupportReBAR() const { return options16.GPUUploadHeapSupported; }
        bool SupportUMA() const { return architecture.UMA || architecture.CacheCoherentUMA; }
    
        uint32_t ResourceBindingTier() const {
            switch(options.ResourceBindingTier) {
                case D3D12_RESOURCE_BINDING_TIER_3: return 3;
                case D3D12_RESOURCE_BINDING_TIER_2: return 2;
                case D3D12_RESOURCE_BINDING_TIER_1:
                default: return 1;
            }
        }
    };

    class DXCAdapter : public IPhysicalAdapter {
        IDXGIAdapter1* _adp;
        common::sp<DXCContext> _ctx;

        D3D12DevCaps _caps;

        void PopulateAdpInfo();

    public:
        DXCAdapter(IDXGIAdapter1* adp, common::sp<DXCContext>&& ctx);

        virtual ~DXCAdapter() override;

        DXCContext& GetContext() { return *_ctx; }
        const D3D12DevCaps& GetCaps() const { return _caps; }

        IDXGIAdapter1* GetHandle() const {return _adp;}
        virtual common::sp<IGraphicsDevice> RequestDevice(
            const IGraphicsDevice::Options& options) override;

    };

    class DXCContext : public IContext {
        AgilitySDKLoader _agilitySDK;

        D3D12DllLoader _d3d12Dll;
        DxgiDllLoader _dxgiDll;

        IDXGIFactory4* _factory;
    public:
        DXCContext(
            AgilitySDKLoader&& agilitySDK,
            D3D12DllLoader&& d3d12Dll,
            DxgiDllLoader&& dxgiDll,
            IDXGIFactory4* factory
        ) 
            : _agilitySDK(std::move(agilitySDK))
            , _d3d12Dll(std::move(d3d12Dll))
            , _dxgiDll(std::move(dxgiDll))
            , _factory(factory)
        { }
        virtual ~DXCContext() override;

        static common::sp<DXCContext> Make(const IContext::Options& opts);

        virtual common::sp<IGraphicsDevice> CreateDefaultDevice(const IGraphicsDevice::Options& options) override;
        virtual std::vector<common::sp<IPhysicalAdapter>> EnumerateAdapters() override;

        const D3D12DllLoader& GetD3D12Dll() const { return _d3d12Dll; }
        const DxgiDllLoader& GetDxgiDll() const { return _dxgiDll; }
    };
}
