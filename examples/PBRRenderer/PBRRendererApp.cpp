#include "PBRRendererApp.hpp"

#include <format>

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
    , _scene(runner->GetRenderService()->GetDevice())
    , _rndr({
        .device = runner->GetRenderService()->GetDevice(),
        .msaaSampleCount = runner->GetRenderService()->GetFrameBufferSampleCount(),
        .renderTargetFormat = runner->GetRenderService()->GetFrameBufferColorFormat(),
        .depthStencilFormat = runner->GetRenderService()->GetFrameBufferDepthStencilFormat(),

        .scene = _scene
    })
    , _cam {
        .position = {0, 0, -4},
        .fovY = 45,
    }
{
    _SetupScene();
}

void PBRRendererApp::_SetupScene() {
    auto boxMeshID = _scene.AddMesh(BuildBoxMesh(1,1,1));

    {
        auto objID = _scene.CreateSceneObject();
        auto* mesh = _scene.GetMeshComponent(objID);
        mesh->meshId = boxMeshID;
        mesh->color = {0.5,0.7,0.6,1};
        mesh->roughness = 0.5;
        mesh->metallic = 0.1;
    }

    {
        auto objID = _scene.CreateSceneObject();
        _scene.GetTransformComponent(objID)->position = {1.5,0,0};
        auto* mesh = _scene.GetMeshComponent(objID);
        mesh->color = {0.3,0.2,0.7,1};
        mesh->meshId = boxMeshID;
        mesh->roughness = 0.3;
        mesh->metallic = 0.9;
    }

}

void PBRRendererApp::OnDrawGui() {        
    ImGui::Begin("Scene Objects");

    int objIndex = 0;
    for (auto& obj : _scene) {
        ImGui::PushID(objIndex);

        auto label = std::format("Object {}", obj.id);
        if (ImGui::CollapsingHeader(label.c_str())) {
            // Transform
            if (ImGui::TreeNode("Transform")) {
                const auto* t = _scene.GetTransformComponent(obj.id);
                glm::vec3 position = t->position;
                glm::vec3 euler = glm::degrees(t->GetEulerAngles());
                glm::vec3 scale = t->scale;

                ImGui::DragFloat3("Position", &position.x, 0.1f);
                ImGui::DragFloat3("Rotation (deg)", &euler.x, 1.0f);
                ImGui::DragFloat3("Scale", &scale.x, 0.1f);

                if (position != t->position || euler != glm::degrees(t->GetEulerAngles()) || scale != t->scale) {
                    auto* tWritable = _scene.GetTransformComponent(obj.id);
                    tWritable->position = position;
                    tWritable->SetEulerAngles(glm::radians(euler));
                    tWritable->scale = scale;
                }
                ImGui::TreePop();
            }

            // Mesh Component
            if (ImGui::TreeNode("Mesh")) {
                const auto* m = _scene.GetMeshComponent(obj.id);
                if (m->meshId != INVALID_ID) {
                    ImGui::Text("Mesh ID: %u", m->meshId);
                    const Mesh* mesh = _scene.GetMesh(m->meshId);
                    if (mesh) {
                        ImGui::Text("Vertices: %zu", mesh->vertices.size());
                    }
                } else {
                    ImGui::TextDisabled("No mesh assigned");
                }

                glm::vec4 color = m->color;
                float roughness = m->roughness;
                float metallic = m->metallic;

                ImGui::ColorEdit4("Color", &color.x);
                ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f);
                ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f);

                if (color != m->color || roughness != m->roughness || metallic != m->metallic) {
                    auto* mWritable = _scene.GetMeshComponent(obj.id);
                    mWritable->color = color;
                    mWritable->roughness = roughness;
                    mWritable->metallic = metallic;
                }
                ImGui::TreePop();
            }
        }

        ImGui::PopID();
        ++objIndex;
    }

    if (objIndex == 0) {
        ImGui::TextDisabled("No scene objects");
    }

    ImGui::End();
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

    _rndr.DrawScene(&renderPass, _cam);
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

