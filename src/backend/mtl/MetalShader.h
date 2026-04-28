#pragma once

#include "alloy/Shader.hpp"
#include "alloy/Pipeline.hpp"

#include <string>
#include <vector>
#include <span>

#import <Metal/Metal.h>

#include <metal_irconverter/metal_irconverter.h>

namespace alloy::mtl {

    class MetalDevice;

    class MetalShader : public alloy::IShader{

        common::sp<MetalDevice> _mtlDev;
        std::vector<uint8_t> _dxil;

        //id<MTLLibrary> _shaderLib;
        //id<MTLFunction> _shaderFn;


        MetalShader(common::sp<MetalDevice>&&, const alloy::IShader::Description&);
        virtual ~MetalShader() override;

    public:

    //static common::sp<MetalShader> Make(
    //    common::sp<MetalDevice>&&,
    //    const alloy::IShader::Description& desc,
    //    const std::string& src);

    static common::sp<MetalShader> MakeFromDXIL(
        common::sp<MetalDevice>&&,
        const alloy::IShader::Description& desc,
        const std::span<const std::uint8_t>& bin);


        //const id<MTLFunction> GetFuncHandle() const {return _shaderFn;}
        virtual const std::span<uint8_t> GetByteCode() override {
            return std::span<uint8_t>{_dxil};
        }
    };

    struct MetalVertexShaderStage {
        id<MTLLibrary> vertexLib;
        id<MTLLibrary> stageInLib;
    };

    MetalVertexShaderStage TranspileVertexShader(id<MTLDevice> dev,
                               const std::vector<VertexLayout>& vertexLayouts,
                                                 const IRRootSignature* rootSig,
                               const std::string& entryPoint,
                               std::span<uint8_t> dxil);

    struct MetalShaderStage {
        id<MTLLibrary> lib;

        union ReflectionData {
            //Directly copied from metal_irconverter.h

            // Fragment stage info
            struct FSInfo {
                int num_render_targets;
                uint8_t rt_index_int;
                bool discards;
            } fs;

            // Compute shader
            struct CSInfo {
                uint32_t tg_size[3];
            } cs;

            // Task / amplification shader
            struct ASInfo
            {
                uint32_t num_threads[3];
                uint32_t max_payload_size_in_bytes;
            } as;

            // Mesh shader
            struct MSInfo{
                uint32_t max_vertex_output_count;
                uint32_t max_primitive_output_count;
                IRMeshShaderPrimitiveTopology primitive_topology;
                uint32_t max_payload_size_in_bytes;
                uint32_t num_threads[3];
            } ms;

        } reflectionData;
    };

    MetalShaderStage TranspileDXILShader(id<MTLDevice> device,
                                       IShader::Stage stage,
                                       const IRRootSignature* rootSig,
                                       const std::string& entryPoint,
                                       const std::span<uint8_t>& dxil);

}
