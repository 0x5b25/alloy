#include "DXCShader.hpp"
#include "DXCDevice.hpp"

namespace alloy::dxc{
    DXCShader::DXCShader(
        const common::sp<DXCDevice> &dev,
        const Description& desc
    ) 
        : IShader(desc)
        , _dev(dev)
    {}
       

    common::sp<IShader> DXCShader::Make(
        const common::sp<DXCDevice> &dev,
        const IShader::Description &desc,
        const std::span<std::uint8_t> &il
    ) {
        std::vector<std::uint8_t> buffer(il.begin(), il.end());
        
        auto dxcShader = new DXCShader(dev, desc);
        dxcShader->_bytes = std::move(buffer);

        return common::sp(dxcShader);
    }
}
