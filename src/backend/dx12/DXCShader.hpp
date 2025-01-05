#pragma once

//3rd-party headers

//veldrid public headers
#include "veldrid/common/RefCnt.hpp"

#include "veldrid/GraphicsDevice.hpp"
#include "veldrid/Pipeline.hpp"
#include "veldrid/Buffer.hpp"
#include "veldrid/Shader.hpp"

//standard library headers
#include <string>
#include <vector>

//backend specific headers

//platform specific headers
#include <d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers


namespace Veldrid::DXC::priv
{


} // namespace Veldrid::DXC



namespace Veldrid
{
    
    class DXCDevice;

    class DXCShader : public Shader{
        
        DXCDevice* _Dev() const {return reinterpret_cast<DXCDevice*>(dev.get());}

    private:
        //VkShaderModule _shaderModule;
        //std::string _name;

        //VkShaderModule ShaderModule => _shaderModule;
        std::vector<uint8_t> _bytes;

        DXCShader(
            const sp<DXCDevice> &dev,
            const Description& desc
        );
       

    public:
        //const VkShaderModule& GetHandle() const { return _shaderModule; }

        ~DXCShader() = default;

        static sp<Shader> Make(
            const sp<DXCDevice>& dev,
            const Shader::Description& desc,
            const std::span<std::uint8_t>& il
        );
        
        const void* GetData() const {return _bytes.data();}

        std::size_t GetDataSizeInBytes() const {return _bytes.size();}

    };

} // namespace Veldrid

