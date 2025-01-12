#pragma once

//3rd-party headers
#include <d3d12.h>
#include <D3D12MemAlloc.h>

//veldrid public headers
#include "veldrid/common/RefCnt.hpp"

#include "veldrid/GraphicsDevice.hpp"
#include "veldrid/SyncObjects.hpp"
#include "veldrid/Buffer.hpp"
#include "veldrid/SwapChain.hpp"
#include "veldrid/CommandQueue.hpp"

//standard library headers
#include <string>
#include <mutex>
#include <atomic>
//backend specific headers

//platform specific headers
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers
#include "DXCResourceFactory.hpp"
#include "D3DDescriptorHeapMgr.hpp"

namespace Veldrid{

    class DXCFence;
    class DXCCommandQueue;
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

    
    class DXCCommandQueue : public CommandQueue{

        ID3D12CommandQueue* _q;
        D3D12_COMMAND_LIST_TYPE _qType;
        DXCDevice* _dev;

    public:

        DXCCommandQueue(DXCDevice* pDev, D3D12_COMMAND_LIST_TYPE cmdQType);

        ID3D12CommandQueue* GetHandle() const {return _q;}

        virtual ~DXCCommandQueue() override;

        //virtual bool WaitForSignal(std::uint64_t timeoutNs) override;
        //bool WaitForSignal() {
        //    return WaitForSignal((std::numeric_limits<std::uint64_t>::max)());
        //}
        //virtual bool IsSignaled() const override;

        //virtual void Reset() override;

        virtual void EncodeSignalFence(Fence* fence, uint64_t value) override;

        virtual void EncodeWaitForFence(Fence* fence, uint64_t value) override;

        virtual void SubmitCommand(CommandList* cmd) override;
        
        virtual sp<CommandList> CreateCommandList() override;

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

        void ReadFromDevice(ID3D12Device* pdev);

        bool SupportEnhancedBarrier() const { return options12.EnhancedBarriersSupported; }
        bool SupportMeshShader() const {return options7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;}
        bool SupportReBAR() const { return options16.GPUUploadHeapSupported; }
        bool SupportUMA() const { return architecture.UMA || architecture.CacheCoherentUMA; }
    
    };

    class DXCDevice : public  GraphicsDevice, public DXCResourceFactory {

    public:



    private:
        Microsoft::WRL::ComPtr<IDXGIAdapter1> _adp;
        Microsoft::WRL::ComPtr<ID3D12Device> _dev;

        //Microsoft::WRL::ComPtr<ID3D12CommandQueue> _q;

        DXCCommandQueue* _gfxQ;
        DXCCommandQueue* _copyQ;

        //Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _cmdAlloc;
        DXCAutoFence _waitIdleFence;

        Microsoft::WRL::ComPtr<D3D12MA::Allocator> _alloc;

        Veldrid::DXC::_DescriptorHeapMgr _rtvHeap, _dsvHeap;

        //DXCResourceFactory _resFactory;

        //GraphicsApiVersion _apiVer;
        AdapterInfo _adpInfo;

        GraphicsDevice::Features _commonFeat;
        D3D12DevCaps _dxcFeat;

        DXCDevice(const Microsoft::WRL::ComPtr<ID3D12Device>& dev);

    public:

        ~DXCDevice();

        ID3D12Device* GetDevice() const {return _dev.Get();}
        IDXGIAdapter1* GetDxgiAdp() const {return _adp.Get();}

        virtual void* GetNativeHandle() const override { return GetDevice(); }

        static sp<GraphicsDevice> Make(
            const GraphicsDevice::Options& options
        );

        //virtual const std::string& DeviceName() const override { return _devName; }
        //virtual const std::string& VendorName() const override { return ""; }
        //virtual const GraphicsApiVersion ApiVersion() const override { return _apiVer; }
        virtual const GraphicsDevice::AdapterInfo& GetAdapterInfo() const override { return _adpInfo; }
        virtual const GraphicsDevice::Features& GetFeatures() const override { return _commonFeat; }

        const D3D12DevCaps& GetDevCaps() const { return _dxcFeat; }

        virtual ResourceFactory* GetResourceFactory() override { return this; };

        D3D12MA::Allocator* Allocator() const {return _alloc.Get();}

        ID3D12CommandQueue* GetImplicitQueue() const {return _gfxQ->GetHandle(); } 
        //virtual void SubmitCommand(const CommandList* cmd) override;
        virtual SwapChain::State PresentToSwapChain(
            const std::vector<Semaphore*>& waitSemaphores,
            SwapChain* sc) override;
               
        virtual CommandQueue* GetGfxCommandQueue() override;
        virtual CommandQueue* GetCopyCommandQueue() override;

        virtual void WaitForIdle() override;

    };

    class DXCFence : public Fence {

        ID3D12Fence* _f;
        //SetEventOnCompletion will block until complete if handle is null
        HANDLE _fenceEventHandle;

    public:

        DXCFence(DXCDevice* dev);

        virtual ~DXCFence() override;

        ID3D12Fence* GetHandle() const {return _f;}

        //virtual bool WaitForSignal(std::uint64_t timeoutNs) override;
        //bool WaitForSignal() {
        //    return WaitForSignal((std::numeric_limits<std::uint64_t>::max)());
        //}
        //virtual bool IsSignaled() const override;

        //virtual void Reset() override;

        virtual uint64_t GetCurrentValue() override {
            return _f->GetCompletedValue();
        }
        virtual void SignalFromCPU(uint64_t signalValue) override {
            _f->Signal(signalValue);
        }
        virtual bool WaitFromCPU(uint64_t expectedValue, uint32_t timeoutMs) override;
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

        
        virtual uint64_t GetNativeHandle() const {return GetHandle()->GetGPUVirtualAddress();}

        ID3D12Resource* GetHandle() const {return _buffer->GetResource();}

        virtual void* MapToCPU();

        virtual void UnMap();

    };



    //class DXCVLDFence : public Fence {
    //
    //    DXCAutoFence _fence;
    //
    //    DXCVLDFence(const sp<GraphicsDevice>& dev) : Fence(dev) {}
    //public:
    //    ~DXCVLDFence() {}
    //
    //    static sp<Fence> Make(
    //        const sp<DXCDevice>& dev,
    //        bool signaled = false
    //    );
    //
    //    const DXCAutoFence& GetHandle() const { return _fence; }
    //
    //    bool WaitForSignal(std::uint64_t timeoutNs) override;
    //
    //    bool IsSignaled() const override { return _fence.IsCompleted(); }
    //
    //    void Reset() override { _fence.IncrementFence();  }
    //};

    

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
