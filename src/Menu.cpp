#include "Menu.hpp"
#include "ConfigSystem.hpp"
#include "../include/imgui/imgui.h"
#include "../include/XorStr.hpp"

void Menu::Draw(Overlay& overlay) {
    if (!overlay.showMenu) return;

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImColor(18, 18, 22, 255);
    colors[ImGuiCol_ChildBg] = ImColor(24, 24, 28, 255);
    colors[ImGuiCol_Border] = ImColor(45, 45, 50, 150);
    colors[ImGuiCol_FrameBg] = ImColor(35, 35, 40, 255);
    colors[ImGuiCol_FrameBgHovered] = ImColor(45, 45, 50, 255);
    colors[ImGuiCol_FrameBgActive] = ImColor(55, 55, 60, 255);
    colors[ImGuiCol_TitleBg] = ImColor(18, 18, 22, 255);
    colors[ImGuiCol_TitleBgActive] = ImColor(18, 18, 22, 255);
    
    ImVec4 accentColor = ImColor(114, 137, 218, 255);
    ImVec4 accentDim = ImColor(114, 137, 218, 150);
    
    colors[ImGuiCol_CheckMark] = accentColor;
    colors[ImGuiCol_SliderGrab] = accentColor;
    colors[ImGuiCol_SliderGrabActive] = ImColor(134, 157, 238, 255);
    colors[ImGuiCol_Button] = ImColor(40, 40, 45, 255);
    colors[ImGuiCol_ButtonHovered] = ImColor(50, 50, 55, 255);
    colors[ImGuiCol_ButtonActive] = accentDim;
    colors[ImGuiCol_Header] = ImColor(35, 35, 40, 255);
    colors[ImGuiCol_HeaderHovered] = ImColor(45, 45, 50, 255);
    colors[ImGuiCol_HeaderActive] = ImColor(55, 55, 60, 255);
    colors[ImGuiCol_Text] = ImColor(240, 240, 245, 255);
    colors[ImGuiCol_TextDisabled] = ImColor(150, 150, 160, 255);

    ImGui::SetNextWindowSize(ImVec2(700, 480), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("FBS Premium", &overlay.showMenu, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {
        
        ImGui::TextColored(accentColor, "FBS");
        ImGui::SameLine();
        ImGui::Text("PREMIUM");
        ImGui::SameLine(ImGui::GetWindowWidth() - 140);
        ImGui::TextDisabled("Status: ACTIVE");
        ImGui::Separator();
        
        ImGui::BeginChild("Sidebar", ImVec2(160, 0), true);
        ImGui::Spacing();
        ImGui::TextDisabled(" MAIN MENU");
        ImGui::Spacing();

        auto drawTab = [&](const char* label, int index) {
            bool selected = (currentTab == index);
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accentColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, accentColor);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            }
            
            if (ImGui::Button(label, ImVec2(140, 35))) {
                currentTab = index;
            }
            
            if (selected) {
                ImGui::PopStyleColor(4);
            } else {
                ImGui::PopStyleColor(1);
            }
            ImGui::Spacing();
        };

        drawTab("Combat", 0);
        drawTab("Visuals & ESP", 1);
        drawTab("Movement", 2);
        drawTab("System Config", 3);

        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);
        ImGui::TextDisabled("   v5.1 Ghost");

        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("Content", ImVec2(0, 0), true);
        
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 15));
        ImGui::SetCursorPos(ImVec2(20, 20));
        ImGui::BeginGroup();

        if (currentTab == 0) {
            ImGui::TextColored(accentColor, "AIMBOT");
            ImGui::Separator();
            ImGui::Checkbox("Enable Aimbot", &aimbotEnabled);
            ImGui::SliderInt("FOV Radius", &aimbotFov, 1, 180, "%d deg");
            ImGui::SliderFloat("Smoothness", &aimbotSmoothness, 1.0f, 20.0f, "%.1f");
            const char* boneNames[] = { "Head", "Chest", "Pelvis" };
            ImGui::Combo("Target Bone", &aimbotBone, boneNames, IM_ARRAYSIZE(boneNames));
            ImGui::Checkbox("Target Teammates", &aimbotTargetTeammates);
            ImGui::TextDisabled("Hold Left Mouse Button to activate");

            ImGui::Dummy(ImVec2(0, 15));

            ImGui::TextColored(accentColor, "TRIGGERBOT");
            ImGui::Separator();
            ImGui::Checkbox("Enable Triggerbot", &triggerbotEnabled);
            ImGui::SliderInt("Reaction Delay (ms)", &triggerbotDelay, 0, 200, "%d ms");
            ImGui::Checkbox("Headshot Only Mode", &triggerbotHeadshotOnly);

            ImGui::Dummy(ImVec2(0, 15));

            ImGui::TextColored(accentColor, "RECOIL CONTROL (RCS)");
            ImGui::Separator();
            ImGui::Checkbox("Enable RCS", &rcsEnabled);
            ImGui::SliderFloat("Horizontal Strength", &rcsHorizontal, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Vertical Strength", &rcsVertical, 0.0f, 2.0f, "%.2f");
            ImGui::TextDisabled("Keep values around 1.0 for natural feel");
        }
        else if (currentTab == 1) {
            ImGui::TextColored(accentColor, "PLAYER VISUALS");
            ImGui::Separator();
            
            ImGui::Columns(2, nullptr, false);
            ImGui::Checkbox("Bounding Boxes", &showEspBoxes);
            ImGui::Checkbox("Health Bars", &showEspHealth);
            ImGui::Checkbox("Skeletons", &showSkeleton);
            ImGui::Checkbox("Snaplines", &showSnaplines);
            ImGui::Checkbox("Head Dots", &showHeadDots);
            ImGui::Checkbox("Eye Lines", &showEyeLines);
            ImGui::Checkbox("Visible Check", &showVisibleCheck);
            ImGui::Checkbox("Filled Boxes", &showFilledBoxes);
            
            ImGui::NextColumn();
            ImGui::Checkbox("Player Names", &showEspNames);
            ImGui::Checkbox("Distance", &showEspDistance);
            ImGui::Checkbox("Show Teammates", &showTeammates);
            ImGui::Checkbox("Show Armor", &showEspArmor);
            ImGui::Checkbox("Show Money", &showEspMoney);
            ImGui::Checkbox("Dropped Weapons", &showDroppedWeapons);
            ImGui::Columns(1);
            
            ImGui::Dummy(ImVec2(0, 15));
            
            ImGui::TextColored(accentColor, "BOX STYLE");
            ImGui::Separator();
            const char* boxStyles[] = { "Corner Box", "Filled Box", "2D Box" };
            ImGui::Combo("Box Type", &boxType, boxStyles, IM_ARRAYSIZE(boxStyles));
            
            ImGui::Dummy(ImVec2(0, 15));
            
            ImGui::TextColored(accentColor, "WORLD VISUALS");
            ImGui::Separator();
            ImGui::Checkbox("World ESP (Items)", &showWorldEsp);
            ImGui::Checkbox("C4 Bomb ESP", &showBombEsp);
            ImGui::Checkbox("Grenade Trajectory", &showGrenadeTrajectory);
            ImGui::Checkbox("Force Third Person", &thirdPersonEnabled);
        }
        else if (currentTab == 2) {
            ImGui::TextColored(accentColor, "MOVEMENT ASSISTANCE");
            ImGui::Separator();
            ImGui::Checkbox("Enable Bunny Hop (Hold Space)", &bhopEnabled);
            ImGui::Checkbox("Strafe Assistance", &bhopStrafeAssist);
        }
        else if (currentTab == 3) {
            ImGui::TextColored(accentColor, "CONFIGURATION");
            ImGui::Separator();
            ImGui::Text("Save your current tuning layout to config.json");
            ImGui::Spacing();
            
            if (ImGui::Button("SAVE SETTINGS", ImVec2(200, 40))) SaveConfig("default");
            ImGui::SameLine();
            if (ImGui::Button("LOAD SETTINGS", ImVec2(200, 40))) LoadConfig("default");
            
            ImGui::Dummy(ImVec2(0, 40));
            
            ImGui::TextColored(ImColor(255, 50, 50), "DANGER ZONE");
            ImGui::Separator();
            if (ImGui::Button("UNLOAD CHEAT ENGINE", ImVec2(410, 40))) {
                overlay.isRunning = false; 
            }
        }

        ImGui::EndGroup();
        ImGui::PopStyleVar();
        
        ImGui::EndChild();

        ImGui::End();
    }
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
