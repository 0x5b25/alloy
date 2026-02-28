#pragma once

#include <Metal/Metal.h>

namespace alloy::mtl {

    class MSAAResolveShader {
        id<MTLComputePipelineState> _pso;

    public:
        MSAAResolveShader(id<MTLDevice> device);
        ~MSAAResolveShader();

        void BindToCmdBuf(id<MTLComputeCommandEncoder> encoder, id<MTLTexture> src, id<MTLTexture> dst);
    };

    class MtlUtilShaderLib {

    public:
        MSAAResolveShader resolveShader;

        MtlUtilShaderLib(id<MTLDevice> device);
        ~MtlUtilShaderLib();

    };
}
