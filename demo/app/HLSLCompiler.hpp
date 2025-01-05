#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <format>

enum class ShaderType {
    VS,  // Vertex Shader
    PS,  // Pixel Shader
    CS,  // Compute Shader
    GS,  // Geometry Shader
    HS,  // Hull Shader
    DS,  // Domain Shader
    LIB  // Library
};

enum class ShaderModel {
    SM_6_0,
    SM_6_1,
    SM_6_2,
    SM_6_3,
    SM_6_4,
    SM_6_5,
    SM_6_6,
    SM_6_7
};

enum class OptimizationLevel {
    Debug,  // No optimizations, include debug info
    Level1, // Basic optimizations
    Level2, // Better optimizations
    Level3  // Aggressive optimizations
};

class ShaderCompileError : public std::runtime_error {
    std::wstring source;
    std::wstring entryPoint;
public:
    ShaderCompileError(const std::wstring& source, const std::wstring& entryPoint, const char* message)
        : std::runtime_error(message), source(source), entryPoint(entryPoint) {}

    const std::wstring& GetSource() const { return source; }
    const std::wstring& GetEntryPoint() const { return entryPoint; }
};

class IHLSLCompiler {
public:
    virtual ~IHLSLCompiler() {}
    
    virtual std::vector<uint8_t> Compile(const std::wstring& source, 
                                       const std::wstring& entryPoint,
                                       ShaderType type,
                                       ShaderModel model = ShaderModel::SM_6_0,
                                       OptimizationLevel optLevel = OptimizationLevel::Level3) = 0;

    static IHLSLCompiler* Create();
};
