#include "DXCDevice.hpp"

//3rd-party headers

//veldrid public headers
#include "veldrid/common/Common.hpp"
#include "veldrid/backend/Backends.hpp"

//standard library headers
#include <codecvt>
//backend specific headers

//platform specific headers
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr
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
            D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
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

namespace Veldrid {

    
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
        //pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &pdev->options12, sizeof(pdev->options12));
        //pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS13, &pdev->options13, sizeof(pdev->options13));
        //pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS14, &pdev->options14, sizeof(pdev->options14));
        //pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS15, &pdev->options15, sizeof(pdev->options15));
        //pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS16, &pdev->options16, sizeof(pdev->options16));
        //pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS17, &pdev->options17, sizeof(pdev->options17));
        //if (FAILED(pdev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS19, &pdev->options19, sizeof(pdev->options19)))) {
        //    pdev->options19.MaxSamplerDescriptorHeapSize = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
        //    pdev->options19.MaxSamplerDescriptorHeapSizeWithStaticSamplers = pdev->options19.MaxSamplerDescriptorHeapSize;
        //    pdev->options19.MaxViewDescriptorHeapSize = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1;
        //}
        //{
        //    D3D12_FEATURE_DATA_FORMAT_SUPPORT a4b4g4r4_support = {
        //        DXGI_FORMAT_A4B4G4R4_UNORM
        //    };
        //    support_a4b4g4r4 =
        //     SUCCEEDED(pdev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &a4b4g4r4_support, sizeof(a4b4g4r4_support)));
        //}
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


    sp<GraphicsDevice> CreateDX12GraphicsDevice(
        const GraphicsDevice::Options& options,
        SwapChainSource* swapChainSource
    ) {
        return DXCDevice::Make(options, swapChainSource);
    }


    sp<DXCQueue> DXCQueue::Make(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE commandType){

        ComPtr<ID3D12CommandQueue> cmdQueue;
        D3D12_COMMAND_QUEUE_DESC queueDesc {};
        queueDesc.Type = commandType;
        queueDesc.NodeMask = 0;
        if(FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue)))){
            return nullptr;
        }


    
        ComPtr<ID3D12Fence> fence;
        if(FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))){
            return nullptr;
        }

        HANDLE fenceEventHandle = CreateEventEx(NULL, false, false, EVENT_ALL_ACCESS);
        if(fenceEventHandle == INVALID_HANDLE_VALUE){
            return nullptr;
        }

        auto dxcQueue = sp<DXCQueue>(new DXCQueue());
        dxcQueue->_queue = std::move(cmdQueue);
        dxcQueue->_queueType = commandType;
        dxcQueue->_fence = std::move(fence);
        dxcQueue->_fenceEventHandle = fenceEventHandle;
        dxcQueue->_lastCompletedFenceValue = ((uint64_t)commandType << 56);
    
        fence->Signal(dxcQueue->_lastCompletedFenceValue);

        return dxcQueue;    
    }

    DXCQueue::~DXCQueue() {
        CloseHandle(_fenceEventHandle);
    }

    std::uint64_t DXCQueue::PollCurrentFenceValue() {
        _lastCompletedFenceValue = std::max(_lastCompletedFenceValue, _fence->GetCompletedValue());
        return _lastCompletedFenceValue;
    }
    
    bool DXCQueue::IsFenceComplete(std::uint64_t fenceValue) {

        if (fenceValue > _lastCompletedFenceValue) {
            PollCurrentFenceValue();
        }
    
        return fenceValue <= _lastCompletedFenceValue;
    }

    void DXCQueue::InsertWait(std::uint64_t fenceValue){
        _queue->Wait(_fence.Get(), fenceValue);
    }
 
    void DXCQueue::InsertWaitForQueueFence(DXCQueue* otherQueue, std::uint64_t fenceValue) {
        _queue->Wait(otherQueue->GetFence(), fenceValue);
    }
 
    void DXCQueue::InsertWaitForQueue(DXCQueue* otherQueue) {
        _queue->Wait(otherQueue->GetFence(), otherQueue->GetNextFenceValue() - 1);
    }

    void DXCQueue::WaitForFenceCPUBlocking(std::uint64_t fenceValue) {
        if (IsFenceComplete(fenceValue)) return;
 
        {
            std::lock_guard<std::mutex> lockGuard(mEventMutex);
    
            _fence->SetEventOnCompletion(fenceValue, _fenceEventHandle);
            WaitForSingleObjectEx(_fenceEventHandle, INFINITE, false);
            _lastCompletedFenceValue = fenceValue;
        }
    }

    std::uint64_t DXCQueue::ExecuteCommandList(ID3D12CommandList* commandList) {
        if( FAILED(((ID3D12GraphicsCommandList*)commandList)->Close()) ){
            return 0;
        }
        _queue->ExecuteCommandLists(1, &commandList);
    
        std::lock_guard<std::mutex> lockGuard(mFenceMutex);
    
        _queue->Signal(GetFence(), mNextFenceValue);

        return mNextFenceValue++;
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

        HANDLE fenceEventHandle = CreateEventEx(NULL, false, false, EVENT_ALL_ACCESS);
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
        //sigVal++;

        return q->Signal(_fence.Get(), sigVal);
    }

    bool DXCAutoFence::IsCompleted() const {
        return _expectedVal.load(std::memory_order_acquire) <= GetCurrentValue();
    }

    DXCDevice::DXCDevice() 
        : _resFactory(this)
    {}

    DXCDevice::~DXCDevice() {
    }

    sp<GraphicsDevice> DXCDevice::Make(const GraphicsDevice::Options &options, SwapChainSource *swapChainSource)
    {
        if(options.debug){
            // Enable the debug layer.
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
            {
                debugController->EnableDebugLayer();
            }
        }
        //Create DXGIFactory
        UINT createFactoryFlags = 0;
        if(options.debug){
            createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }

        ComPtr<IDXGIFactory4> dxgiFactory;
        auto status = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
        if(FAILED(status)) {
            return nullptr;
        } 

        //Find physical dx12 adapter
        ComPtr<IDXGIAdapter1> adp;
        if(FAILED(GetAdapter(dxgiFactory.Get(), options.debug, adp))){
            return nullptr;
        }

        //Enumeration done. create the selected device
        ComPtr<ID3D12Device> device;
        if(FAILED(D3D12CreateDevice(adp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))){
            return nullptr;
        }

        if(swapChainSource!=nullptr){
            //Save data for later swap chain creation
        }

        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        /*typedef struct D3D12_COMMAND_QUEUE_DESC
    {
    D3D12_COMMAND_LIST_TYPE Type;
    INT Priority;
    D3D12_COMMAND_QUEUE_FLAGS Flags;
    UINT NodeMask;
    } 	D3D12_COMMAND_QUEUE_DESC;*/
        ComPtr<ID3D12CommandQueue> queue;
        if(FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)))){
            return nullptr;
        }

        ComPtr<ID3D12CommandAllocator> cmdAlloc;
        if(FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc)))){
            return nullptr;
        }

        ComPtr<ID3D12Fence> waitIdleFence;
        D3D12_FENCE_FLAGS fenceFlags {};
        if(FAILED(device->CreateFence(0, fenceFlags, IID_PPV_ARGS(&waitIdleFence)))){
            return nullptr;
        }

        ComPtr<D3D12MA::Allocator> allocator;
        
        D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
        allocatorDesc.pDevice = device.Get();
        allocatorDesc.pAdapter = adp.Get();

        if(FAILED(D3D12MA::CreateAllocator(&allocatorDesc, &allocator))){
            return nullptr;
        }

        
        auto dev = sp<DXCDevice>(new DXCDevice());
        

        dev->_adp = std::move(adp);
        dev->_dev = std::move(device);
        dev->_q = std::move(queue);
        dev->_cmdAlloc = std::move(cmdAlloc);
        dev->_waitIdleFence.Init(std::move(waitIdleFence));
        dev->_alloc = std::move(allocator);

        dev->_adpInfo = {};
        dev->_dxcFeat = {};
        dev->_dxcFeat.ReadFromDevice(dev->_dev.Get());

        //Fill driver and api info
        {//Get device ID & driver version
            DXGI_ADAPTER_DESC1 desc;
            if(SUCCEEDED(dev->_adp->GetDesc1(&desc))){
                dev->_adpInfo.vendorID = desc.VendorId;
                dev->_adpInfo.deviceID = desc.DeviceId;
            }

            LARGE_INTEGER drvVer{};
            dev->_adp->CheckInterfaceSupport(__uuidof(IDXGIDevice), &drvVer);
            dev->_adpInfo.driverVersion = drvVer.QuadPart;
        }

        {//Get api version
            
            auto& apiVer = dev->_adpInfo.apiVersion;
            switch (dev->_dxcFeat.feature_level)
            {
            case D3D_FEATURE_LEVEL_12_2:apiVer.major = 12; apiVer.minor = 2; break;
            case D3D_FEATURE_LEVEL_12_1:apiVer.major = 12; apiVer.minor = 1; break;
            case D3D_FEATURE_LEVEL_12_0:apiVer.major = 12; apiVer.minor = 0;break;
            case D3D_FEATURE_LEVEL_11_1:apiVer.major = 11; apiVer.minor = 1;break;
            case D3D_FEATURE_LEVEL_11_0:
            default: apiVer.major = 11; apiVer.minor = 0;
                break;
            }

            dev->_commonFeat.commandListDebugMarkers = dev->_dxcFeat.feature_level >= D3D_FEATURE_LEVEL_11_1;
            dev->_commonFeat.bufferRangeBinding = dev->_dxcFeat.feature_level >= D3D_FEATURE_LEVEL_11_1;
            
        }


        {//get device name
            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(dev->_adp->GetDesc1(&desc))){
                do{
                    auto& devNameStr = dev->_adpInfo.deviceName;
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

    void DXCDevice::SubmitCommand(const std::vector<SubmitBatch> &batch, Fence *fence){
    }
    SwapChain::State DXCDevice::PresentToSwapChain(const std::vector<Semaphore *> &waitSemaphores, SwapChain *sc)
    {
        for(auto* sem : waitSemaphores){
            //auto dxcSem = static_cast<DXCVLDFence*>(sem);
        }
        
        return SwapChain::State::Optimal;
    }

    void DXCDevice::WaitForIdle()
    {
        _waitIdleFence.InsertSignalToQueueAutoInc(_q.Get());
        _waitIdleFence.WaitOnCPU(INFINITE);
    }

    DXCBuffer::~DXCBuffer() {
        _buffer->Release();

        DEBUGCODE(_buffer = nullptr);
    }

    sp<Buffer> DXCBuffer::Make(
        const sp<DXCDevice> &dev,
        const Buffer::Description &desc)
    {
        
        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        //For buffers Alignment must be 64KB (D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT) or 0, 
        //which is effectively 64KB.
        resourceDesc.Alignment = 0;
        resourceDesc.Width = desc.sizeInBytes;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        D3D12_RESOURCE_STATES resourceState;

        switch (desc.hostAccess)
        {        
        case Description::HostAccess::PreferRead:
            allocationDesc.HeapType = D3D12_HEAP_TYPE_READBACK;
            resourceState = D3D12_RESOURCE_STATE_COPY_DEST;
            break;
        case Description::HostAccess::PreferWrite:
            allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
            break;
        case Description::HostAccess::None:
        default:
            allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
            resourceState = D3D12_RESOURCE_STATE_COMMON;
            break;
        }

        allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

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

        return sp(buf);
    }

    void *DXCBuffer::MapToCPU() {
        
        void* mappedPtr;
        auto hr = GetHandle()->Map(0, NULL, &mappedPtr);
        if(FAILED(hr)){
            return nullptr;
        }

        return mappedPtr;
    }

    
    void DXCBuffer::UnMap() {        
        GetHandle()->Unmap(0, NULL);
    }

    sp<Fence> DXCVLDFence::Make(const sp<DXCDevice> &dev, bool signaled) {

        ComPtr<ID3D12Fence> fence;
        D3D12_FENCE_FLAGS fenceFlags {};
        if(FAILED(dev->GetDevice()->CreateFence(0, fenceFlags, IID_PPV_ARGS(&fence)))){
            return nullptr;
        }


        auto fen = sp<DXCVLDFence>{new DXCVLDFence(dev)};
        if(!fen->_fence.Init(std::move(fence))){
            return nullptr;
        }

        return fen;
    }

    
    bool DXCVLDFence::WaitForSignal(std::uint64_t timeoutNs) {
        auto timeoutMsLong = timeoutNs / 10000000;
        std::uint32_t timeoutMs = timeoutMsLong;
        if(timeoutMsLong >= 0xFFFFFFFF){
            timeoutMs = 0xFFFFFFFF;
        }

        auto res = _fence.WaitOnCPU(timeoutMs);

        return SUCCEEDED(res);
    }

    
    sp<Semaphore> DXCVLDSemaphore::Make(const sp<DXCDevice> &dev) {

        ComPtr<ID3D12Fence> fence;
        D3D12_FENCE_FLAGS fenceFlags {};
        if(FAILED(dev->GetDevice()->CreateFence(0, fenceFlags, IID_PPV_ARGS(&fence)))){
            return nullptr;
        }


        auto sem = sp<DXCVLDSemaphore>{new DXCVLDSemaphore(dev)};
        if(!sem->_fence.Init(std::move(fence))){
            return nullptr;
        }

        return sem;
    }


}
