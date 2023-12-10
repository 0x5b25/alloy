#pragma once

//3rd-party headers
#include <D3D12MemAlloc.h>

//veldrid public headers
#include "veldrid/common/RefCnt.hpp"

#include "veldrid/GraphicsDevice.hpp"
#include "veldrid/SyncObjects.hpp"
#include "veldrid/Buffer.hpp"
#include "veldrid/SwapChain.hpp"

//standard library headers
#include <string>
#include <mutex>
#include <atomic>
//backend specific headers

//platform specific headers
#include <d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers
#include "DXCResourceFactory.hpp"

namespace Veldrid{


    class DXCQueue : public RefCntBase{
    private:
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> _queue;
        D3D12_COMMAND_LIST_TYPE _queueType;
    
        std::mutex mFenceMutex;
        std::mutex mEventMutex;
    
        Microsoft::WRL::ComPtr<ID3D12Fence> _fence;
        std::uint64_t mNextFenceValue;
        std::uint64_t _lastCompletedFenceValue;
        HANDLE _fenceEventHandle;

        DXCQueue() {}

    public:

        static sp<DXCQueue> Make(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE commandType);


        ~DXCQueue();
    
        bool IsFenceComplete(std::uint64_t fenceValue);
        void InsertWait(std::uint64_t fenceValue);
        void InsertWaitForQueueFence(DXCQueue* otherQueue, std::uint64_t fenceValue);
        void InsertWaitForQueue(DXCQueue* otherQueue);
    
        void WaitForFenceCPUBlocking(std::uint64_t fenceValue);
        void WaitForIdle() { WaitForFenceCPUBlocking(mNextFenceValue - 1); }
    
        ID3D12CommandQueue* GetCommandQueue() { return _queue.Get(); }
    
        std::uint64_t PollCurrentFenceValue();
        std::uint64_t GetLastCompletedFence() { return _lastCompletedFenceValue; }
        std::uint64_t GetNextFenceValue() { return mNextFenceValue; }
        ID3D12Fence* GetFence() { return _fence.Get(); }
    
        std::uint64_t ExecuteCommandList(ID3D12CommandList* List);    
    
    };

    //Signal operation will increment expected fence value, so a wait-signal pattern is required
    class DXCAutoFence {
        Microsoft::WRL::ComPtr<ID3D12Fence> _fence;
        std::atomic<std::uint64_t> _expectedVal;
        std::mutex _waitLock;

        //SetEventOnCompletion will block until complete if handle is null
        HANDLE _fenceEventHandle;

    public:
        DXCAutoFence();

        ~DXCAutoFence();

        bool Init(Microsoft::WRL::ComPtr<ID3D12Fence>&& fence);

        ID3D12Fence* GetHandle() { return _fence.Get(); }
        std::uint64_t GetCurrentValue() const { return _fence->GetCompletedValue(); }

        HRESULT WaitOnCPU(std::uint32_t timeoutMs);
        HRESULT SignalOnCPU();
        HRESULT SignalOnCPUAutoInc(); //Automatic increment fence

        bool IsCompleted() const;

        HRESULT InsertWaitToQueue(ID3D12CommandQueue* q);
        HRESULT InsertSignalToQueue(ID3D12CommandQueue* q);
        HRESULT InsertSignalToQueueAutoInc(ID3D12CommandQueue* q);//Automatic increment fence

        void IncrementFence() { ++_expectedVal; }

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
        //D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12;
        //D3D12_FEATURE_DATA_D3D12_OPTIONS13 options13;
        //D3D12_FEATURE_DATA_D3D12_OPTIONS14 options14;
        //D3D12_FEATURE_DATA_D3D12_OPTIONS15 options15;
        //D3D12_FEATURE_DATA_D3D12_OPTIONS16 options16;
        //D3D12_FEATURE_DATA_D3D12_OPTIONS17 options17;
        //D3D12_FEATURE_DATA_D3D12_OPTIONS19 options19;

        float timestamp_period;
        //bool support_a4b4g4r4;

        void ReadFromDevice(ID3D12Device* pdev);
    };

    class DXCDevice : public  GraphicsDevice {

    public:



    private:
        Microsoft::WRL::ComPtr<IDXGIAdapter1> _adp;
        Microsoft::WRL::ComPtr<ID3D12Device> _dev;

        Microsoft::WRL::ComPtr<ID3D12CommandQueue> _q;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _cmdAlloc;
        DXCAutoFence _waitIdleFence;

        Microsoft::WRL::ComPtr<D3D12MA::Allocator> _alloc;


        DXCResourceFactory _resFactory;

        //GraphicsApiVersion _apiVer;
        AdapterInfo _adpInfo;

        GraphicsDevice::Features _commonFeat;
        D3D12DevCaps _dxcFeat;

        DXCDevice();

    public:

        ~DXCDevice();

        ID3D12Device* GetDevice() {return _dev.Get();}

        static sp<GraphicsDevice> Make(
            const GraphicsDevice::Options& options,
            SwapChainSource* swapChainSource = nullptr
        );

        //virtual const std::string& DeviceName() const override { return _devName; }
        //virtual const std::string& VendorName() const override { return ""; }
        //virtual const GraphicsApiVersion ApiVersion() const override { return _apiVer; }
        virtual const GraphicsDevice::AdapterInfo& GetAdapterInfo() const override { return _adpInfo; }
        virtual const GraphicsDevice::Features& GetFeatures() const override { return _commonFeat; }

        const D3D12DevCaps& GetDevCaps() const { return _dxcFeat; }

        virtual ResourceFactory* GetResourceFactory() override { return &_resFactory; };

        ID3D12CommandQueue* GraphicsQueue() const {return _q.Get();}

        D3D12MA::Allocator* Allocator() const {return _alloc.Get();}


        virtual void SubmitCommand(
            const std::vector<SubmitBatch>& batch,
            Fence* fence) override;
        virtual SwapChain::State PresentToSwapChain(
            const std::vector<Semaphore*>& waitSemaphores,
            SwapChain* sc) override;
               
        virtual void WaitForIdle() override;

    };

    
    class DXCBuffer : public Buffer{

    private:
        D3D12MA::Allocation* _buffer;
        //VmaAllocation _allocation;

        //VkBufferUsageFlags _usages;
        //VmaMemoryUsage _allocationType;

        DXCDevice* _Dev() const {
            return static_cast<DXCDevice*>(dev.get());
        }

        DXCBuffer(
            const sp<GraphicsDevice>& dev,
            const Buffer::Description& desc
        ) : 
            Buffer(dev, desc)
        {}

    public:

        virtual ~DXCBuffer();

        virtual std::string GetDebugName() const override {
            return "<VK_DEVBUF>";
        }

        static sp<Buffer> Make(
            const sp<DXCDevice>& dev,
            const Buffer::Description& desc
        );

        ID3D12Resource* GetHandle() const {return _buffer->GetResource();}

        virtual void* MapToCPU();

        virtual void UnMap();

    };


    class DXCVLDFence : public Fence {

        DXCAutoFence _fence;

        DXCVLDFence(const sp<GraphicsDevice>& dev) : Fence(dev) {}
    public:
        ~DXCVLDFence() {}

        static sp<Fence> Make(
            const sp<DXCDevice>& dev,
            bool signaled = false
        );

        const DXCAutoFence& GetHandle() const { return _fence; }

        bool WaitForSignal(std::uint64_t timeoutNs) override;

        bool IsSignaled() const override { return _fence.IsCompleted(); }

        void Reset() override { _fence.IncrementFence();  }
    };

    

    class DXCVLDSemaphore : public Semaphore {

        DXCAutoFence _fence;

        DXCVLDSemaphore(const sp<GraphicsDevice>& dev) : Semaphore(dev) {}
    public:
        ~DXCVLDSemaphore() {}

        static sp<Semaphore> Make(
            const sp<DXCDevice>& dev
        );

        const DXCAutoFence& GetHandle() const { return _fence; }
    };

}
