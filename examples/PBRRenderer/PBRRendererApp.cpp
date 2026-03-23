#include "PBRRendererApp.hpp"

#include <imgui.h>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include "MeshBuilder.hpp"


glm::vec3 Camera::GetFrontVector() const {
    float dy = sin(glm::radians(pitch));

    float dr = cos(glm::radians(pitch));

    float dx = sin(glm::radians(yaw));
    float dz = cos(glm::radians(yaw));

    return glm::vec3 {dr * dx, dy, dr * dz};
}

glm::vec3 Camera::GetRightVector() const  {

    float dx = cos(glm::radians(yaw));
    float dz = -sin(glm::radians(yaw));
    
    return glm::vec3 {dx, 0, dz};

    //glm::vec3 front = GetFrontVector();
    //glm::vec3 up {0, 1, 0};
    ////Using lefthand coordinate system 
    //auto right = glm::cross(up, front);
    //return glm::normalize(right);
}

Camera::operator Viewport() const {

    glm::vec3 front = GetFrontVector();
    glm::vec3 up {0, 1, 0};

    return Viewport{
        .fovDeg = fovY,
        .aspectRatio = (float)windowW / (float)windowH,
        .nearClip = 0.1, .farClip = 10.f,
        .position = position,
        .front = front,
        .up = up,
    };
}


PBRRendererApp::PBRRendererApp(IAppRunner* runner)
    : _runner(runner)
    , _rndr({
        .device = runner->GetRenderService()->GetDevice(),
        .msaaSampleCount = runner->GetRenderService()->GetFrameBufferSampleCount(),
        .renderTargetFormat = runner->GetRenderService()->GetFrameBufferColorFormat(),
        .depthStencilFormat = runner->GetRenderService()->GetFrameBufferDepthStencilFormat(),
    })
    , _scene(runner->GetRenderService()->GetDevice())
    , _cubeMesh(BuildBoxMesh(1,1,1))
    , _cam {
        .position = {0, 0, -4},
        .fovY = 45,
    }
{

}

void PBRRendererApp::_SetupScene() {
    auto boxMeshID = _scene.AddMesh(BuildBoxMesh(1,1,1));

    auto objID = _scene.CreateSceneObject();
    auto* obj = _scene.GetSceneObject(objID);
    obj->mesh.meshId = boxMeshID;
}

void PBRRendererApp::OnDrawGui() {        
    ImGui::ShowDemoWindow();
}

void PBRRendererApp::OnRenderFrame(alloy::IRenderCommandEncoder& renderPass) { 

    auto rndrSvc = _runner->GetRenderService();
    auto frameIdx = rndrSvc->GetCurrentFrameIndex();

    uint32_t windowW, windowH;
    rndrSvc->GetFrameBufferSize(windowW, windowH);

    //Viewport vp {
    //    .fovDeg = 45,
    //    .aspectRatio = (float)windowW / (float)windowH,
    //    .nearClip = 0.1, .farClip = 10.f,
    //    .position = {2,2,2},
    //    .front = glm::normalize(glm::vec3{-1, -1, -1}),
    //    .up = glm::vec3{0,1,0},
    //};

    _cam.windowW = (float)windowW;
    _cam.windowH = (float)windowH;

    _rndr.BeginRendering(&renderPass, _cam);
    _rndr.DrawMesh(_cubeMesh);
    _rndr.EndRendering();
}

void PBRRendererApp::Update() { 

}

void PBRRendererApp::FixedUpdate() {

    const auto& io = ImGui::GetIO();
    if(_mouseLocked || (!io.WantCaptureMouse && !io.WantCaptureKeyboard)) {
        _HandleInput();
    }
}


void PBRRendererApp::_HandleInput() {

    auto window = (GLFWwindow*)_runner->GetWindowHandle();
    auto deltaSec = _runner->GetTimeService()->GetFixedUpdateDeltaSeconds();

    //Only handle input when LMB is down
    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    if (state == GLFW_PRESS)
    {
        if(!_mouseLocked) {
            _runner->LockAndHideCursor();
            _mouseLocked = true;
            glfwGetCursorPos(window, &_cursorXPos, &_cursorYPos);
        }
    }
    else {
        if(_mouseLocked) {
            _runner->UnlockAndShowCursor();
            _mouseLocked = false;
        }
        return;
    }

    //Calculate mouse delta
    double cursorXPos, cursorYPos;
    glfwGetCursorPos(window, &cursorXPos, &cursorYPos);
    auto dx = cursorXPos - _cursorXPos; _cursorXPos = cursorXPos;
    auto dy = cursorYPos - _cursorYPos; _cursorYPos = cursorYPos;

    //Move and clamp camera orientation
    _cam.pitch -= dy * 0.1; // +Y: mouse move down
    if(_cam.pitch > 85) _cam.pitch = 85;
    if(_cam.pitch < -85) _cam.pitch = -85;

    _cam.yaw += dx * 0.1; // +X: mouse move right
    while(_cam.yaw > 180) _cam.yaw -= 360;
    while(_cam.yaw < -180) _cam.yaw += 360;

    //Viewport movement
    float moveSpeed = 1;
    glm::vec3 moveDir{};
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        //Move front
        moveDir += glm::vec3(0, 0, 1);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        //Move left
        moveDir += glm::vec3(-1, 0, 0);
    } 
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        //Move back
        moveDir += glm::vec3(0, 0, -1);
    } 
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        //Move right
        moveDir += glm::vec3(1, 0, 0);
    }

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        //Move up
        moveDir += glm::vec3(0, 1, 0);
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        //Move down
        moveDir += glm::vec3(0, -1, 0);
    }

    auto front = _cam.GetFrontVector();
    auto right = _cam.GetRightVector();
    auto up = glm::vec3(0, 1, 0);

    auto moveVec = front * moveDir.z + right * moveDir.x + up * moveDir.y;
    _cam.position +=  moveVec * moveSpeed * deltaSec;
}

