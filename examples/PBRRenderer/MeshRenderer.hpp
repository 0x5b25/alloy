#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <alloy/alloy.hpp>

#include "Scene.hpp"

struct Viewport {
    float fovDeg; //Y direction fov
    float aspectRatio; //width / height
    float nearClip, farClip;

    glm::vec3 position;
    glm::vec3 front, up, right;
};

class MeshRenderer {
    template<typename T> using alloy_sp = alloy::common::sp<T>;

    Scene& _scene; 

    alloy::PixelFormat _rtFormat;
    alloy::PixelFormat _dsFormat;
    alloy::SampleCount _msaaSampleCnt;

    alloy_sp<alloy::IGraphicsDevice> _dev;
    alloy_sp<alloy::IGfxPipeline> _objRenderPipeline;

    

    void _CreateObjRenderPipeline();

public:
    struct CreateArgs {
        alloy_sp<alloy::IGraphicsDevice> device;
        alloy::SampleCount msaaSampleCount;
        alloy::PixelFormat renderTargetFormat;
        alloy::PixelFormat depthStencilFormat;

        Scene& scene;
    };

    MeshRenderer(const CreateArgs& args);
    ~MeshRenderer();

    void DrawScene(alloy::IRenderCommandEncoder* rndPass, const Viewport& vp);
};
