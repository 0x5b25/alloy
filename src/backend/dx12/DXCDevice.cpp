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

HRESULT GetAdapter(bool enableDebug, ComPtr<IDXGIAdapter1>& outAdp) {


    UINT createFactoryFlags = 0;
    if(enableDebug){
        createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }

    ComPtr<IDXGIFactory4> dxgiFactory;
    auto status = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
    if(FAILED(status)) {
        return status;
    } 
    
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
        status = adapter->GetDesc1(&desc);
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
            status = dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&selectedAdp));
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


    DXCDevice::DXCDevice() : _resFactory(this) {}

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

        //Find physical dx12 adapter
        ComPtr<IDXGIAdapter1> adp;
        if(FAILED(GetAdapter(options.debug, adp))){
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
        
        auto dev = sp<DXCDevice>(new DXCDevice());
        //Fill driver and api info
        {
            dev->_apiVer = GraphicsApiVersion::Unknown;
            static const D3D_FEATURE_LEVEL s_featureLevels[] =
            {
                D3D_FEATURE_LEVEL_12_2,
                D3D_FEATURE_LEVEL_12_1,
                D3D_FEATURE_LEVEL_12_0,
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
            };

            D3D12_FEATURE_DATA_FEATURE_LEVELS featLevels =
            {
                _countof(s_featureLevels), s_featureLevels, D3D_FEATURE_LEVEL_11_0
            };

            if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS,
                 &featLevels, sizeof(featLevels))))
            {
                switch (featLevels.MaxSupportedFeatureLevel)
                {
                case D3D_FEATURE_LEVEL_12_2:dev->_apiVer.major = 12; dev->_apiVer.minor = 2; break;
                case D3D_FEATURE_LEVEL_12_1:dev->_apiVer.major = 12; dev->_apiVer.minor = 1; break;
                case D3D_FEATURE_LEVEL_12_0:dev->_apiVer.major = 12; dev->_apiVer.minor = 0;break;
                case D3D_FEATURE_LEVEL_11_1:dev->_apiVer.major = 11; dev->_apiVer.minor = 1;break;
                case D3D_FEATURE_LEVEL_11_0:
                default: dev->_apiVer.major = 11; dev->_apiVer.minor = 0;
                    break;
                }

                dev->_commonFeat.commandListDebugMarkers = featLevels.MaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_11_1;
                dev->_commonFeat.bufferRangeBinding = featLevels.MaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_11_1;
            }
        }

        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
            if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,
                 &options, sizeof(options))))
            {
                dev->_commonFeat.shaderFloat64 = options.DoublePrecisionFloatShaderOps;
            }
        }

        {
            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(adp->GetDesc1(&desc))){
                std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
                const std::wstring wdesc{desc.Description};
                dev->_devName = utf8_conv.to_bytes(wdesc);
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
        

        dev->_adp = std::move(adp);
        dev->_dev = std::move(device);

        return dev;
    }

    void DXCDevice::WaitForIdle() {

    }
   
}
