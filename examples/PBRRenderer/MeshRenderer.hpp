#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <alloy/alloy.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec2 texCoord;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

// Assume triangle list topology
// CCW front facing
struct Mesh {
    std::vector<Vertex> vertices;
    //std::vector<uint32_t> indices;
};

struct Viewport {
    float fovDeg; //Y direction fov
    float aspectRatio; //width / height
    float nearClip, farClip;

    glm::vec3 position;
    glm::vec3 front, up, right;
};

class MeshRenderer {
    template<typename T> using alloy_sp = alloy::common::sp<T>;

    alloy::PixelFormat _rtFormat;
    alloy::PixelFormat _dsFormat;
    alloy::SampleCount _msaaSampleCnt;

    alloy_sp<alloy::IGraphicsDevice> _dev;
    alloy_sp<alloy::IGfxPipeline> _objRenderPipeline;
    alloy_sp<alloy::IResourceLayout> _objRenderLayout;

    alloy::IRenderCommandEncoder* _currRndPass;

    void _CreateObjRenderPipeline();

public:
    struct CreateArgs {
        alloy_sp<alloy::IGraphicsDevice> device;
        alloy::SampleCount msaaSampleCount;
        alloy::PixelFormat renderTargetFormat;
        alloy::PixelFormat depthStencilFormat;
    };

    MeshRenderer(const CreateArgs& args);
    ~MeshRenderer();

    void BeginRendering(alloy::IRenderCommandEncoder* rndPass, const Viewport& vp);
    void DrawMesh(const Mesh& mesh);
    void EndRendering();

};
