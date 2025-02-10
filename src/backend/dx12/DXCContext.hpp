#pragma once



//3rd-party headers
#include <d3d12.h>
#include <D3D12MemAlloc.h>

//alloy public headers
#include "alloy/Context.hpp"

//standard library headers
#include <string>
#include <vector>
//backend specific headers

//platform specific headers
#include <dxgi1_4.h> //Guaranteed by DX12

//Local headers

namespace alloy::dxc {

    class DXCDevice;


    class DXCAdapter : public IPhysicalAdapter {
        IDXGIAdapter1* _adp;

        void PopulateAdpInfo();

    public:
        DXCAdapter(IDXGIAdapter1* adp)
            : _adp(adp)
        {
            PopulateAdpInfo();
        }

        virtual ~DXCAdapter() override {
            _adp->Release();
        }

        IDXGIAdapter1* GetHandle() const {return _adp;}
        virtual common::sp<IGraphicsDevice> RequestDevice(
            const IGraphicsDevice::Options& options) override;

    };

    class DXCContext : public IContext {
        IDXGIFactory4* _factory;
    public:
        DXCContext(IDXGIFactory4* factory);
        virtual ~DXCContext() override;

        static common::sp<DXCContext> Make();

        virtual common::sp<IGraphicsDevice> CreateDefaultDevice(const IGraphicsDevice::Options& options) override;
        virtual std::vector<common::sp<IPhysicalAdapter>> EnumerateAdapters() override;
    };
}
