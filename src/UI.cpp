#define NOMINMAX 
#include "UI.h"
#include "Scene.h"
#include "Camera.h"
#include "MeshLoader.h"
#include <imgui/imgui.h>
#include <algorithm>

namespace {

void applyEmberTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(14.0f, 12.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.CellPadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;

    style.WindowRounding = 14.0f;
    style.ChildRounding = 12.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 10.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 10.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.96f, 0.91f, 0.84f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.65f, 0.58f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.06f, 0.05f, 0.97f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.09f, 0.07f, 0.92f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.07f, 0.06f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.33f, 0.21f, 0.16f, 0.90f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.12f, 0.09f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.31f, 0.17f, 0.12f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.42f, 0.22f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.19f, 0.10f, 0.07f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.30f, 0.13f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.12f, 0.07f, 0.05f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.16f, 0.10f, 0.08f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.09f, 0.07f, 0.06f, 0.75f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.37f, 0.22f, 0.15f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.54f, 0.31f, 0.19f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.67f, 0.37f, 0.18f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.98f, 0.68f, 0.30f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.92f, 0.51f, 0.20f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.66f, 0.24f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.34f, 0.18f, 0.11f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.50f, 0.25f, 0.13f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.64f, 0.29f, 0.12f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.25f, 0.14f, 0.10f, 0.95f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.41f, 0.22f, 0.13f, 0.95f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.55f, 0.27f, 0.13f, 0.95f);
    colors[ImGuiCol_Separator] = ImVec4(0.42f, 0.25f, 0.18f, 0.85f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.71f, 0.44f, 0.24f, 0.90f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.82f, 0.54f, 0.25f, 0.95f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.68f, 0.39f, 0.18f, 0.35f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.84f, 0.53f, 0.25f, 0.65f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.93f, 0.64f, 0.28f, 0.95f);
    colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.11f, 0.08f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.46f, 0.25f, 0.15f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.58f, 0.30f, 0.15f, 1.00f);
    colors[ImGuiCol_TabSelected] = colors[ImGuiCol_TabActive];
    colors[ImGuiCol_TabDimmed] = ImVec4(0.12f, 0.08f, 0.06f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.31f, 0.18f, 0.11f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.88f, 0.59f, 0.28f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.99f, 0.74f, 0.34f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.94f, 0.62f, 0.27f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.76f, 0.39f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.24f, 0.14f, 0.10f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.41f, 0.25f, 0.17f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.15f, 0.11f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.16f, 0.10f, 0.08f, 0.35f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.81f, 0.44f, 0.18f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.97f, 0.73f, 0.34f, 0.90f);
    colors[ImGuiCol_NavCursor] = ImVec4(0.96f, 0.67f, 0.28f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.73f, 0.35f, 0.70f);
}

void drawSectionHeader(const char* title, const char* subtitle = nullptr)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.99f, 0.72f, 0.32f, 1.0f));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    if (subtitle && subtitle[0] != '\0') {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", subtitle);
        ImGui::PopStyleColor();
    }
    ImGui::Separator();
}

void drawProgressRow(const char* label, float fraction, const char* overlay)
{
    ImGui::TextUnformatted(label);
    ImGui::ProgressBar(std::clamp(fraction, 0.0f, 1.0f), ImVec2(-1.0f, 8.0f), overlay);
}

float halfWidth()
{
    return (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
}

}

void UI::init(Scene* scene, Camera* camera)
{
    scene_ = scene;
    camera_ = camera;
    applyEmberTheme();
}

void UI::draw(const ImGuiIO& io)
{
    wantRestart = false;

    const glm::mat4& vp = camera_->getViewProj();
    drawAxisLabels(vp, io.DisplaySize.x, io.DisplaySize.y);

    drawObjectsPanel();
    drawControlsPanel(io);
}
void UI::drawAxisLabels(const glm::mat4& viewProj, float winW, float winH)
{
    // Project a world position to screen
    auto project = [&](glm::vec3 wp) -> ImVec2 {
        glm::vec4 c = viewProj * glm::vec4(wp, 1.0f);
        if (c.w <= 0.0f) return { -9999, -9999 };
        glm::vec3 n = glm::vec3(c) / c.w;
        return { (n.x * 0.5f + 0.5f) * winW,
                 (1.0f - (n.y * 0.5f + 0.5f)) * winH };
        };

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // +X label (D key — red)
    ImVec2 px = project({ 2.7f, 0.01f, 0.0f });
    dl->AddText(px, IM_COL32(220, 60, 60, 220), "+X  (D)");

    // +Z label (S key — blue)
    ImVec2 pz = project({ 0.0f, 0.01f, 2.7f });
    dl->AddText(pz, IM_COL32(70, 130, 220, 220), "+Z  (S)");
}


void UI::drawObjectsPanel()
{
    const float PAD = 10.0f;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 winPos = vp->WorkPos;
    ImVec2 winSize = vp->WorkSize;

    ImGui::SetNextWindowPos({ winPos.x + PAD, winPos.y + PAD }, ImGuiCond_Always);
    float pw = std::min(340.0f, std::max(260.0f, winSize.x * 0.24f));
    float ph = std::max(120.0f, winSize.y - 2.0f * PAD);
    ImGui::SetNextWindowSize({ pw, ph }, ImGuiCond_Always);
    ImGui::Begin("Scene & Assets", NULL,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    static const std::vector<std::string> empty;
    const auto& available = scene_->availableMeshNames
        ? *scene_->availableMeshNames : empty;
    auto& objects = scene_->objects;
    int& sel = scene_->selectedObjectIndex;
    drawSectionHeader("Asset library");

    if (ImGui::BeginChild("asset_lib", ImVec2(0.0f, 100.0f), true)) {
        if (available.empty()) {
            ImGui::TextDisabled("No .glb files found in /data");
        }
        else {
            for (int i = 0; i < (int)available.size(); ++i) {
                if (ImGui::Selectable(available[i].c_str(), selectedMeshIndex_ == i))
                    selectedMeshIndex_ = i;
            }
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();
    bool canAdd = selectedMeshIndex_ >= 0
        && selectedMeshIndex_ < (int)available.size();
    if (!canAdd) ImGui::BeginDisabled();
    if (ImGui::Button("Add to scene", ImVec2(-1.0f, 0.0f))) {
        SceneObject obj;
        obj.meshFile = available[selectedMeshIndex_];
        obj.pos = scene_->emitter.origin;
        obj.pos.y = 0.0f;
        objects.push_back(obj);
        sel = (int)objects.size() - 1;
    }
    if (!canAdd) ImGui::EndDisabled();
    ImGui::Spacing();
    drawSectionHeader("Placed objects");

    if (ImGui::BeginChild("placed_objs", ImVec2(0.0f, 120.0f), true)) {
        if (objects.empty()) {
            ImGui::TextDisabled("No objects in scene");
        }
        else {
            for (int i = 0; i < (int)objects.size(); ++i) {
                bool isSel = (sel == i);
                std::string rowLabel = std::to_string(i + 1)
                    + ".  " + objects[i].meshFile + "##r" + std::to_string(i);
                float removeW = ImGui::CalcTextSize("x").x
                    + ImGui::GetStyle().FramePadding.x * 2.0f + 4.0f;
                float rowW = ImGui::GetContentRegionAvail().x - removeW
                    - ImGui::GetStyle().ItemSpacing.x;

                if (ImGui::Selectable(rowLabel.c_str(), isSel,
                    ImGuiSelectableFlags_None, ImVec2(rowW, 0.0f)))
                    sel = i;
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button,
                    ImVec4(0.45f, 0.12f, 0.08f, 0.70f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                    ImVec4(0.70f, 0.15f, 0.10f, 1.00f));
                std::string xId = "x##x" + std::to_string(i);
                if (ImGui::SmallButton(xId.c_str())) {
                    objects.erase(objects.begin() + i);
                    if (sel >= (int)objects.size())
                        sel = (int)objects.size() - 1;
                    ImGui::PopStyleColor(2);
                    break;
                }
                ImGui::PopStyleColor(2);
            }
        }
    }
    ImGui::EndChild();

    if (sel >= 0 && sel < (int)objects.size()) {
        ImGui::Spacing();
        const auto& p = objects[sel].pos;
        ImGui::TextDisabled("%s  —  %.2f  %.2f  %.2f",
            objects[sel].meshFile.c_str(), p.x, p.y, p.z);
        ImGui::TextDisabled("LMB drag or WASD to move");
    }
    ImGui::Spacing();
    drawSectionHeader("Controls");

    auto key = [](const char* label, bool active) {
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button,
                ImVec4(0.92f, 0.51f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.05f, 0.03f, 0.02f, 1.0f));
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Button,
                ImVec4(0.18f, 0.11f, 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImGui::GetStyleColorVec4(ImGuiCol_Text));
        }
        ImGui::SmallButton(label);
        ImGui::PopStyleColor(2);
        };

    bool lmb = ImGui::IsMouseDown(0);
    bool mmb = ImGui::IsMouseDown(2);
    bool wKey = ImGui::IsKeyDown(ImGuiKey_W);
    bool aKey = ImGui::IsKeyDown(ImGuiKey_A);
    bool sKey = ImGui::IsKeyDown(ImGuiKey_S);
    bool dKey = ImGui::IsKeyDown(ImGuiKey_D);
    bool rKey = ImGui::IsKeyDown(ImGuiKey_R);
    bool f5 = ImGui::IsKeyDown(ImGuiKey_F5);
    bool f9 = ImGui::IsKeyDown(ImGuiKey_F9);

    if (ImGui::BeginChild("controls_ref", ImVec2(0.0f, 0.0f), true)) {

        ImGui::TextDisabled("camera");
        key("Left Mouse", lmb); ImGui::SameLine();
        ImGui::TextUnformatted("Drag obj / orbit");
        key("Middle Mouse", mmb); ImGui::SameLine();
        ImGui::TextUnformatted("Pan camera");
        ImGui::Spacing();
        bool hasObj = sel >= 0 && sel < (int)objects.size();
        ImGui::TextDisabled("selected object");
        if (!hasObj) ImGui::BeginDisabled();
        key("W", wKey); ImGui::SameLine();
        key("A", aKey); ImGui::SameLine();
        key("S", sKey); ImGui::SameLine();
        key("D", dKey); ImGui::SameLine();
        ImGui::TextUnformatted("Move XZ");
        if (!hasObj) ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::TextDisabled("scene");
        key("R", rKey); ImGui::SameLine();
        ImGui::TextUnformatted("Restart");
    }
    ImGui::EndChild();

    ImGui::End();
}




void UI::drawControlsPanel(const ImGuiIO& io)
{
    const float PAD = 10.0f;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 winPos = vp->WorkPos;
    ImVec2 winSize = vp->WorkSize;

    ImGui::SetNextWindowPos(
        { winPos.x + winSize.x - PAD, winPos.y + PAD },
        ImGuiCond_Always, { 1.0f, 0.0f });
    float pw = std::min(380.0f, std::max(260.0f, winSize.x * 0.28f));
    float ph = std::max(120.0f, winSize.y - 2.0f * PAD);
    ImGui::SetNextWindowSize({ pw, ph }, ImGuiCond_Always);

    ImGui::Begin("Control Panel", NULL,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    drawSectionHeader("Status");
    ImGui::Text("%.0f FPS", io.Framerate);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextDisabled(scene_->smokeEnabled ? "Smoke on" : "Smoke off");
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextDisabled(scene_->fuelInfinite ? "Infinite fuel"
        : (scene_->fuelEnabled ? "Fuel active" : "No fuel"));
    float fuelFrac = scene_->fuelMax > 0.0f ? scene_->fuel / scene_->fuelMax : 0.0f;
    drawProgressRow("Fuel Reserve", scene_->fuelEnabled ? fuelFrac : 1.0f, scene_->fuelEnabled ? "" : "Disabled");

    ImGui::Spacing();
    drawSectionHeader("Presets");

    float bw = (ImGui::GetContentRegionAvail().x
        - ImGui::GetStyle().ItemSpacing.x * 3.0f) / 4.0f;
    if (ImGui::Button("Lighter", ImVec2(bw, 0.0f))) scene_->applyPreset("Lighter");
    ImGui::SameLine();
    if (ImGui::Button("Campfire", ImVec2(bw, 0.0f))) scene_->applyPreset("Campfire");
    ImGui::SameLine();
    if (ImGui::Button("Wildfire", ImVec2(bw, 0.0f))) scene_->applyPreset("Wildfire");
    ImGui::Spacing();
    if (ImGui::Button("Restart simulation", ImVec2(-1.0f, 0.0f)))
        wantRestart = true;

    ImGui::Spacing();
    if (!ImGui::BeginTabBar("ember_tabs")) { ImGui::End(); return; }
    if (ImGui::BeginTabItem("Fire")) {
        if (ImGui::BeginChild("tab_fire", ImVec2(0.0f, 0.0f), true)) {

            ImGui::TextDisabled("behaviour");
            ImGui::SliderFloat("Buoyancy", &scene_->globals.buoyancy, 0.0f, 5.0f);
            ImGui::SliderFloat("Cooling", &scene_->globals.cooling, 0.01f, 1.0f);

            ImGui::Spacing();
            ImGui::TextDisabled("turbulence");
            ImGui::SliderFloat("Amplitude", &scene_->globals.turbAmp, 0.0f, 2.0f);
            ImGui::SliderFloat("Frequency", &scene_->globals.turbFreq, 0.1f, 5.0f);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Checkbox("Smoke trail", &scene_->smokeEnabled);

            ImGui::Checkbox("Infinite fuel", &scene_->fuelInfinite);
            if (scene_->fuelInfinite) scene_->fuelEnabled = true;

            if (!scene_->fuelInfinite) {
                ImGui::Spacing();
                ImGui::TextDisabled("capacity");
                ImGui::SliderFloat("Max fuel", &scene_->fuelMax, 1.0f, 200.0f);
                if (scene_->fuelMax < 1.0f) scene_->fuelMax = 1.0f;
                if (scene_->fuel > scene_->fuelMax) scene_->fuel = scene_->fuelMax;

                ImGui::SliderFloat("Burn rate", &scene_->fuelBurnRate, 0.0f, 20.0f);

                ImGui::Spacing();
                ImGui::TextDisabled("top-up");
                ImGui::SliderFloat("Add amount", &scene_->addFuelAmount, 0.0f, 50.0f);
                if (ImGui::Button("Add fuel now", ImVec2(-1.0f, 0.0f)))
                    scene_->addFuel();
                ImGui::Spacing();
                float pct = scene_->fuelMax > 0.0f
                    ? (scene_->fuel / scene_->fuelMax) * 100.0f : 0.0f;
                ImGui::TextDisabled("Remaining: %.0f%%", pct);
            }
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Particles")) {
        if (ImGui::BeginChild("tab_particles", ImVec2(0.0f, 0.0f), true)) {

            ImGui::TextDisabled("shape");
            ImGui::SliderFloat("Radius", &scene_->emitter.radius, 0.01f, 1.0f);
            ImGui::SliderFloat("Base size", &scene_->emitter.baseSize, 0.01f, 0.5f);

            ImGui::Spacing();
            ImGui::TextDisabled("launch");
            ImGui::SliderFloat("Lifetime", &scene_->emitter.lifetimeBase, 0.1f, 5.0f);
            ImGui::SliderFloat("Min speed", &scene_->emitter.initialSpeedMin, 0.0f, 5.0f);
            ImGui::SliderFloat("Max speed", &scene_->emitter.initialSpeedMax, 0.0f, 5.0f);
            scene_->emitter.initialSpeedMin = std::min(scene_->emitter.initialSpeedMin, scene_->emitter.initialSpeedMax);
            scene_->emitter.initialSpeedMax = std::max(scene_->emitter.initialSpeedMax, scene_->emitter.initialSpeedMin);
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Wind")) {
        if (ImGui::BeginChild("tab_wind", ImVec2(0.0f, 0.0f), true)) {

            ImGui::Checkbox("Enable wind", &scene_->enableWind);

            if (scene_->enableWind) {
                ImGui::Spacing();
                ImGui::Checkbox("Show arrow", &scene_->showWind);
                ImGui::SliderFloat("Strength", &scene_->windStrength, 0.0f, 10.0f);
                ImGui::SliderFloat3("Direction",
                    (float*)&scene_->globals.wind, -1.0f, 1.0f);
                if (ImGui::Button("Reset direction", ImVec2(-1.0f, 0.0f)))
                    scene_->globals.wind = glm::vec3(0.0f);

                ImGui::Spacing();
                ImGui::Checkbox("Tornado mode", &scene_->tornadoMode);
                if (scene_->tornadoMode) {
                    ImGui::SliderFloat("Swirl", &scene_->tornadoStrength, 0.0f, 20.0f);
                    ImGui::SliderFloat("Radius", &scene_->tornadoRadius, 0.2f, 20.0f);
                    ImGui::SliderFloat("Inflow", &scene_->tornadoInflow, 0.0f, 10.0f);
                    ImGui::SliderFloat("Updraft", &scene_->tornadoUpdraft, 0.0f, 10.0f);
                }
            }
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Lighting")) {
        if (ImGui::BeginChild("tab_lighting", ImVec2(0.0f, 0.0f), true)) {
            ImGui::TextDisabled("fire + smoke");
            ImGui::ColorEdit3("Fire/Smoke color", (float*)&scene_->fireLightColor);
            ImGui::SliderFloat("Intensity", &scene_->fireLightIntensity, 0.0f, 8.0f);
            ImGui::SliderFloat("Range", &scene_->fireLightRange, 0.5f, 8.0f);
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
    ImGui::End();
}
