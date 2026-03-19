#include "HLSLCompiler.hpp"

#include <stdexcept>
#include <format>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <memory>

#include <dxc_static.hpp>
//
//#ifdef _WIN32
//    #include <Windows.h>
//#else
//    #include <dlfcn.h>
//#endif
//
//#include <dxcapi.h>

class HLSLCompilerImpl : public IHLSLCompiler
{
private:
    IMachDxcCompiler* pCompiler;
    static const std::unordered_map<ShaderType, const char*> shaderTypeMap;
    static const std::unordered_map<ShaderModel, const char*> shaderModelMap;

public:
    HLSLCompilerImpl()
        : pCompiler(IMachDxcCompiler::Create())
    { }

    ~HLSLCompilerImpl()
    {
        delete pCompiler;
    }
    
    virtual std::vector<uint8_t> Compile(const std::string& source, 
                                         const std::string& entryPoint,
                                         const CompileOptions& options) override {
        std::vector<std::string> args;
        args.push_back("-E");
        args.push_back(entryPoint.c_str());
        args.push_back("-T");
        
        auto typeIt = shaderTypeMap.find(options.type);
        if (typeIt == shaderTypeMap.end()) {
            throw std::invalid_argument(std::format("Invalid shader type: {}", static_cast<int>(options.type)));
        }

        auto modelIt = shaderModelMap.find(options.model);
        if (modelIt == shaderModelMap.end()) {
            throw std::invalid_argument(std::format("Invalid shader model: {}", static_cast<int>(options.model)));
        }

        // Combine type and model into target string (e.g., "ps_6_0")
        std::string target = std::format("{}{}", typeIt->second, modelIt->second);
        args.push_back(target.c_str());

        // Add optimization level
        switch(options.optLevel) {
            case OptimizationLevel::Debug:
                args.push_back("-O0");
                
                break;
            case OptimizationLevel::Level1:
                args.push_back("-O1");
                break;
            case OptimizationLevel::Level2:
                args.push_back("-O2");
                break;
            case OptimizationLevel::Level3:
                args.push_back("-O3");
                break;
            default: throw std::invalid_argument(std::format("Invalid optimization level: {}", static_cast<int>(options.optLevel)));
        }

        if(options.keepDebugInfo) {
            args.push_back("-Zi");  // Add debug info
            args.push_back("-Qembed_debug"); //Embed debug info into bytecode
        }

        IMachDxcCompiler::CompileOptions compileOpts {};
        compileOpts.code = source;
        compileOpts.args = std::move(args);

        auto pResult = std::shared_ptr<IMachDxcCompileResults>(pCompiler->Compile(compileOpts));
        
        // Check for compilation errors
        {
            auto errMsg = pResult->GetErrorMessage();
            if (!errMsg.empty()) {
                throw ShaderCompileError("", entryPoint, errMsg);
            }
        }

        uint32_t hrStatus = pResult->GetHResult();
        if (int(hrStatus) < 0) {
            throw ShaderCompileError("", entryPoint, 
                std::format("Compilation failed with HRESULT {:#010x}", hrStatus).c_str());
        }

        // Get compiled shader bytecode if available
        auto dxilObj = pResult->GetOutput(IMachDxcCompileResults::OutputType::Objects);
        if (dxilObj.empty()) {
            return {};
        }

#if 0
        //Print disassemble results:
        {
            DxcBuffer dxilBuffer { };
            dxilBuffer.Ptr = pShader->GetBufferPointer();
            dxilBuffer.Size = pShader->GetBufferSize();
            dxilBuffer.Encoding = 0;

            ComPtr<IDxcResult> pDasmResult;
            ThrowIfFailed(pCompiler3->Disassemble(&dxilBuffer, IID_PPV_ARGS(&pDasmResult)));
        
            ComPtr<IDxcBlobUtf8> pDasm;
            ThrowIfFailed(pDasmResult->GetOutput(DXC_OUT_DISASSEMBLY, IID_PPV_ARGS(&pDasm), nullptr));

            std::cout << "Disassemble of shader code: \n" << pDasm->GetStringPointer() << std::endl;        
        }
#endif
        // Copy bytecode to vector
        return dxilObj;
    }
};

const std::unordered_map<ShaderType, const char*> HLSLCompilerImpl::shaderTypeMap = {
    {ShaderType::VS, "vs_"},
    {ShaderType::PS, "ps_"},
    {ShaderType::CS, "cs_"},
    {ShaderType::GS, "gs_"},
    {ShaderType::HS, "hs_"},
    {ShaderType::DS, "ds_"},
    {ShaderType::LIB, "lib_"}
};

const std::unordered_map<ShaderModel, const char*> HLSLCompilerImpl::shaderModelMap = {
    {ShaderModel::SM_6_0, "6_0"},
    {ShaderModel::SM_6_1, "6_1"},
    {ShaderModel::SM_6_2, "6_2"},
    {ShaderModel::SM_6_3, "6_3"},
    {ShaderModel::SM_6_4, "6_4"},
    {ShaderModel::SM_6_5, "6_5"},
    {ShaderModel::SM_6_6, "6_6"},
    {ShaderModel::SM_6_7, "6_7"}
};

IHLSLCompiler* IHLSLCompiler::Create() {
    try {
        return new HLSLCompilerImpl();
    } catch(...) {
        return nullptr;
    }
}
