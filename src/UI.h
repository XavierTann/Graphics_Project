#pragma once
#include <imgui/imgui.h>
#include <string>
#include <glm/glm.hpp>


class Scene;
class Camera;

class UI {
public:

    void init(Scene* scene, Camera* camera);

    void draw(const ImGuiIO& io);

    bool wantRestart    = false;


private:
    Scene*  scene_  = nullptr;
    Camera* camera_ = nullptr;

    int selectedMeshIndex_ = -1;

    void drawAxisLabels(const glm::mat4& viewProj, float winW, float winH);
    void drawObjectsPanel();
    void drawControlsPanel(const ImGuiIO& io);
};
