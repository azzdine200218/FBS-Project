#include "Menu.hpp"
#include "ConfigSystem.hpp"
#include "../include/imgui/imgui.h"
#include "../include/imgui/imgui_internal.h"
#include "../include/XorStr.hpp"
#include <cmath>
#include <chrono>

static float GetTime() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(now - start).count();
}

static ImColor GetAccentPulse(float t) {
    float pulse = (sinf(t * 2.0f) + 1.0f) * 0.5f;
    int r = (int)(100 + pulse * 30);
    int g = (int)(120 + pulse * 40);
    int b = (int)(220 + pulse * 35);
    return ImColor(r, g, b, 255);
}

static void DrawGradientRect(ImDrawList* drawList, ImVec2 p1, ImVec2 p2, ImU32 col_top, ImU32 col_bot) {
    drawList->AddRectFilledMultiColor(p1, p2, col_top, col_top, col_bot, col_bot);
}

static bool ToggleSwitch(const char* label, bool* v) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    float height = ImGui::GetFrameHeight() * 0.8f;
    float width = height * 1.8f;
    const ImRect total_bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + width + style.ItemInnerSpacing.x + label_size.x, window->DC.CursorPos.y + height));
    
    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed) *v = !*v;

    float radius = height * 0.5f;
    float anim_t = *v ? 1.0f : 0.0f;

    ImU32 bg_color = *v ? IM_COL32(100, 130, 220, 255) : IM_COL32(50, 50, 55, 255);
    if (hovered) bg_color = *v ? IM_COL32(120, 150, 235, 255) : IM_COL32(65, 65, 70, 255);

    ImVec2 p_min = total_bb.Min;
    ImDrawList* draw_list = window->DrawList;
    draw_list->AddRectFilled(p_min, ImVec2(p_min.x + width, p_min.y + height), bg_color, radius);

    float knob_x = p_min.x + radius + anim_t * (width - height);
    float knob_y = p_min.y + radius;
    draw_list->AddCircleFilled(ImVec2(knob_x, knob_y), radius - 2.0f, IM_COL32(255, 255, 255, 240));

    ImGui::RenderText(ImVec2(p_min.x + width + style.ItemInnerSpacing.x, p_min.y + (height - label_size.y) * 0.5f), label);

    return pressed;
}

void Menu::Draw(Overlay& overlay) {
    if (!overlay.showMenu) return;

    float time = GetTime();
    ImColor accent = GetAccentPulse(time);
    ImVec4 accentVec = (ImVec4)accent;

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(0, 0);
    style.FramePadding = ImVec2(8, 5);
    style.ItemSpacing = ImVec2(10, 8);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImColor(12, 12, 16, 250);
    colors[ImGuiCol_ChildBg] = ImColor(18, 18, 24, 255);
    colors[ImGuiCol_Border] = ImColor(40, 42, 54, 200);
    colors[ImGuiCol_FrameBg] = ImColor(30, 30, 38, 255);
    colors[ImGuiCol_FrameBgHovered] = ImColor(40, 40, 50, 255);
    colors[ImGuiCol_FrameBgActive] = ImColor(50, 50, 62, 255);
    colors[ImGuiCol_TitleBg] = ImColor(12, 12, 16, 255);
    colors[ImGuiCol_TitleBgActive] = ImColor(12, 12, 16, 255);

    colors[ImGuiCol_CheckMark] = accentVec;
    colors[ImGuiCol_SliderGrab] = accentVec;
    colors[ImGuiCol_SliderGrabActive] = ImColor(140, 160, 240, 255);
    colors[ImGuiCol_Button] = ImColor(30, 30, 38, 255);
    colors[ImGuiCol_ButtonHovered] = ImColor(45, 45, 55, 255);
    colors[ImGuiCol_ButtonActive] = ImColor(60, 60, 72, 255);
    colors[ImGuiCol_Header] = ImColor(30, 30, 38, 255);
    colors[ImGuiCol_HeaderHovered] = ImColor(40, 42, 54, 255);
    colors[ImGuiCol_HeaderActive] = ImColor(50, 52, 66, 255);
    colors[ImGuiCol_Separator] = ImColor(40, 42, 54, 150);
    colors[ImGuiCol_Text] = ImColor(230, 232, 240, 255);
    colors[ImGuiCol_TextDisabled] = ImColor(120, 122, 140, 255);
    colors[ImGuiCol_ScrollbarBg] = ImColor(18, 18, 24, 255);
    colors[ImGuiCol_ScrollbarGrab] = ImColor(50, 52, 66, 255);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(65, 68, 82, 255);

    ImGui::SetNextWindowSize(ImVec2(750, 520), ImGuiCond_FirstUseEver);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin("##FBS", &overlay.showMenu, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar)) {

        ImDrawList* windowDraw = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();

        // Top bar gradient
        DrawGradientRect(windowDraw,
            windowPos,
            ImVec2(windowPos.x + windowSize.x, windowPos.y + 50),
            IM_COL32(20, 20, 28, 255),
            IM_COL32(12, 12, 16, 255));

        // Accent line under header
        windowDraw->AddRectFilled(
            ImVec2(windowPos.x, windowPos.y + 48),
            ImVec2(windowPos.x + windowSize.x, windowPos.y + 50),
            (ImU32)accent);

        // Title
        ImGui::SetCursorPos(ImVec2(20, 12));
        ImGui::PushFont(nullptr);
        ImGui::TextColored(accentVec, "FBS");
        ImGui::SameLine();
        ImGui::TextColored(ImColor(200, 202, 210, 255), "PREMIUM");
        ImGui::SameLine(windowSize.x - 160);
        float dot_pulse = (sinf(time * 3.0f) + 1.0f) * 0.5f;
        windowDraw->AddCircleFilled(
            ImVec2(windowPos.x + windowSize.x - 165, windowPos.y + 25),
            4.0f, ImColor(50, (int)(200 + dot_pulse * 55), 80, 255));
        ImGui::TextColored(ImColor(100, 255, 130, 255), " Connected");
        ImGui::PopFont();

        // Sidebar
        ImGui::SetCursorPos(ImVec2(0, 52));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.055f, 0.075f, 1.0f));
        ImGui::BeginChild("##Sidebar", ImVec2(170, 0), false);

        ImGui::Spacing();
        ImGui::SetCursorPosX(15);
        ImGui::TextColored(ImColor(90, 92, 110, 255), "NAVIGATION");
        ImGui::Spacing();
        ImGui::Spacing();

        struct TabInfo { const char* icon; const char* label; };
        TabInfo tabs[] = {
            { "[A]", "Combat" },
            { "[V]", "Visuals" },
            { "[M]", "Movement" },
            { "[S]", "Settings" }
        };

        for (int i = 0; i < 4; i++) {
            bool selected = (currentTab == i);
            ImVec2 cursor = ImGui::GetCursorScreenPos();

            if (selected) {
                windowDraw->AddRectFilled(
                    ImVec2(cursor.x, cursor.y),
                    ImVec2(cursor.x + 170, cursor.y + 38),
                    IM_COL32(100, 130, 220, 25));
                windowDraw->AddRectFilled(
                    ImVec2(cursor.x, cursor.y + 2),
                    ImVec2(cursor.x + 3, cursor.y + 36),
                    (ImU32)accent);
            }

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.04f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.08f));

            char tabLabel[64];
            snprintf(tabLabel, sizeof(tabLabel), "  %s  %s##tab%d", tabs[i].icon, tabs[i].label, i);

            if (ImGui::Button(tabLabel, ImVec2(170, 38))) {
                currentTab = i;
            }

            ImGui::PopStyleColor(3);
        }

        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 50);
        ImGui::SetCursorPosX(15);
        ImGui::TextColored(ImColor(60, 62, 78, 255), "v5.2 Ghost");
        ImGui::SetCursorPosX(15);
        ImGui::TextColored(ImColor(45, 47, 58, 255), "FBS Project");

        ImGui::EndChild();
        ImGui::PopStyleColor();

        // Content area
        ImGui::SameLine();
        ImGui::SetCursorPos(ImVec2(172, 52));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(25, 20));
        ImGui::BeginChild("##Content", ImVec2(0, 0), false);

        if (currentTab == 0) {
            // === COMBAT TAB ===
            ImGui::TextColored(accentVec, "AIMBOT");
            ImGui::Spacing();
            ToggleSwitch("Enable Aimbot", &aimbotEnabled);
            if (aimbotEnabled) {
                ImGui::Indent(10);
                ImGui::SliderInt("FOV Radius", &aimbotFov, 1, 180, "%d deg");
                ImGui::SliderFloat("Smoothness", &aimbotSmoothness, 1.0f, 20.0f, "%.1f");
                const char* boneNames[] = { "Head", "Chest", "Pelvis" };
                ImGui::Combo("Target Bone", &aimbotBone, boneNames, IM_ARRAYSIZE(boneNames));
                ImGui::Checkbox("Target Teammates", &aimbotTargetTeammates);
                ImGui::TextDisabled("Hold LMB to activate");
                ImGui::Unindent(10);
            }

            ImGui::Dummy(ImVec2(0, 12));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 8));

            ImGui::TextColored(accentVec, "TRIGGERBOT");
            ImGui::Spacing();
            ToggleSwitch("Enable Triggerbot", &triggerbotEnabled);
            if (triggerbotEnabled) {
                ImGui::Indent(10);
                ImGui::SliderInt("Reaction Delay", &triggerbotDelay, 0, 200, "%d ms");
                ImGui::Checkbox("Headshot Only", &triggerbotHeadshotOnly);
                ImGui::Unindent(10);
            }

            ImGui::Dummy(ImVec2(0, 12));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 8));

            ImGui::TextColored(accentVec, "RECOIL CONTROL");
            ImGui::Spacing();
            ToggleSwitch("Enable RCS", &rcsEnabled);
            if (rcsEnabled) {
                ImGui::Indent(10);
                ImGui::SliderFloat("Horizontal", &rcsHorizontal, 0.0f, 2.0f, "%.2f");
                ImGui::SliderFloat("Vertical", &rcsVertical, 0.0f, 2.0f, "%.2f");
                ImGui::TextDisabled("1.0 = natural feel");
                ImGui::Unindent(10);
            }
        }
        else if (currentTab == 1) {
            // === VISUALS TAB ===
            ImGui::TextColored(accentVec, "PLAYER ESP");
            ImGui::Spacing();

            ImGui::Columns(2, "##espcols", false);
            ImGui::SetColumnWidth(0, 250);

            ToggleSwitch("Bounding Boxes", &showEspBoxes);
            ToggleSwitch("Filled Boxes", &showFilledBoxes);
            ToggleSwitch("Health Bars", &showEspHealth);
            ToggleSwitch("Skeletons", &showSkeleton);
            ToggleSwitch("Snaplines", &showSnaplines);
            ToggleSwitch("Head Dots", &showHeadDots);
            ToggleSwitch("Visible Check", &showVisibleCheck);

            ImGui::NextColumn();

            ToggleSwitch("Player Names", &showEspNames);
            ToggleSwitch("Distance", &showEspDistance);
            ToggleSwitch("Show Teammates", &showTeammates);
            ToggleSwitch("Armor Bars", &showEspArmor);
            ToggleSwitch("Money", &showEspMoney);
            ToggleSwitch("Eye Lines", &showEyeLines);
            ToggleSwitch("Dropped Weapons", &showDroppedWeapons);

            ImGui::Columns(1);

            ImGui::Dummy(ImVec2(0, 8));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 8));

            ImGui::TextColored(accentVec, "BOX STYLE");
            ImGui::Spacing();
            const char* boxStyles[] = { "Corner Box", "Filled Box", "2D Box" };
            ImGui::Combo("Box Type", &boxType, boxStyles, IM_ARRAYSIZE(boxStyles));

            ImGui::Dummy(ImVec2(0, 8));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 8));

            ImGui::TextColored(accentVec, "WORLD");
            ImGui::Spacing();
            ToggleSwitch("World ESP", &showWorldEsp);
            ToggleSwitch("Bomb ESP", &showBombEsp);
            ToggleSwitch("Grenade Trajectory", &showGrenadeTrajectory);
            ToggleSwitch("Third Person", &thirdPersonEnabled);
        }
        else if (currentTab == 2) {
            // === MOVEMENT TAB ===
            ImGui::TextColored(accentVec, "MOVEMENT");
            ImGui::Spacing();
            ToggleSwitch("Bunny Hop (Hold Space)", &bhopEnabled);
            if (bhopEnabled) {
                ImGui::Indent(10);
                ImGui::Checkbox("Strafe Assist", &bhopStrafeAssist);
                ImGui::Unindent(10);
            }
        }
        else if (currentTab == 3) {
            // === SETTINGS TAB ===
            ImGui::TextColored(accentVec, "CONFIGURATION");
            ImGui::Spacing();
            ImGui::TextDisabled("Save and load your settings");
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.35f, 0.65f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.40f, 0.72f, 1.0f));
            if (ImGui::Button("SAVE CONFIG", ImVec2(200, 40))) SaveConfig("default");
            ImGui::PopStyleColor(2);

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.28f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.28f, 0.38f, 1.0f));
            if (ImGui::Button("LOAD CONFIG", ImVec2(200, 40))) LoadConfig("default");
            ImGui::PopStyleColor(2);

            ImGui::Dummy(ImVec2(0, 40));

            ImGui::TextColored(ImColor(255, 80, 80, 255), "DANGER ZONE");
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.20f, 0.20f, 1.0f));
            if (ImGui::Button("EXIT APPLICATION", ImVec2(410, 40))) {
                overlay.isRunning = false;
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::End();
    }
    ImGui::PopStyleVar();
}

void Menu::SaveConfig(const char* filename) {
    ConfigSystem config;
    config.SetPointers(this, nullptr);
    config.SaveConfig(filename);
}

void Menu::LoadConfig(const char* filename) {
    ConfigSystem config;
    config.SetPointers(this, nullptr);
    config.LoadConfig(filename);
}
