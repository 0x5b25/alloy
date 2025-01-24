#pragma once

//3rd-party headers

//alloy public headers
#include "alloy/common/RefCnt.hpp"

#include "alloy/GraphicsDevice.hpp"
#include "alloy/Pipeline.hpp"
#include "alloy/Buffer.hpp"
#include "alloy/Shader.hpp"

//standard library headers
#include <string>
#include <vector>

//backend specific headers

//platform specific headers
#include <d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers


namespace alloy::DXC::priv
{


} // namespace alloy::DXC



namespace alloy::dxc
{
    
    class DXCDevice;

    class DXCShader : public IShader{
        
        common::sp<DXCDevice> _dev;

    private:
        //VkShaderModule _shaderModule;
        //std::string _name;

        //VkShaderModule ShaderModule => _shaderModule;
        std::vector<uint8_t> _bytes;

        DXCShader(
            const common::sp<DXCDevice> &dev,
            const Description& desc
        );
       

    public:
        //const VkShaderModule& GetHandle() const { return _shaderModule; }

        ~DXCShader() = default;

        static common::sp<IShader> Make(
            const common::sp<DXCDevice>& dev,
            const IShader::Description& desc,
            const std::span<std::uint8_t>& il
        );
        
        virtual const std::span<uint8_t> GetByteCode() override { return _bytes; }

    };

} // namespace alloy

