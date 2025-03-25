#include "DXCDevice.hpp"

//3rd-party headers

//alloy public headers
#include "alloy/common/Common.hpp"
#include "alloy/backend/Backends.hpp"

//standard library headers
#include <algorithm>

//backend specific headers
#include "D3DCommon.hpp"
#include "DXCContext.hpp"
#include "DXCCommandList.hpp"
#include "DXCSwapChain.hpp"

//platform specific headers
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr
#define WIN32_LEAN_AND_MEAN
#include <Windows.h> // For HRESULT


#include <dxgidebug.h>


//Import DX12 agility SDK
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = AGILITY_SDK_VERSION_EXPORT; }

extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

// From DXSampleHelper.h 
// Source: https://github.com/Microsoft/DirectX-Graphics-Samples
//inline void ThrowIfFailed(HRESULT hr)
//{
//    if (FAILED(hr))
//    {
//        throw std::exception();
//    }
//}

using Microsoft::WRL::ComPtr;

HRESULT GetAdapter(IDXGIFactory4* dxgiFactory, bool enableDebug, ComPtr<IDXGIAdapter1>& outAdp) {
    
    ComPtr<IDXGIAdapter1> selectedAdp = nullptr;
    SIZE_T maxDedicatedVideoMemory = 0;
    UINT adapterIndex = 0;
    while(true)
    {
        ComPtr<IDXGIAdapter1> adapter;
        if(DXGI_ERROR_NOT_FOUND == dxgiFactory->EnumAdapters1(adapterIndex, &adapter)){
            break;
        }

        ++adapterIndex;

        DXGI_ADAPTER_DESC1 desc;
        auto status = adapter->GetDesc1(&desc);
        if (FAILED(status))
            continue;

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // Don't select the Basic Render Driver adapter.
            continue;
        }

        // Check to see if the adapter supports Direct3D 12,
        // but don't create the actual device yet.
        if (FAILED(
            D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                _uuidof(ID3D12Device), nullptr)))
        {
            continue;
        }

        if(desc.DedicatedVideoMemory > maxDedicatedVideoMemory ) {
            maxDedicatedVideoMemory = desc.DedicatedVideoMemory;
            //std::swap(adapter, selectedAdp);
            selectedAdp = std::move(adapter);
        }
    }

    if(selectedAdp == nullptr) {
        if(enableDebug){
            //Get a WARP device if debug mode & no dx12 capable device found.
            auto status = dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&selectedAdp));
            if (FAILED(status)) {
                return status;
            }
        } else {
            //WARP is a software simulated device for validation use,
            //and mostly irrelevant to release builds
            return DXGI_ERROR_NOT_FOUND;
        }
    }


    outAdp = std::move(selectedAdp);  
    
    return S_OK;
}


namespace alloy::dxc {

    void D3D12DevCaps::ReadFromDevice(ID3D12Device* pdev)
    {
        D3D_FEATURE_LEVEL checklist[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_2,
        };

        D3D12_FEATURE_DATA_FEATURE_LEVELS levels = {
            _countof(checklist),
            checklist
        };

        if(SUCCEEDED(pdev->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &levels, sizeof(levels)))){
            feature_level = levels.MaxSupportedFeatureLevel;
        }

        D3D12_FEATURE_DATA_SHADER_MODEL shader_model{};
        shader_model.HighestShaderModel = D3D_SHADER_MODEL_6_7;
        if(SUCCEEDED(pdev->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shader_model, sizeof(shader_model)))){
            this->shader_model = shader_model.HighestShaderModel;
        }

        D3D12_FEATURE_DATA_ROOT_SIGNATURE root_sig{};
        root_sig.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if(SUCCEEDED(pdev->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &root_sig, sizeof(root_sig)))){
            root_sig_version = root_sig.HighestVersion;
        }


        pdev->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE1, &architecture, sizeof(architecture));
        pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));
        pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &options1, sizeof(options1));
        pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &options2, sizeof(options2));
        pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3, &options3, sizeof(options3));
        pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS4, &options4, sizeof(options4));
        pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7));
        pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &options12, sizeof(options12));
        pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS13, &options13, sizeof(options13));
        pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS14, &options14, sizeof(options14));
        pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS15, &options15, sizeof(options15));
        pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS16, &options16, sizeof(options16));
        pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS17, &options17, sizeof(options17));
        if (FAILED(pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS19, &options19, sizeof(options19)))) {
            options19.MaxSamplerDescriptorHeapSize = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
            options19.MaxSamplerDescriptorHeapSizeWithStaticSamplers = options19.MaxSamplerDescriptorHeapSize;
            options19.MaxViewDescriptorHeapSize = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1;
        }
        {
            D3D12_FEATURE_DATA_FORMAT_SUPPORT a4b4g4r4_support = {
                DXGI_FORMAT_A4B4G4R4_UNORM
            };
            support_a4b4g4r4 =
             SUCCEEDED(pdev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &a4b4g4r4_support, sizeof(a4b4g4r4_support)));
        }
        D3D12_COMMAND_QUEUE_DESC queue_desc = {
           /*.Type =*/ D3D12_COMMAND_LIST_TYPE_DIRECT,
           /*.Priority =*/ D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
           /*.Flags =*/ D3D12_COMMAND_QUEUE_FLAG_NONE,
           /*.NodeMask =*/ 0,
        };
 
        ID3D12CommandQueue *cmdqueue;
        pdev->CreateCommandQueue(&queue_desc,
                                 IID_ID3D12CommandQueue,
                                 (void **)&cmdqueue);
 
        uint64_t ts_freq;
        cmdqueue->GetTimestampFrequency(&ts_freq);
        timestamp_period = 1000000000.0f / ts_freq;
        cmdqueue->Release();
    }

    DXCAutoFence::DXCAutoFence()
        : _fence(nullptr)
        , _expectedVal(0)
        , _fenceEventHandle(nullptr)
    {}

    DXCAutoFence::~DXCAutoFence(){
        if(_fenceEventHandle!=nullptr){
            CloseHandle(_fenceEventHandle);
        }
    }

    bool DXCAutoFence::Init(ComPtr<ID3D12Fence>&& fence) {
        _fence = fence;
        _expectedVal = 0;

        HANDLE fenceEventHandle = CreateEvent(nullptr, false, false, nullptr);
        if(fenceEventHandle == INVALID_HANDLE_VALUE){
            return false;
        }

        _fenceEventHandle = fenceEventHandle;

        return true;
    }

    HRESULT DXCAutoFence::WaitOnCPU(std::uint32_t timeoutMs) {
        auto expectedVal = _expectedVal.load(std::memory_order_acquire);
        std::lock_guard lockGuard{_waitLock};

        //Return if current fence is already completed
        if(expectedVal <= GetCurrentValue()) return S_OK;
    
        auto result = _fence->SetEventOnCompletion(expectedVal, nullptr);
        if(FAILED(result)) return result;

        auto waitResult = WaitForSingleObjectEx(_fenceEventHandle, timeoutMs, false);
        switch(waitResult){
            //The state of the specified object is signaled.
            case WAIT_OBJECT_0: return S_OK;
            //The time-out interval elapsed, and the object's state is nonsignaled.
            case WAIT_TIMEOUT: return DXGI_ERROR_WAIT_TIMEOUT;
            //Object not released before thread terminate
            case WAIT_ABANDONED:
            //The wait was ended by one or more user-mode asynchronous procedure calls (APC) queued to the thread.
            case WAIT_IO_COMPLETION:
            //Other generic failures
            default: return E_FAIL;
        }
        
    }

    
    HRESULT DXCAutoFence::SignalOnCPU() {
        //Atomically increment the expected fence value
        //auto sigVal = _expectedVal.fetch_add(1, std::memory_order_acq_rel);
        auto sigVal = _expectedVal.load(std::memory_order_acq_rel);

        //acquired old value, we need to +1
        //sigVal++;

        return _fence->Signal(sigVal);
        //WaitForSingleObjectEx(_fenceEventHandle, INFINITE, false);
          
    }

    HRESULT DXCAutoFence::SignalOnCPUAutoInc() {
        //Atomically increment the expected fence value
        auto sigVal = _expectedVal.fetch_add(1, std::memory_order_acq_rel);

        //acquired old value, we need to +1
        //sigVal++;

        return _fence->Signal(sigVal);
        //WaitForSingleObjectEx(_fenceEventHandle, INFINITE, false);
          
    }

    HRESULT DXCAutoFence::InsertWaitToQueue(ID3D12CommandQueue *q) {
        auto expectedVal = _expectedVal.load(std::memory_order_acquire);

        return q->Wait(GetHandle(), expectedVal);
    }

    HRESULT DXCAutoFence::InsertSignalToQueue(ID3D12CommandQueue *q) {
        //Atomically increment the expected fence value
        //auto sigVal = _expectedVal.fetch_add(1, std::memory_order_acq_rel);
        auto sigVal = _expectedVal.load(std::memory_order_acq_rel);
        //acquired old value, we need to +1
        //sigVal++;

        return q->Signal(_fence.Get(), sigVal);
    }

    HRESULT DXCAutoFence::InsertSignalToQueueAutoInc(ID3D12CommandQueue *q) {
        //Atomically increment the expected fence value
        auto sigVal = _expectedVal.fetch_add(1, std::memory_order_acq_rel);
        //acquired old value, we need to +1
        sigVal++;

        return q->Signal(_fence.Get(), sigVal);
    }

    bool DXCAutoFence::IsCompleted() const {
        return _expectedVal.load(std::memory_order_acquire) <= GetCurrentValue();
    }

    DXCDevice::DXCDevice(ID3D12Device* dev) 
        : _dev(dev)
        , _rtvHeap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 64)
        , _dsvHeap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 64)
        , _shaderResHeap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 512)
        , _samplerHeap(dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 512)
    {}

    DXCDevice::~DXCDevice() {
        delete _gfxQ;
        delete _copyQ;

        if (_umaPool) {
            _umaPool->Release();
        }
        _alloc->Release();
        _dev->Release();
    }

    common::sp<IGraphicsDevice> DXCDevice::Make(
        const common::sp<DXCAdapter>& adp,
        const IGraphicsDevice::Options &options
    ) {

        //Enumeration done. create the selected device
        ComPtr<ID3D12Device> device;
        {
            auto hr = D3D12CreateDevice(adp->GetHandle(), D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&device));
            if (FAILED(hr)) {
                return nullptr;
            }
        }

        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        /*typedef struct D3D12_COMMAND_QUEUE_DESC
    {
    D3D12_COMMAND_LIST_TYPE Type;
    INT Priority;
    D3D12_COMMAND_QUEUE_FLAGS Flags;
    UINT NodeMask;
    } 	D3D12_COMMAND_QUEUE_DESC;*/
        //ComPtr<ID3D12CommandQueue> queue;
        //if(FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)))){
        //    return nullptr;
        //}

        //ComPtr<ID3D12CommandAllocator> cmdAlloc;
        //if(FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc)))){
        //    return nullptr;
        //}

        ComPtr<ID3D12Fence> waitIdleFence;
        D3D12_FENCE_FLAGS fenceFlags {};
        if(FAILED(device->CreateFence(0, fenceFlags, IID_PPV_ARGS(&waitIdleFence)))){
            return nullptr;
        }

        D3D12MA::Allocator* allocator;
        
        D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
        allocatorDesc.pDevice = device.Get();
        allocatorDesc.pAdapter = adp->GetHandle();

        
        if(FAILED(D3D12MA::CreateAllocator(&allocatorDesc, &allocator))){
            return nullptr;
        }

        
        auto dev = common::sp<DXCDevice>(new DXCDevice(device.Detach()));
        

        dev->_adp = std::move(adp);
        //dev->_dev = std::move(device);
        //dev->_q = std::move(queue);
        dev->_gfxQ = new DXCCommandQueue(dev.get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
        dev->_copyQ = new DXCCommandQueue(dev.get(), D3D12_COMMAND_LIST_TYPE_COPY);
        //dev->_cmdAlloc = std::move(cmdAlloc);
        dev->_waitIdleFence.Init(std::move(waitIdleFence));
        dev->_alloc = allocator;

        //dev->_adpInfo = {};
        dev->_dxcFeat = {};
        dev->_dxcFeat.ReadFromDevice(dev->_dev);

        //Create CPU accessable VRAM heap for UMA type device
        D3D12MA::Pool* pool = nullptr;
        if (dev->_dxcFeat.SupportUMA()) {
            D3D12MA::POOL_DESC poolDesc = {};
            poolDesc.HeapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
            // For CPU readback use D3D12_CPU_PAGE_PROPERTY_WRITE_BACK
            poolDesc.HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
            poolDesc.HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
            // These flags are optional but recommended.
            poolDesc.Flags = D3D12MA::POOL_FLAG_MSAA_TEXTURES_ALWAYS_COMMITTED;
            poolDesc.HeapFlags = D3D12_HEAP_FLAG_CREATE_NOT_ZEROED;

            HRESULT hr = allocator->CreatePool(&poolDesc, &pool);
            if (FAILED(hr)) {

            }
        }
        dev->_umaPool = pool;

        ////Fill driver and api info
        //{//Get device ID & driver version
        //    DXGI_ADAPTER_DESC1 desc;
        //    if(SUCCEEDED(dev->_adp->GetDesc1(&desc))){
        //        dev->_adpInfo.vendorID = desc.VendorId;
        //        dev->_adpInfo.deviceID = desc.DeviceId;
        //    }
//
        //    LARGE_INTEGER drvVer{};
        //    dev->_adp->CheckInterfaceSupport(__uuidof(IDXGIDevice), &drvVer);
        //    dev->_adpInfo.driverVersion = drvVer.QuadPart;
        //}
//
        //{//Get api version
        //    
        //    auto& apiVer = dev->_adpInfo.apiVersion;
        //    switch (dev->_dxcFeat.feature_level)
        //    {
        //    case D3D_FEATURE_LEVEL_12_2:apiVer.major = 12; apiVer.minor = 2; break;
        //    case D3D_FEATURE_LEVEL_12_1:apiVer.major = 12; apiVer.minor = 1; break;
        //    case D3D_FEATURE_LEVEL_12_0:apiVer.major = 12; apiVer.minor = 0;break;
        //    case D3D_FEATURE_LEVEL_11_1:apiVer.major = 11; apiVer.minor = 1;break;
        //    case D3D_FEATURE_LEVEL_11_0:
        //    default: apiVer.major = 11; apiVer.minor = 0;
        //        break;
        //    }
//
        //    dev->_commonFeat.commandListDebugMarkers = dev->_dxcFeat.feature_level >= D3D_FEATURE_LEVEL_11_1;
        //    dev->_commonFeat.bufferRangeBinding = dev->_dxcFeat.feature_level >= D3D_FEATURE_LEVEL_11_1;
        //    
        //}
//
//
        //{//get device name
        //    DXGI_ADAPTER_DESC1 desc{};
        //    if (SUCCEEDED(dev->_adp->GetDesc1(&desc))){
        //        do{
        //            auto& devNameStr = dev->_adpInfo.deviceName;
        //            auto required_size = WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, NULL, 0, NULL, NULL);
        //            devNameStr.resize(required_size);
        //            if (required_size == 0) break;
//
        //            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, devNameStr.data(), required_size, NULL, NULL);
        //        }while(0);
    //
//
        //        //std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
        //        //const std::wstring wdesc{desc.Description};
        //        //dev->_devName = utf8_conv.to_bytes(wdesc);
        //    }            
        //}

        dev->_commonFeat.computeShader = true;
        dev->_commonFeat.geometryShader = true;
        dev->_commonFeat.tessellationShaders = true;
        dev->_commonFeat.multipleViewports = true;
        dev->_commonFeat.samplerLodBias = true;
        dev->_commonFeat.drawBaseVertex = true;
        dev->_commonFeat.drawBaseInstance = true;
        dev->_commonFeat.drawIndirect = true;
        dev->_commonFeat.drawIndirectBaseInstance = true;
        dev->_commonFeat.fillModeWireframe = true;
        dev->_commonFeat.samplerAnisotropy = true;
        dev->_commonFeat.depthClipDisable = true;
        dev->_commonFeat.texture1D = true;
        dev->_commonFeat.independentBlend = true;
        dev->_commonFeat.structuredBuffer = true;
        dev->_commonFeat.subsetTextureView = true;
        dev->_commonFeat.shaderFloat64 = dev->_dxcFeat.options.DoublePrecisionFloatShaderOps;   
        return dev;
    }

    //void DXCDevice::SubmitCommand(const CommandList* cmd){
    //    auto dxcCmdList = PtrCast<DXCCommandList>(cmd);
//
    //    ID3D12CommandList* pRawCmdList = dxcCmdList->GetHandle();
//
    //    GetImplicitQueue()->ExecuteCommandLists(1, &pRawCmdList);
    //}

    
    IPhysicalAdapter& DXCDevice::GetAdapter() const {return *_adp.get();}

    ISwapChain::State DXCDevice::PresentToSwapChain(ISwapChain *sc)
    {

        auto rawHandle = PtrCast<DXCSwapChain>(sc)->GetHandle();

        rawHandle->Present(1, 0);
        return ISwapChain::State::Optimal;
    }

    ICommandQueue* DXCDevice::GetGfxCommandQueue() {
        return _gfxQ;
    }
    ICommandQueue* DXCDevice::GetCopyCommandQueue() {
        return _copyQ;
    }

    void DXCDevice::WaitForIdle()
    {
        _waitIdleFence.InsertSignalToQueueAutoInc(_gfxQ->GetHandle());
        _waitIdleFence.WaitOnCPU(INFINITE);
    }

    DXCBuffer::~DXCBuffer() {
        _buffer->Release();

        DEBUGCODE(_buffer = nullptr);
    }

    common::sp<IBuffer> DXCBuffer::Make(
        const common::sp<DXCDevice> &dev,
        const IBuffer::Description &desc)
    {
        
        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        //For buffers Alignment must be 64KB (D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT) or 0, 
        //which is effectively 64KB.
        resourceDesc.Alignment = 0;

        auto byteCnt = desc.sizeInBytes;
        if(desc.usage.uniformBuffer) {
            //CBV requires 256byte alignment
            byteCnt = ((byteCnt + 255) / 256 ) * 256;
        }
        resourceDesc.Width = byteCnt;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        if(desc.usage.structuredBufferReadWrite) {
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        D3D12_RESOURCE_STATES resourceState;

        switch (desc.hostAccess)
        {        
        case HostAccess::PreferRead:
            allocationDesc.HeapType = D3D12_HEAP_TYPE_READBACK;
            resourceState = D3D12_RESOURCE_STATE_COPY_DEST;
            break;
        case HostAccess::PreferWrite:
            allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
            break;
        case HostAccess::None:
        default:
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            resourceState = D3D12_RESOURCE_STATE_COMMON;
            break;
        }

        D3D12MA::Allocation* allocation;
        HRESULT hr = dev->Allocator()->CreateResource(
            &allocationDesc,
            &resourceDesc,
            resourceState,
            NULL,
            &allocation,
             IID_NULL, NULL);

        if(FAILED(hr)){
            return nullptr;
        }

        auto buf = new DXCBuffer{ dev, desc };
        buf->_buffer = allocation;
        buf->description.sizeInBytes = byteCnt;

        return common::sp(buf);
    }

    void *DXCBuffer::MapToCPU() {
        
        void* mappedPtr;
        auto hr = GetHandle()->Map(0, NULL, &mappedPtr);
        if(FAILED(hr)){
            return nullptr;
        }

        return mappedPtr;
    }

    DXCCommandQueue::DXCCommandQueue(DXCDevice* pDev, D3D12_COMMAND_LIST_TYPE cmdQType)
        : _dev(pDev)
        , _qType(cmdQType)
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc {};
        queueDesc.Type = cmdQType;// ;
        queueDesc.NodeMask = 0;
        ThrowIfFailed(pDev->GetDevice()->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_q)));
    }

    DXCCommandQueue::~DXCCommandQueue() {
        _q->Release();
    }

    common::sp<ICommandList> DXCCommandQueue::CreateCommandList() {
        _dev->ref();
        return DXCCommandList::Make(common::sp(_dev), _qType);
    }

    //virtual bool WaitForSignal(std::uint64_t timeoutNs) = 0;
    //bool WaitForSignal() {
    //    return WaitForSignal((std::numeric_limits<std::uint64_t>::max)());
    //}
    //virtual bool IsSignaled() const = 0;

    //virtual void Reset() = 0;

    void DXCCommandQueue::EncodeSignalEvent(IEvent* evt, uint64_t value) {
        auto dxcFence = PtrCast<DXCFence>(evt);
        dxcFence->RegisterSyncPoint(this, value);
        _q->Signal(dxcFence->GetHandle(), value);
    }

    void DXCCommandQueue::EncodeWaitForEvent(IEvent* evt, uint64_t value) {
        auto dxcFence = PtrCast<DXCFence>(evt);
        dxcFence->SyncTimelineToThis(this, value);
        _q->Wait(dxcFence->GetHandle(), value);
    }

    void DXCCommandQueue::SubmitCommand(ICommandList* cmd) {
        auto dxcCmdList = PtrCast<DXCCommandList>(cmd);
        auto rawCmdList = dxcCmdList->GetHandle();
        _q->ExecuteCommandLists(1, &rawCmdList);
    }


    
    void DXCBuffer::UnMap() {        
        GetHandle()->Unmap(0, NULL);
    }

    DXCFence::DXCFence(DXCDevice* dev) 
        : _dev(dev)
    {
        dev->ref();

        ThrowIfFailed(dev->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_f)));
        _fenceEventHandle = CreateEvent(nullptr, false, false, nullptr);
        
        if (_fenceEventHandle == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }

    bool DXCFence::WaitFromCPU(uint64_t expectedValue, uint32_t timeoutMs) {

        ThrowIfFailed(_f->SetEventOnCompletion(expectedValue, _fenceEventHandle));

        auto waitResult = WaitForSingleObjectEx(_fenceEventHandle, timeoutMs, false);
        switch(waitResult){
            //The state of the specified object is signaled.
            case WAIT_OBJECT_0: 
                SyncTimelineToThis(_dev.get(), expectedValue);
                return true;
            //The time-out interval elapsed, and the object's state is nonsignaled.
            case WAIT_TIMEOUT: return false;
            //Object not released before thread terminate
            case WAIT_ABANDONED:
            //The wait was ended by one or more user-mode asynchronous procedure calls (APC) queued to the thread.
            case WAIT_IO_COMPLETION:
            //Other generic failures
            default: ThrowIfFailed(E_FAIL);
        }
        return false;
    }

    void DXCFence::SignalFromCPU(uint64_t signalValue) {
        
        RegisterSyncPoint(_dev.get(), signalValue);
        _f->Signal(signalValue);
    }

    DXCFence::~DXCFence() {
        CloseHandle(_fenceEventHandle);
        _f->Release();
    }

    
    void DXCFence::RegisterSyncPoint(IDXCTimeline* timeline, uint64_t syncValue) {
        _signalingTimelines[timeline] = syncValue;
    }

    void DXCFence::SyncTimelineToThis(IDXCTimeline* timeline, uint64_t syncValue) {

        struct _Kvpair {
            IDXCTimeline* timeline;
            uint64_t fenceValue;
        };

        std::vector<_Kvpair> timelinesSorted{};

        for(auto& [k, v] : _signalingTimelines) {
            if(k == timeline) {
                //Same queue signal and wait may cause deadlocks.
                //Put an assertion here for ease of debugging
                assert(v >= syncValue);
                continue;
            }
            timelinesSorted.push_back({k, v});
        }

        std::sort(timelinesSorted.begin(), timelinesSorted.end(),
            [](const _Kvpair& a, const _Kvpair& b) {
                return a.fenceValue < b.fenceValue;
            }
        );

        IDXCTimeline::ResourceStates states {};
        for(auto&[k, _] : timelinesSorted) {
            states.SyncTo(k->GetCurrentState());
        }

        timeline->GetCurrentState().SyncTo(states);
    }

    
} // namespace alloy::dxc
