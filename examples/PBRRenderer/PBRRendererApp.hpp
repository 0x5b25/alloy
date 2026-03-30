#pragma once

#include "IApp.hpp"
#include "MeshRenderer.hpp"
#include "Scene.hpp"

class MeshRenderer;

struct Camera {
    glm::vec3 position;
    float yaw, pitch; //degrees

    float fovY; //degrees
    float windowW, windowH;

    glm::vec3 GetFrontVector() const;
    glm::vec3 GetRightVector() const;

    operator Viewport() const;
};

class PBRRendererApp : public IApp {
    IAppRunner* _runner;

    Scene _scene;
    MeshRenderer _rndr;

    //Mesh _cubeMesh;

    Camera _cam;

    bool _mouseLocked = false;
    double _cursorXPos, _cursorYPos;

    void _HandleInput();

    void _SetupScene();

public:
    PBRRendererApp(IAppRunner* runner);

    virtual ~PBRRendererApp() override {}

    virtual int GetExitCode() override { return 0; }

    virtual void FixedUpdate() override;

    virtual void Update() override;

    virtual void OnDrawGui() override;
    virtual void OnRenderFrame(alloy::IRenderCommandEncoder& renderPass) override;

    virtual void OnFrameComplete(uint32_t frameIdx) {}
    virtual void OnFrameBegin(uint32_t frameIdx) {}

};