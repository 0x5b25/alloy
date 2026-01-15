#pragma once

#include "alloy/ExecuteIndirect.hpp"

#include <d3d12.h>

namespace alloy::dxc {

    class DXCDevice;
    class DXCResourceLayout;

    class DXCIndirectCommandLayout : public IIndirectCommandLayout {

        common::sp<DXCDevice> _dev;
        common::sp<DXCResourceLayout> _resLayout;

        ID3D12CommandSignature* _cmdSig;

    public:

        virtual ~DXCIndirectCommandLayout() override;

        static common::sp<IIndirectCommandLayout> Make(
            const common::sp<DXCDevice>& dev,
            const IIndirectCommandLayout::Description& desc
        );

        ID3D12CommandSignature* GetHandle() const {return _cmdSig;}

    };

}
