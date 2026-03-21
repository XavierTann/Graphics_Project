#pragma once
#include <imgui/imgui.h>
#include <string>


class Scene;
class Camera;

class UI {
public:

    void init(Scene* scene, Camera* camera);

    void draw(const ImGuiIO& io);

    bool wantSaveConfig = false;
    bool wantLoadConfig = false;
    bool wantRestart    = false;

private:
    Scene*  scene_  = nullptr;
    Camera* camera_ = nullptr;

    int selectedMeshIndex_ = -1;

    void drawObjectsPanel();
    void drawControlsPanel(const ImGuiIO& io);
};
