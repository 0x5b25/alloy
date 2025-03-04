#pragma once

//3rd-party headers
#include <d3d12.h>
#include <D3D12MemAlloc.h>

//alloy public headers
#include "alloy/common/RefCnt.hpp"

#include "alloy/GraphicsDevice.hpp"
#include "alloy/SyncObjects.hpp"
#include "alloy/Buffer.hpp"
#include "alloy/SwapChain.hpp"
#include "alloy/CommandQueue.hpp"

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

namespace alloy::dxc{

    class DXCFence;
    class DXCCommandQueue;
    class DXCAdapter;

    class DXCCommandQueue : public ICommandQueue{

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

        virtual void EncodeSignalEvent(IEvent* evt, uint64_t value) override;

        virtual void EncodeWaitForEvent(IEvent* evt, uint64_t value) override;

        virtual void SubmitCommand(ICommandList* cmd) override;
        
        virtual common::sp<ICommandList> CreateCommandList() override;

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
    
        uint32_t ResourceBindingTier() const {
            switch(options.ResourceBindingTier) {
                case D3D12_RESOURCE_BINDING_TIER_3: return 3;
                case D3D12_RESOURCE_BINDING_TIER_2: return 2;
                case D3D12_RESOURCE_BINDING_TIER_1:
                default: return 1;
            }
        }
    };

    class DXCDevice : public  IGraphicsDevice, public DXCResourceFactory {

    private:
        //Microsoft::WRL::ComPtr<IDXGIAdapter1> _adp;
        
        common::sp<DXCAdapter> _adp;
        
        ID3D12Device* _dev;

        //Microsoft::WRL::ComPtr<ID3D12CommandQueue> _q;

        DXCCommandQueue* _gfxQ;
        DXCCommandQueue* _copyQ;

        //Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _cmdAlloc;
        DXCAutoFence _waitIdleFence;

        D3D12MA::Allocator* _alloc;
        D3D12MA::Pool* _umaPool;

        alloy::dxc::_DescriptorHeapMgr _rtvHeap, _dsvHeap;

        _ShaderResDescriptorHeapMgr _shaderResHeap, _samplerHeap;

        //DXCResourceFactory _resFactory;

        //GraphicsApiVersion _apiVer;
        

        IGraphicsDevice::Features _commonFeat;
        D3D12DevCaps _dxcFeat;

        DXCDevice(ID3D12Device* dev);

    public:

        ~DXCDevice();

        ID3D12Device* GetDevice() const {return _dev;}
        IPhysicalAdapter& GetAdapter() const override;

        virtual void* GetNativeHandle() const override { return GetDevice(); }

        _Descriptor AllocateRTV() { return _rtvHeap.Allocate(); }
        void FreeRTV(const _Descriptor& desc) { _rtvHeap.Free(desc); }

        _Descriptor AllocateDSV() { return _dsvHeap.Allocate(); }
        void FreeDSV(const _Descriptor& desc) { _dsvHeap.Free(desc); }

        _ShaderResDescriptor AllocateShaderRes(uint32_t count) { return _shaderResHeap.Allocate(count); }
        void FreeShaderRes(const _ShaderResDescriptor& desc) { _shaderResHeap.Free(desc); }

        _ShaderResDescriptor AllocateSampler(uint32_t count) { return _samplerHeap.Allocate(count); }
        void FreeSampler(const _ShaderResDescriptor& desc) { _samplerHeap.Free(desc); }

        static common::sp<IGraphicsDevice> Make(
            const common::sp<DXCAdapter>& adp,
            const IGraphicsDevice::Options& options
        );

        //virtual const std::string& DeviceName() const override { return _devName; }
        //virtual const std::string& VendorName() const override { return ""; }
        //virtual const GraphicsApiVersion ApiVersion() const override { return _apiVer; }
        //virtual const IGraphicsDevice::AdapterInfo& GetAdapterInfo() const override { return _adpInfo; }
        virtual const IGraphicsDevice::Features& GetFeatures() const override { return _commonFeat; }

        const D3D12DevCaps& GetDevCaps() const { return _dxcFeat; }

        virtual ResourceFactory& GetResourceFactory() override { return *this; };

        D3D12MA::Allocator* Allocator() const { return _alloc; }
        D3D12MA::Pool* UMAPool() const {return _umaPool;}

        ID3D12CommandQueue* GetImplicitQueue() const {return _gfxQ->GetHandle(); } 
        //virtual void SubmitCommand(const CommandList* cmd) override;
        virtual ISwapChain::State PresentToSwapChain(
            ISwapChain* sc) override;
               
        virtual ICommandQueue* GetGfxCommandQueue() override;
        virtual ICommandQueue* GetCopyCommandQueue() override;

        virtual void WaitForIdle() override;

    };

    class DXCFence : public IEvent {

        common::sp<DXCDevice> _dev;

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

        virtual uint64_t GetSignaledValue() override {
            return _f->GetCompletedValue();
        }
        virtual void SignalFromCPU(uint64_t signalValue) override {
            _f->Signal(signalValue);
        }
        virtual bool WaitFromCPU(uint64_t expectedValue, uint32_t timeoutMs) override;
    };

    
    class DXCBuffer : public IBuffer{

    private:
        common::sp<DXCDevice> _dev;
        D3D12MA::Allocation* _buffer;
        //VmaAllocation _allocation;

        //VkBufferUsageFlags _usages;
        //VmaMemoryUsage _allocationType;

        DXCBuffer(
            const common::sp<DXCDevice>& dev,
            const IBuffer::Description& desc
        )
            : IBuffer(desc)
            , _dev(dev)
        {}

    public:

        virtual ~DXCBuffer();

        //virtual std::string GetDebugName() const override {
        //    return "<VK_DEVBUF>";
        //}

        static common::sp<IBuffer> Make(
            const common::sp<DXCDevice>& dev,
            const IBuffer::Description& desc
        );

        
        virtual uint64_t GetNativeHandle() const {return GetHandle()->GetGPUVirtualAddress();}

        ID3D12Resource* GetHandle() const {return _buffer->GetResource();}

        virtual void* MapToCPU();

        virtual void UnMap();


        virtual void SetDebugName(const std::string& name) {
            GetHandle()->SetPrivateData( WKPDID_D3DDebugObjectName, name.size(), name.data() );
        }

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

    

    //class DXCVLDSemaphore : public Semaphore {
    //
    //    DXCAutoFence _fence;
    //
    //    DXCVLDSemaphore(const sp<GraphicsDevice>& dev) : Semaphore(dev) {}
    //public:
    //    ~DXCVLDSemaphore() {}
    //
    //    static sp<Semaphore> Make(
    //        const sp<DXCDevice>& dev
    //    );
    //
    //    const DXCAutoFence& GetHandle() const { return _fence; }
    //};

}
