#include "DXCContext.hpp"

#include <dxgidebug.h>
#include "D3DCommon.hpp"
#include "DXCDevice.hpp"

#include <iostream>

namespace alloy {
    
    common::sp<IContext> CreateDX12Context() {
        return alloy::dxc::DXCContext::Make();
    }
}

namespace alloy::dxc {

    void PrintDxErrorMsg() {
        IDXGIDebug1* dxgiDebug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        {
            IDXGIInfoQueue* dxgiInfoQueue;
            ThrowIfFailed(dxgiDebug->QueryInterface(IID_PPV_ARGS(&dxgiInfoQueue)));
        
            for (UINT64 index = 0; index < dxgiInfoQueue->GetNumStoredMessages(DXGI_DEBUG_ALL); index++)
            {
                SIZE_T length = 0;
                dxgiInfoQueue->GetMessage(DXGI_DEBUG_ALL, index, nullptr, &length);
                auto msg = (DXGI_INFO_QUEUE_MESSAGE*)malloc(sizeof(length));
                dxgiInfoQueue->GetMessage(DXGI_DEBUG_ALL, index, msg, &length);

                std::cout << "Description: " << msg->pDescription << std::endl;
                
                free(msg);
            }
            dxgiInfoQueue->Release();
            dxgiDebug->Release();
        }
    }

    common::sp<IGraphicsDevice> DXCAdapter::RequestDevice(
        const IGraphicsDevice::Options& options
    ) {
        return DXCDevice::Make(common::ref_sp(this), options);
    }

    DXCAdapter::DXCAdapter(IDXGIAdapter1* adp, common::sp<DXCContext>&& ctx)
        : _adp(adp)
        , _ctx(std::move(ctx))
    {
        PopulateAdpInfo();
    }

    DXCAdapter::~DXCAdapter() {
        _adp->Release();
    }

    
    void DXCAdapter::PopulateAdpInfo() {
        
        ID3D12Device* dev;

        ThrowIfFailed(D3D12CreateDevice(_adp, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)));

        D3D12DevCaps caps {};
        caps.ReadFromDevice(dev);

        dev->Release();

        //Fill driver and api info
        {//Get device ID & driver version
            DXGI_ADAPTER_DESC1 desc{};
            if(SUCCEEDED(_adp->GetDesc1(&desc))){
                info.vendorID = desc.VendorId;
                info.deviceID = desc.DeviceId;
            }
        
            LARGE_INTEGER drvVer{};
            _adp->CheckInterfaceSupport(__uuidof(IDXGIDevice), &drvVer);
            info.driverVersion = drvVer.QuadPart;
        }

        {//get device name
            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(_adp->GetDesc1(&desc))){
                do{
                    auto& devNameStr = info.deviceName;
                    auto required_size = WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, NULL, 0, NULL, NULL);
                    devNameStr.resize(required_size);
                    if (required_size == 0) break;
        
                    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, devNameStr.data(), required_size, NULL, NULL);
                }while(0);
        
        
                //std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
                //const std::wstring wdesc{desc.Description};
                //dev->_devName = utf8_conv.to_bytes(wdesc);
            }            
        }

        {//Get api version
            
            auto& apiVer = info.apiVersion;
            apiVer = {Backend::DX12};
            switch (caps.feature_level)
            {
            case D3D_FEATURE_LEVEL_12_2:apiVer.major = 12; apiVer.minor = 2; break;
            case D3D_FEATURE_LEVEL_12_1:apiVer.major = 12; apiVer.minor = 1; break;
            case D3D_FEATURE_LEVEL_12_0:apiVer.major = 12; apiVer.minor = 0;break;
            case D3D_FEATURE_LEVEL_11_1:apiVer.major = 11; apiVer.minor = 1;break;
            case D3D_FEATURE_LEVEL_11_0:
            default: apiVer.major = 11; apiVer.minor = 0;
                break;
            }

            //dev->_commonFeat.commandListDebugMarkers = dev->_dxcFeat.feature_level >= D3D_FEATURE_LEVEL_11_1;
            //dev->_commonFeat.bufferRangeBinding = dev->_dxcFeat.feature_level >= D3D_FEATURE_LEVEL_11_1;
            
        }

        {//Populate vram info
        
            DXGI_ADAPTER_DESC1 desc{};
            if(SUCCEEDED(_adp->GetDesc1(&desc))){
                auto& localSeg = info.memSegments.emplace_back();
                localSeg.sizeInBytes = desc.DedicatedVideoMemory;
                if(caps.SupportReBAR() || caps.SupportUMA()) {
                    localSeg.flags.isCPUVisible = 1;
                }

                auto& sharedSeg = info.memSegments.emplace_back();
                sharedSeg.sizeInBytes = desc.SharedSystemMemory;
                sharedSeg.flags.isInSysMem = 1;
                sharedSeg.flags.isCPUVisible = 1;
            }
        }

        info.capabilities.isUMA = caps.SupportUMA();
        info.capabilities.supportMeshShader = caps.SupportMeshShader();

        uint32_t full_heap_count = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1;
        uint32_t uav_count = 8;
        switch(caps.ResourceBindingTier()){
            case 1 : {
                uav_count = caps.feature_level == D3D_FEATURE_LEVEL_11_0 ? 8 : 64;
                full_heap_count = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1;
            } break;
            case 2 : {
                full_heap_count = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2;
                uav_count = 64;
            } break;
            case 3 : {
                // Theoretically vram limited, but in practice 2^20 is the limit
                auto tier3_practical_descriptor_limit = 1 << 20;
                full_heap_count = uav_count = tier3_practical_descriptor_limit;
            } break;
        };

        info.limits.maxImageDimension1D = D3D12_REQ_TEXTURE1D_U_DIMENSION;
        info.limits.maxImageDimension2D = std::min(D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION,
            D3D12_REQ_TEXTURECUBE_DIMENSION);
        info.limits.maxImageDimension3D = D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
        info.limits.maxImageArrayLayers = D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
        //max_bind_groups: crate::MAX_BIND_GROUPS as u32,
        //max_bindings_per_bind_group: 65535,
        // dynamic offsets take a root constant, so we expose the minimum here
        //max_dynamic_uniform_buffers_per_pipeline_layout: base
        //    .max_dynamic_uniform_buffers_per_pipeline_layout,
        //max_dynamic_storage_buffers_per_pipeline_layout: base
        //    .max_dynamic_storage_buffers_per_pipeline_layout,
        info.limits.maxPerStageDescriptorSampledImages = caps.ResourceBindingTier() > 1 ? 
            full_heap_count : 128;
        info.limits.maxPerStageDescriptorSamplers = caps.ResourceBindingTier() > 1 ? 
            D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE : 16;
        // these both account towards `uav_count`, but we can't express the limit as as sum
        // of the two, so we divide it by 4 to account for the worst case scenario
        // (2 shader stages, with both using 16 storage textures and 16 storage buffers)
        info.limits.maxPerStageDescriptorStorageBuffers = uav_count / 4;
        info.limits.maxPerStageDescriptorStorageImages = uav_count / 4;
        info.limits.maxPerStageDescriptorUniformBuffers = full_heap_count;
        info.limits.maxPerStageResources = D3D12_COMMONSHADER_INPUT_RESOURCE_REGISTER_COUNT;
        info.limits.maxUniformBufferRange = D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
        info.limits.maxStorageBufferRange = 1 << 31;
        info.limits.maxVertexInputBindings = D3D12_VS_INPUT_REGISTER_COUNT;
        info.limits.maxVertexInputAttributes = D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
        info.limits.maxVertexInputBindingStride = D3D12_SO_BUFFER_MAX_STRIDE_IN_BYTES,
        //min_subgroup_size: 4, // Not using `features1.WaveLaneCountMin` as it is unreliable
        //max_subgroup_size: 128,
        // The push constants are part of the root signature which
        // has a limit of 64 DWORDS (256 bytes), but other resources
        // also share the root signature:
        //
        // - push constants consume a `DWORD` for each `4 bytes` of data
        // - If a bind group has buffers it will consume a `DWORD`
        //   for the descriptor table
        // - If a bind group has samplers it will consume a `DWORD`
        //   for the descriptor table
        // - Each dynamic uniform buffer will consume `2 DWORDs` for the
        //   root descriptor
        // - Each dynamic storage buffer will consume `1 DWORD` for a
        //   root constant representing the dynamic offset
        // - The special constants buffer count as constants
        //
        // Since we can't know beforehand all root signatures that
        // will be created, the max size to be used for push
        // constants needs to be set to a reasonable number instead.
        //
        // Source: https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signature-limits#memory-limits-and-costs
        info.limits.maxPushConstantsSize = 128;
        info.limits.minMemoryMapAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        info.limits.minUniformBufferOffsetAlignment 
            = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        info.limits.minStorageBufferOffsetAlignment = 4;
        info.limits.maxVertexOutputComponents = D3D12_VS_OUTPUT_REGISTER_COUNT;
        info.limits.maxFragmentInputComponents = D3D12_PS_INPUT_REGISTER_COUNT;
        info.limits.maxFragmentOutputAttachments = D3D12_PS_OUTPUT_REGISTER_COUNT;
        //max_inter_stage_shader_components: base.max_inter_stage_shader_components,
        //max_color_attachments,
        //max_color_attachment_bytes_per_sample,

        // According to Microsoft’s documentation, group shared memory is limited to 16KB
        // for D3D10 level hardware and 32KB for D3D11 level hardware. There is no 
        // specification for the amount of shared memory that is available for D3D12.
        // Nvidia suggests don't go beyond 16KB for most optimal performance.
        info.limits.maxComputeSharedMemorySize = 32768;
        info.limits.maxComputeWorkGroupInvocations 
            = D3D12_CS_4_X_THREAD_GROUP_MAX_THREADS_PER_GROUP;
        info.limits.maxComputeWorkGroupSize[0] = D3D12_CS_THREAD_GROUP_MAX_X;
        info.limits.maxComputeWorkGroupSize[1] = D3D12_CS_THREAD_GROUP_MAX_Y;
        info.limits.maxComputeWorkGroupSize[2] = D3D12_CS_THREAD_GROUP_MAX_Z;

        info.limits.maxComputeWorkGroupCount[0] = 
        info.limits.maxComputeWorkGroupCount[1] = 
        info.limits.maxComputeWorkGroupCount[2] = 
            D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
        //max_compute_workgroups_per_dimension:
        //    Direct3D12::D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION,
        // Dx12 does not expose a maximum buffer size in the API.
        // This limit is chosen to avoid potential issues with drivers should they internally
        // store buffer sizes using 32 bit ints (a situation we have already encountered with vulkan).
        
        //max_buffer_size: i32::MAX as u64,
        //max_non_sampler_bindings: 1_000_000,
    }


    
    common::sp<DXCContext> DXCContext::Make() {
        UINT dxgiFactoryFlags = 0;
        // Enable the debug layer (requires the Graphics Tools "optional feature").
        // NOTE: Enabling the debug layer after device creation will invalidate the active device.
        //if(options.debug){
#ifndef NDEBUG
            // Enable the debug layer.
            ID3D12Debug* debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
            {
                debugController->EnableDebugLayer();

                // Enable additional debug layers.
                dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
                debugController->Release();
            }
            else {
                OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
                return nullptr;
            }

            IDXGIInfoQueue* dxgiInfoQueue;
            if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
            {
                dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
                dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
            }
#endif
        //}
        //Create DXGIFactory
        IDXGIFactory4* dxgiFactory;
        auto status = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
        if(FAILED(status)) {
            return nullptr;
        }

        return common::sp(new DXCContext(dxgiFactory));
    }

    
    DXCContext::DXCContext(IDXGIFactory4* factory)
        : _factory(factory)
    { }
       
    DXCContext::~DXCContext() {
        _factory->Release();

        //Check for leak objects
#ifndef NDEBUG
        {
            IDXGIDebug1* dxgiDebug;
            if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
            {
                dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL,
                    DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
            }
        }
#endif

    }

    common::sp<IGraphicsDevice> DXCContext::CreateDefaultDevice(
        const IGraphicsDevice::Options& options
    ) {
        auto adapters = EnumerateAdapters();
        if(adapters.empty())
            return nullptr;
        
        return adapters.front()->RequestDevice(options);
    }

    std::vector<common::sp<IPhysicalAdapter>> DXCContext::EnumerateAdapters() {

        std::vector<common::sp<IPhysicalAdapter>> adapters;

        UINT adapterIndex = 0;
        while(true)
        {
            IDXGIAdapter1* adapter;
            if(DXGI_ERROR_NOT_FOUND == _factory->EnumAdapters1(adapterIndex, &adapter)){
                break;
            }

            ++adapterIndex;

            DXGI_ADAPTER_DESC1 desc;
            auto status = adapter->GetDesc1(&desc);
            if (FAILED(status)) {
                adapter->Release();
                continue;
            }

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                adapter->Release();
                continue;
            }

            // Check to see if the adapter supports Direct3D 12,
            // but don't create the actual device yet.
            if (FAILED(
                D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0,
                    _uuidof(ID3D12Device), nullptr)))
            {
                adapter->Release();
                continue;
            }

            auto dxcAdp = new DXCAdapter(adapter, common::ref_sp(this));
            adapters.emplace_back(dxcAdp);
        }

#ifndef NDEBUG
        if(adapters.empty()) {
            //if(enableDebug){
                //Get a WARP device if debug mode & no dx12 capable device found.
                IDXGIAdapter1* adapter;
                auto status = _factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
                if (SUCCEEDED(status)) {
                    
                    auto dxcAdp = new DXCAdapter(adapter, common::ref_sp(this));
                    adapters.emplace_back(dxcAdp);
                }
            //} else {
                //WARP is a software simulated device for validation use,
                //and mostly irrelevant to release builds
                //return {};
            //}
        }
#endif
        return adapters;
    }

}
