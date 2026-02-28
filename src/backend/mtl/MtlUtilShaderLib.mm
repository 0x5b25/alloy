#include "MtlUtilShaderLib.h"
#include <cstdint>
#include <string>
#include <stdexcept>
#include "shaders/MSAAResolve.h"

namespace alloy::mtl {


    MSAAResolveShader::MSAAResolveShader(id<MTLDevice> device) 
        : _pso(nil)
    {
        @autoreleasepool {
            auto shaderData = dispatch_data_create(MSAAResolve_contents, sizeof(MSAAResolve_contents), nil, nil);
            
            NSError* error = nullptr;
            auto shaderLib = [[device newLibraryWithData:shaderData error:&error] autorelease];
            if(error) {
                std::string errMsg = [error.localizedDescription UTF8String];
                [error release];

                throw std::runtime_error(errMsg);
            }

            auto shaderFn = [[shaderLib newFunctionWithName:@"averageResolveKernel"] autorelease];
            if(shaderFn == nil) {
                throw std::runtime_error("Function name lookup failed");
            }

            auto desc = [[MTLComputePipelineDescriptor new] autorelease];
            desc.label = @"MSAAResolvePSO";
            desc.computeFunction = shaderFn;

            MTLPipelineOption options {};
            auto pipeline = [device newComputePipelineStateWithDescriptor:desc
                                                                  options:options 
                                                               reflection:nil
                                                                    error:&error];
            if(error) {
                std::string errMsg = [error.localizedDescription UTF8String];
                [error release];

                throw std::runtime_error(errMsg);
            }

            _pso = pipeline;
        }
    }

    MSAAResolveShader::~MSAAResolveShader() {
        @autoreleasepool {
            if(_pso) [_pso release];
        }
    }


    void MSAAResolveShader::BindToCmdBuf(
        id<MTLComputeCommandEncoder> encoder, 
        id<MTLTexture> src,
        id<MTLTexture> dst
    ) {
        @autoreleasepool {

            // Calculate the maximum threads per threadgroup based on the thread execution width.
            auto w = _pso.threadExecutionWidth;
            auto h = _pso.maxTotalThreadsPerThreadgroup / w;
            auto wgSize = MTLSizeMake(w, h, 1);

            auto threadsPerGrid = MTLSizeMake(src.width, src.height, 1);

            auto _CastTexArrToSingleTexIfNeeded = [](id<MTLTexture> tex)->id<MTLTexture> {
                auto oldType = [tex textureType];
                bool isCastNeeded = true;
                auto newType = oldType;
                switch(oldType) {
                case MTLTextureType2DArray: newType = MTLTextureType2D; break;
                case MTLTextureType2DMultisampleArray: newType = MTLTextureType2DMultisample; break;
                default: isCastNeeded = false;
                }

                if(isCastNeeded) {
                    auto view = [tex newTextureViewWithPixelFormat:[tex pixelFormat] 
                                     textureType:newType 
                                          levels:NSMakeRange(0, 1)
                                          slices:NSMakeRange(0, 1)];
                    [view autorelease];
                    return view;
                }else {
                    return tex;
                }
            };

            [encoder setComputePipelineState:_pso];
            [encoder setTexture:_CastTexArrToSingleTexIfNeeded(src) atIndex:0];
            [encoder setTexture:_CastTexArrToSingleTexIfNeeded(dst) atIndex:1];
            [encoder dispatchThreads:threadsPerGrid threadsPerThreadgroup:wgSize];
        }
    }


    MtlUtilShaderLib::MtlUtilShaderLib(id<MTLDevice> device)
        : resolveShader(device)
    { }

    MtlUtilShaderLib::~MtlUtilShaderLib() { }

}
