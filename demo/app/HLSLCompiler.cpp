#include "HLSLCompiler.hpp"

#include <stdexcept>
#include <format>
#include <vector>
#include <unordered_map>
#include <iostream>

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif

#include <dxcapi.h>

class HrException : public std::runtime_error
{
    const HRESULT m_hr;

    std::string HrToString(HRESULT hr)
    {
        return std::format("HRESULT of {:#X}", hr);
    }

public:
    HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
    HRESULT Error() const { return m_hr; }
};

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw HrException(hr);
    }
}

template<typename T>
class ComPtr {
    T* ptr = nullptr;

public:
    ComPtr() noexcept = default;
    
    ComPtr(T* p) noexcept : ptr(p) {}
    
    ~ComPtr() {
        if (ptr) ptr->Release();
    }
    
    // Move semantics
    ComPtr(ComPtr&& other) noexcept : ptr(other.ptr) {
        other.ptr = nullptr;
    }
    
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            if (ptr) ptr->Release();
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }
    
    // Delete copy operations
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    
    // Access operators
    T* operator->() const noexcept { return ptr; }
    T** operator&() noexcept { return &ptr; }
    T* Get() const noexcept { return ptr; }
    
    // Boolean operator for null check
    explicit operator bool() const noexcept { return ptr != nullptr; }
    
    // Release ownership
    T* Detach() noexcept {
        T* temp = ptr;
        ptr = nullptr;
        return temp;
    }
    
    // Reset pointer
    void Reset() noexcept {
        if (ptr) {
            ptr->Release();
            ptr = nullptr;
        }
    }
};

class DxcInstanceHelper {

    void* _dxcDylib;

    bool LoadDxcDylib() {
        #ifdef _WIN32
            _dxcDylib = LoadLibraryA("dxcompiler.dll");
        #elif __APPLE__
            _dxcDylib = dlopen("libdxcompiler.dylib", RTLD_LAZY);
        #else
            _dxcDylib = dlopen("libdxcompiler.so", RTLD_LAZY);
        #endif
        return _dxcDylib != nullptr;
    }

    void* LookupFunction(const char* name) {
        #ifdef _WIN32
            return (void*)GetProcAddress((HMODULE)_dxcDylib, name);
        #else
            return dlsym((void*)_dxcDylib, name);
        #endif
    }

    void UnloadDxcDylib() {
        #ifdef _WIN32
            FreeLibrary((HMODULE)_dxcDylib);
        #else
            dlclose(_dxcDylib);
        #endif
    }

public:

    DxcCreateInstanceProc pfnDxcCreateInstance;
    DxcCreateInstance2Proc pfnDxcCreateInstance2;

    DxcInstanceHelper()
        : _dxcDylib(nullptr)
        , pfnDxcCreateInstance(nullptr)
        , pfnDxcCreateInstance2(nullptr)
    {
        if(LoadDxcDylib()){
            pfnDxcCreateInstance = (DxcCreateInstanceProc)LookupFunction("DxcCreateInstance");
            pfnDxcCreateInstance2 = (DxcCreateInstance2Proc)LookupFunction("DxcCreateInstance2");
        } else {
            throw std::runtime_error("Failed to load dxcompiler");
        }
    }

    ~DxcInstanceHelper() {
        if(_dxcDylib)
            UnloadDxcDylib();
    }
};


class HLSLCompilerImpl : public IHLSLCompiler
{
private:
    DxcInstanceHelper* pInst;
    ComPtr<IDxcUtils> pUtils;
    ComPtr<IDxcCompiler3> pCompiler3;
    static const std::unordered_map<ShaderType, const wchar_t*> shaderTypeMap;
    static const std::unordered_map<ShaderModel, const wchar_t*> shaderModelMap;

public:
    HLSLCompilerImpl(DxcInstanceHelper* pInst)
        : pInst(pInst)
    {
        ThrowIfFailed(pInst->pfnDxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));
        ThrowIfFailed(pInst->pfnDxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler3)));
    }

    ~HLSLCompilerImpl()
    {
        pUtils.Reset();
        pCompiler3.Reset();
        delete pInst;
    }
    
    virtual std::vector<uint8_t> Compile(const std::string& source, 
                                       const std::wstring& entryPoint,
                                       ShaderType type,
                                       ShaderModel model,
                                       OptimizationLevel optLevel) override {
        std::vector<const wchar_t*> args;
        args.push_back(L"-E");
        args.push_back(entryPoint.c_str());
        args.push_back(L"-T");
        
        auto typeIt = shaderTypeMap.find(type);
        if (typeIt == shaderTypeMap.end()) {
            throw std::invalid_argument(std::format("Invalid shader type: {}", static_cast<int>(type)));
        }

        auto modelIt = shaderModelMap.find(model);
        if (modelIt == shaderModelMap.end()) {
            throw std::invalid_argument(std::format("Invalid shader model: {}", static_cast<int>(model)));
        }

        // Combine type and model into target string (e.g., "ps_6_0")
        std::wstring target = std::format(L"{}{}", typeIt->second, modelIt->second);
        args.push_back(target.c_str());

        // Add optimization level
        switch(optLevel) {
            case OptimizationLevel::Debug:
                args.push_back(L"-O0");
                args.push_back(L"-Zi");  // Add debug info
                args.push_back(L"-Qembed_debug"); //Embed debug info into bytecode
                break;
            case OptimizationLevel::Level1:
                args.push_back(L"-O1");
                break;
            case OptimizationLevel::Level2:
                args.push_back(L"-O2");
                break;
            case OptimizationLevel::Level3:
                args.push_back(L"-O3");
                break;
            default: throw std::invalid_argument(std::format("Invalid optimization level: {}", static_cast<int>(optLevel)));
        }

        DxcBuffer sourceBuffer { };
        sourceBuffer.Ptr = source.c_str();
        sourceBuffer.Size = source.size();// * sizeof(wchar_t);
        sourceBuffer.Encoding = DXC_CP_UTF8;

        ComPtr<IDxcResult> pResult;
        ThrowIfFailed(pCompiler3->Compile(&sourceBuffer, args.data(), args.size(), nullptr, IID_PPV_ARGS(&pResult)));

        // Check for compilation errors
        if (pResult->HasOutput(DXC_OUT_ERRORS)) {
            ComPtr<IDxcBlobUtf8> pErrors;
            ThrowIfFailed(pResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr));
            if (pErrors && pErrors->GetStringLength() > 0) {
                throw ShaderCompileError(L"", entryPoint, pErrors->GetStringPointer());
            }
        }

        HRESULT hrStatus;
        ThrowIfFailed(pResult->GetStatus(&hrStatus));
        if (FAILED(hrStatus)) {
            throw ShaderCompileError(L"", entryPoint, 
                std::format("Compilation failed with HRESULT {:#010x}", static_cast<unsigned int>(hrStatus)).c_str());
        }

        // Get compiled shader bytecode if available
        if (!pResult->HasOutput(DXC_OUT_OBJECT)) {
            return {};
        }

        ComPtr<IDxcBlob> pShader;
        ThrowIfFailed(pResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), nullptr));

        //Print disassemble results:
        if(true){
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

        // Copy bytecode to vector
        const uint8_t* byteData = reinterpret_cast<const uint8_t*>(pShader->GetBufferPointer());
        const size_t byteSize = pShader->GetBufferSize();
        return std::vector<uint8_t>(byteData, byteData + byteSize);
    }
};

const std::unordered_map<ShaderType, const wchar_t*> HLSLCompilerImpl::shaderTypeMap = {
    {ShaderType::VS, L"vs_"},
    {ShaderType::PS, L"ps_"},
    {ShaderType::CS, L"cs_"},
    {ShaderType::GS, L"gs_"},
    {ShaderType::HS, L"hs_"},
    {ShaderType::DS, L"ds_"},
    {ShaderType::LIB, L"lib_"}
};

const std::unordered_map<ShaderModel, const wchar_t*> HLSLCompilerImpl::shaderModelMap = {
    {ShaderModel::SM_6_0, L"6_0"},
    {ShaderModel::SM_6_1, L"6_1"},
    {ShaderModel::SM_6_2, L"6_2"},
    {ShaderModel::SM_6_3, L"6_3"},
    {ShaderModel::SM_6_4, L"6_4"},
    {ShaderModel::SM_6_5, L"6_5"},
    {ShaderModel::SM_6_6, L"6_6"},
    {ShaderModel::SM_6_7, L"6_7"}
};

IHLSLCompiler* IHLSLCompiler::Create() {
    try {
        DxcInstanceHelper* pInst = new DxcInstanceHelper();
        return new HLSLCompilerImpl(pInst);
    } catch(...) {
        return nullptr;
    }
}
