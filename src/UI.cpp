#define NOMINMAX 
#include "UI.h"
#include "Scene.h"
#include "Camera.h"
#include "MeshLoader.h"
#include <imgui/imgui.h>
#include <algorithm>

void UI::init(Scene* scene, Camera* camera)
{
    scene_ = scene;
    camera_ = camera;
}

void UI::draw(const ImGuiIO& io)
{
    // Reset per-frame signals
    wantSaveConfig = false;
    wantLoadConfig = false;
    wantRestart = false;

    drawObjectsPanel();
    drawControlsPanel(io);
}

void UI::drawObjectsPanel()
{
    const float PAD = 10.0f;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 winPos = vp->WorkPos;
    ImVec2 winSize = vp->WorkSize;

    ImGui::SetNextWindowPos({ winPos.x + PAD, winPos.y + PAD }, ImGuiCond_Always);
    float pw = std::min(320.0f, std::max(200.0f, winSize.x - 2.0f * PAD));
    float ph = std::max(120.0f, winSize.y - 2.0f * PAD);
    ImGui::SetNextWindowSize({ pw, ph }, ImGuiCond_Always);

    ImGui::Begin("Objects", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    ImGui::Button("Refresh");

    static const std::vector<std::string> empty;
    const auto& available = scene_->availableMeshNames
        ? *scene_->availableMeshNames
        : empty;

    if (available.empty()) {
        ImGui::Text("No objects found.");
    }
    else {
        ImGui::Text("Data folder: ./data");
        ImGui::Separator();
        for (int i = 0; i < (int)available.size(); ++i) {
            if (ImGui::Selectable(available[i].c_str(), selectedMeshIndex_ == i))
                selectedMeshIndex_ = i;
        }
        if (selectedMeshIndex_ >= 0 && selectedMeshIndex_ < (int)available.size()) {
            if (ImGui::Button("Add Selected")) {
                SceneObject obj;
                obj.meshFile = available[selectedMeshIndex_];
                obj.pos = scene_->emitter.origin;
                obj.pos.y = 0.0f;
                scene_->objects.push_back(obj);
                scene_->selectedObjectIndex = (int)scene_->objects.size() - 1;
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Scene Objects");
    auto& objects = scene_->objects;
    if (objects.empty()) {
        ImGui::Text("None");
    }
    else {
        for (int i = 0; i < (int)objects.size(); ++i) {
            std::string label = std::to_string(i) + ": " + objects[i].meshFile;
            if (ImGui::Selectable(label.c_str(), scene_->selectedObjectIndex == i))
                scene_->selectedObjectIndex = i;
        }

        int& sel = scene_->selectedObjectIndex;
        if (sel >= 0 && sel < (int)objects.size()) {
            SceneObject& obj = objects[sel];
            ImGui::Separator();
            ImGui::DragFloat3("Position", (float*)&obj.pos, 0.02f);
            ImGui::SliderFloat("Burnability", &obj.burnability, 0.0f, 1.0f);
            ImGui::SliderFloat("Fuel", &obj.fuel, 0.0f, obj.fuelMax);
            ImGui::SliderFloat("Fuel Max", &obj.fuelMax, 0.1f, 50.0f);
            if (obj.fuel > obj.fuelMax) obj.fuel = obj.fuelMax;
            ImGui::SliderFloat("Burn Rate", &obj.burnRate, 0.0f, 10.0f);
            ImGui::SliderFloat("Disturb Radius", &obj.disturbRadius, 0.0f, 10.0f);
            ImGui::SliderFloat("Disturb Strength", &obj.disturbStrength, 0.0f, 20.0f);
            if (ImGui::Button("Remove")) {
                objects.erase(objects.begin() + sel);
                if (sel >= (int)objects.size()) sel = (int)objects.size() - 1;
            }
        }
    }

    ImGui::End();
}


void UI::drawControlsPanel(const ImGuiIO& io)
{
    const float PAD = 10.0f;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 winPos = vp->WorkPos;
    ImVec2 winSize = vp->WorkSize;

    ImGui::SetNextWindowPos({ winPos.x + winSize.x - PAD, winPos.y + PAD },
        ImGuiCond_Always, { 1.0f, 0.0f });
    float pw = std::min(320.0f, std::max(200.0f, winSize.x - 2.0f * PAD));
    float ph = std::max(120.0f, winSize.y - 2.0f * PAD);
    ImGui::SetNextWindowSize({ pw, ph }, ImGuiCond_Always);

    ImGui::Begin("Ember Engine Controls", NULL,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    if (ImGui::CollapsingHeader("Global Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Wind", &scene_->enableWind);
        if (scene_->enableWind) {
            ImGui::Checkbox("Show Wind Dir", &scene_->showWind);
            ImGui::SliderFloat("Wind Strength", &scene_->windStrength, 0.0f, 10.0f);
            ImGui::SliderFloat3("Wind Dir", (float*)&scene_->globals.wind, -1.0f, 1.0f);
            if (ImGui::Button("Reset Wind"))  scene_->globals.wind = glm::vec3(0.0f);
            ImGui::Checkbox("Tornado Mode", &scene_->tornadoMode);
            if (scene_->tornadoMode) {
                ImGui::SliderFloat("Tornado Strength", &scene_->tornadoStrength, 0.0f, 20.0f);
                ImGui::SliderFloat("Tornado Radius", &scene_->tornadoRadius, 0.2f, 20.0f);
                ImGui::SliderFloat("Tornado Inflow", &scene_->tornadoInflow, 0.0f, 10.0f);
                ImGui::SliderFloat("Tornado Updraft", &scene_->tornadoUpdraft, 0.0f, 10.0f);
            }
        }
        ImGui::SliderFloat("Buoyancy", &scene_->globals.buoyancy, 0.0f, 5.0f);
        ImGui::SliderFloat("Cooling", &scene_->globals.cooling, 0.01f, 1.0f);
        ImGui::SliderFloat("Turbulence Amp", &scene_->globals.turbAmp, 0.0f, 2.0f);
        ImGui::SliderFloat("Turbulence Freq", &scene_->globals.turbFreq, 0.1f, 5.0f);
    }


    if (ImGui::CollapsingHeader("Emitter Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Radius", &scene_->emitter.radius, 0.01f, 1.0f);
        ImGui::SliderFloat("Base Size", &scene_->emitter.baseSize, 0.01f, 0.5f);
        ImGui::SliderFloat("Lifetime", &scene_->emitter.lifetimeBase, 0.1f, 5.0f);
        ImGui::SliderFloat("Speed Min", &scene_->emitter.initialSpeedMin, 0.0f, 5.0f);
        ImGui::SliderFloat("Speed Max", &scene_->emitter.initialSpeedMax, 0.0f, 5.0f);
    }


    ImGui::Separator();
    ImGui::Text("Presets");
    if (ImGui::Button("Lighter"))   scene_->applyPreset("Lighter");
    ImGui::SameLine();
    if (ImGui::Button("Campfire"))  scene_->applyPreset("Campfire");
    ImGui::SameLine();
    if (ImGui::Button("Wildfire"))  scene_->applyPreset("Wildfire");
    ImGui::SameLine();
    if (ImGui::Button("Iris Fire")) scene_->applyPreset("Iris Fire");


    if (ImGui::CollapsingHeader("Fuel", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Fuel", &scene_->fuelEnabled);
        ImGui::Checkbox("Infinite Fuel", &scene_->fuelInfinite);
        ImGui::Checkbox("Blow Away When Out", &scene_->fuelBlowAway);
        ImGui::SliderFloat("Fuel Max", &scene_->fuelMax, 1.0f, 200.0f);
        if (scene_->fuelMax < 1.0f) scene_->fuelMax = 1.0f;
        if (scene_->fuel > scene_->fuelMax) scene_->fuel = scene_->fuelMax;
        ImGui::SliderFloat("Fuel", &scene_->fuel, 0.0f, scene_->fuelMax);
        ImGui::SliderFloat("Burn Rate", &scene_->fuelBurnRate, 0.0f, 20.0f);
        ImGui::SliderFloat("Add Fuel Amount", &scene_->addFuelAmount, 0.0f, 50.0f);
        if (ImGui::Button("Add Fuel Now"))     scene_->addFuel();
        float frac = scene_->fuelEnabled
            ? (scene_->fuelMax > 0.0f ? scene_->fuel / scene_->fuelMax : 0.0f)
            : 1.0f;
        ImGui::ProgressBar(frac, ImVec2(-1.0f, 0.0f));
    }


    ImGui::Separator();
    if (ImGui::Button("Restart"))              wantRestart = true;
    ImGui::SameLine();
    if (ImGui::Button("Save Config (F5)"))     wantSaveConfig = true;
    ImGui::SameLine();
    if (ImGui::Button("Load Config (F9)"))     wantLoadConfig = true;

    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::End();
}