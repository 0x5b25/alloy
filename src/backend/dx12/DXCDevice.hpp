#pragma once

//3rd-party headers

//veldrid public headers
#include "veldrid/common/Common.hpp"
#include "veldrid/GraphicsDevice.hpp"

//standard library headers
#include <string>
#include <mutex>
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

    class DXCDevice : public  GraphicsDevice {

    public:


    private:
        Microsoft::WRL::ComPtr<IDXGIAdapter1> _adp;
        Microsoft::WRL::ComPtr<ID3D12Device> _dev;


        DXCResourceFactory _resFactory;

        GraphicsApiVersion _apiVer;

        std::string _devName, _devVendor;
        std::string _drvName, _drvInfo;
        GraphicsDevice::Features _commonFeat;

        DXCDevice();

    public:

        ~DXCDevice();

        static sp<GraphicsDevice> Make(
            const GraphicsDevice::Options& options,
            SwapChainSource* swapChainSource = nullptr
        );

        virtual const std::string& DeviceName() const override { return _devName; }
        virtual const std::string& VendorName() const override { return ""; }
        virtual const GraphicsApiVersion ApiVersion() const override { return _apiVer; }
        virtual const GraphicsDevice::Features& GetFeatures() const override { return _commonFeat; }

        virtual ResourceFactory* GetResourceFactory() override { return &_resFactory; };

        virtual void SubmitCommand(
            const std::vector<SubmitBatch>& batch,
            Fence* fence) override;
        virtual SwapChain::State PresentToSwapChain(
            const std::vector<Semaphore*>& waitSemaphores,
            SwapChain* sc) override;
               
        virtual void WaitForIdle() override;

    };

}
