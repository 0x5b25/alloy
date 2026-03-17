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
    std::string source;
    std::string entryPoint;
public:
    ShaderCompileError(const std::string& source, const std::string& entryPoint, const std::string& message)
        : std::runtime_error(message), source(source), entryPoint(entryPoint) {}

    const std::string& GetSource() const { return source; }
    const std::string& GetEntryPoint() const { return entryPoint; }
};

class IHLSLCompiler {
public:
    virtual ~IHLSLCompiler() {}

    struct CompileOptions {
        ShaderType type;
        ShaderModel model;
        OptimizationLevel optLevel;
        bool keepDebugInfo;
    };
    
    virtual std::vector<uint8_t> Compile(const std::string& source, 
                                       const std::string& entryPoint,
                                       const CompileOptions& options) = 0;

    static IHLSLCompiler* Create();
};
