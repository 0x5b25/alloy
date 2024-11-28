#include "DXCShader.hpp"
#include "DXCDevice.hpp"

namespace Veldrid{
    DXCShader::DXCShader(
        const sp<DXCDevice> &dev,
        const Description& desc
    ) : Shader(dev, desc)
    {}
       

    sp<Shader> DXCShader::Make(
        const sp<DXCDevice> &dev,
        const Shader::Description &desc,
        const std::span<std::uint8_t> &il
    ) {
        std::vector<std::uint8_t> buffer(il.begin(), il.end());
        
        auto dxcShader = new DXCShader(dev, desc);
        dxcShader->_bytes = std::move(buffer);

        return sp(dxcShader);
    }
}
