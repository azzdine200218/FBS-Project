#include "../include/imgui/imgui.h"
#include "ESP.hpp"
#include "../include/Offsets.hpp"
#include <iostream>
#include <cmath>
#include <cstdio>

bool ESP::WorldToScreen(const Vector3& worldPos, Vector2& screenPos, const Matrix4x4& viewMatrix, int screenWidth, int screenHeight) {
    float _x = viewMatrix.m[0] * worldPos.x + viewMatrix.m[1] * worldPos.y + viewMatrix.m[2] * worldPos.z + viewMatrix.m[3];
    float _y = viewMatrix.m[4] * worldPos.x + viewMatrix.m[5] * worldPos.y + viewMatrix.m[6] * worldPos.z + viewMatrix.m[7];
    float w = viewMatrix.m[12] * worldPos.x + viewMatrix.m[13] * worldPos.y + viewMatrix.m[14] * worldPos.z + viewMatrix.m[15];

    if (w < 0.01f)
        return false;

    float inv_w = 1.f / w;
    _x *= inv_w;
    _y *= inv_w;

    float x = screenWidth / 2.0f;
    float y = screenHeight / 2.0f;

    x += 0.5f * _x * screenWidth + 0.5f;
    y -= 0.5f * _y * screenHeight + 0.5f;

    screenPos.x = x;
    screenPos.y = y;

    return true;
}

ImColor ESP::GetHealthColor(int health) {
    if (health < 0) health = 0;
    if (health > 100) health = 100;
    float t = health / 100.0f;
    int r = (int)((1.0f - t) * 255);
    int g = (int)(t * 255);
    return ImColor(r, g, 40, 255);
}

ImColor ESP::GetDistanceColor(float distance) {
    if (distance < 10.0f) return ImColor(255, 60, 60, 255);
    if (distance < 25.0f) return ImColor(255, 180, 50, 255);
    if (distance < 50.0f) return ImColor(255, 255, 80, 255);
    return ImColor(120, 200, 255, 255);
}

void ESP::DrawSkeleton(KernelInterface& kernel, ULONG pid, uint64_t pCSPlayerPawn, const Matrix4x4& viewMatrix, int screenWidth, int screenHeight) {
    uint64_t gameSceneNode = kernel.ReadMemory<uint64_t>(pid, pCSPlayerPawn + offsets::m_pGameSceneNode);
    if (!gameSceneNode) return;

    uint64_t boneArray = kernel.ReadMemory<uint64_t>(pid, gameSceneNode + offsets::m_modelState + 0x80);
    if (!boneArray || boneArray < 0x1000000) {
        boneArray = kernel.ReadMemory<uint64_t>(pid, gameSceneNode + offsets::m_modelState + 0xA8);
    }

    if (!boneArray || boneArray < 0x1000000) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    char boneData[1024] = {};
    if (!kernel.BulkRead(pid, boneArray, boneData, sizeof(boneData))) return;

    for (const auto& connection : boneConnections) {
        if (connection.bone1 * 32 >= sizeof(boneData) || connection.bone2 * 32 >= sizeof(boneData)) continue;

        Vector3 bone1Pos = *(Vector3*)(boneData + connection.bone1 * 32);
        Vector3 bone2Pos = *(Vector3*)(boneData + connection.bone2 * 32);

        if ((std::abs(bone1Pos.x) < 0.01f && std::abs(bone1Pos.y) < 0.01f) ||
            (std::abs(bone2Pos.x) < 0.01f && std::abs(bone2Pos.y) < 0.01f)) {
            continue;
        }

        float dx = bone1Pos.x - bone2Pos.x;
        float dy = bone1Pos.y - bone2Pos.y;
        float dz = bone1Pos.z - bone2Pos.z;
        float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        if (distance > 45.0f) continue;

        Vector2 screenPos1, screenPos2;
        if (WorldToScreen(bone1Pos, screenPos1, viewMatrix, screenWidth, screenHeight) &&
            WorldToScreen(bone2Pos, screenPos2, viewMatrix, screenWidth, screenHeight)) {

            bool isSpine = (connection.bone1 == Bones::Head || connection.bone1 == Bones::Neck ||
                           connection.bone1 == Bones::Spine || connection.bone2 == Bones::Spine);
            ImColor boneColor = isSpine ? ImColor(130, 160, 255, 200) : ImColor(200, 210, 255, 160);

            drawList->AddLine(ImVec2(screenPos1.x, screenPos1.y), ImVec2(screenPos2.x, screenPos2.y),
                            ImColor(0, 0, 0, 120), 3.0f);
            drawList->AddLine(ImVec2(screenPos1.x, screenPos1.y), ImVec2(screenPos2.x, screenPos2.y),
                            boneColor, 1.5f);

            if (connection.bone1 == Bones::Head || connection.bone2 == Bones::Head) {
                Vector2 headScreen = (connection.bone1 == Bones::Head) ? screenPos1 : screenPos2;
                drawList->AddCircle(ImVec2(headScreen.x, headScreen.y), 3.0f, ImColor(255, 255, 255, 180), 8, 1.0f);
            }
        }
    }
}

void ESP::DrawCornerBox(float x, float y, float w, float h, ImColor color, float thickness) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    float lineW = w / 3.5f;
    float lineH = h / 3.5f;
    ImColor outlineClr = ImColor(0, 0, 0, 180);

    auto drawCorner = [&](ImVec2 p1, ImVec2 p2, ImVec2 p3) {
        drawList->AddLine(p1, p2, outlineClr, thickness + 2.0f);
        drawList->AddLine(p1, p3, outlineClr, thickness + 2.0f);
        drawList->AddLine(p1, p2, color, thickness);
        drawList->AddLine(p1, p3, color, thickness);
    };

    drawCorner(ImVec2(x, y), ImVec2(x + lineW, y), ImVec2(x, y + lineH));
    drawCorner(ImVec2(x + w, y), ImVec2(x + w - lineW, y), ImVec2(x + w, y + lineH));
    drawCorner(ImVec2(x, y + h), ImVec2(x + lineW, y + h), ImVec2(x, y + h - lineH));
    drawCorner(ImVec2(x + w, y + h), ImVec2(x + w - lineW, y + h), ImVec2(x + w, y + h - lineH));
}

void ESP::DrawFilledBox(float x, float y, float w, float h, ImColor fillColor, ImColor outlineColor) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImColor gradTop = ImColor(
        (int)(fillColor.Value.x * 255), (int)(fillColor.Value.y * 255),
        (int)(fillColor.Value.z * 255), 30);
    ImColor gradBot = ImColor(
        (int)(fillColor.Value.x * 255), (int)(fillColor.Value.y * 255),
        (int)(fillColor.Value.z * 255), 80);
    drawList->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + w, y + h),
        (ImU32)gradTop, (ImU32)gradTop, (ImU32)gradBot, (ImU32)gradBot);
    drawList->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), outlineColor, 0.0f, ImDrawFlags_None, 1.5f);
}

void ESP::DrawGlowBox(float x, float y, float w, float h, ImColor color, float glowRadius) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImColor glowColor = ImColor(
        (int)(color.Value.x * 255), (int)(color.Value.y * 255),
        (int)(color.Value.z * 255), 30);

    for (float i = glowRadius; i > 0; i -= 1.0f) {
        float alpha = (1.0f - i / glowRadius) * 0.15f;
        ImColor stepColor = ImColor(
            (int)(color.Value.x * 255), (int)(color.Value.y * 255),
            (int)(color.Value.z * 255), (int)(alpha * 255));
        drawList->AddRect(ImVec2(x - i, y - i), ImVec2(x + w + i, y + h + i), stepColor, 0.0f, ImDrawFlags_None, 1.0f);
    }
    drawList->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), color, 0.0f, ImDrawFlags_None, 1.5f);
}

void ESP::DrawHealthBar(float x, float y, float h, int health) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (health < 0) health = 0;
    if (health > 100) health = 100;

    float barH = h * (health / 100.0f);
    ImColor healthClr = GetHealthColor(health);

    drawList->AddRectFilled(ImVec2(x - 6, y - 1), ImVec2(x - 2, y + h + 1), ImColor(0, 0, 0, 180), 1.0f);
    drawList->AddRectFilled(ImVec2(x - 5, y + h - barH), ImVec2(x - 3, y + h), healthClr, 1.0f);

    if (health < 100) {
        char hpText[8];
        snprintf(hpText, sizeof(hpText), "%d", health);
        ImVec2 textSize = ImGui::CalcTextSize(hpText);
        float textY = y + h - barH - textSize.y - 2.0f;
        if (textY < y - textSize.y) textY = y - textSize.y;
        drawList->AddText(ImVec2(x - 5 - textSize.x / 2.0f + 1, textY + 1), ImColor(0, 0, 0, 200), hpText);
        drawList->AddText(ImVec2(x - 5 - textSize.x / 2.0f, textY), healthClr, hpText);
    }
}

void ESP::DrawGradientHealthBar(float x, float y, float h, int health) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (health < 0) health = 0;
    if (health > 100) health = 100;

    float barH = h * (health / 100.0f);
    float barWidth = 3.0f;

    drawList->AddRectFilled(ImVec2(x - 7, y - 1), ImVec2(x - 7 + barWidth + 2, y + h + 1), ImColor(0, 0, 0, 180), 2.0f);

    ImColor topColor = ImColor(50, 255, 80, 255);
    ImColor botColor = ImColor(255, 50, 50, 255);
    float filledTop = y + h - barH;
    drawList->AddRectFilledMultiColor(
        ImVec2(x - 6, filledTop), ImVec2(x - 6 + barWidth, y + h),
        (ImU32)topColor, (ImU32)topColor, (ImU32)botColor, (ImU32)botColor);
}

void ESP::DrawArmorBar(float x, float y, float h, int armor) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (armor < 0) armor = 0;
    if (armor > 100) armor = 100;

    float barH = h * (armor / 100.0f);

    drawList->AddRectFilled(ImVec2(x - 11, y - 1), ImVec2(x - 7, y + h + 1), ImColor(0, 0, 0, 180), 1.0f);
    drawList->AddRectFilled(ImVec2(x - 10, y + h - barH), ImVec2(x - 8, y + h), ImColor(80, 140, 255, 230), 1.0f);
}

void ESP::DrawInfo(float x, float y, const char* text, ImColor color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    drawList->AddText(ImVec2(x + 1, y + 1), ImColor(0, 0, 0, 180), text);
    drawList->AddText(ImVec2(x, y), color, text);
}

void ESP::DrawInfoBadge(float x, float y, const char* text, ImColor bgColor, ImColor textColor) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImVec2 textSize = ImGui::CalcTextSize(text);
    float padX = 4.0f, padY = 2.0f;

    drawList->AddRectFilled(
        ImVec2(x - padX, y - padY),
        ImVec2(x + textSize.x + padX, y + textSize.y + padY),
        bgColor, 3.0f);
    drawList->AddText(ImVec2(x, y), textColor, text);
}

void ESP::DrawSnapline(float targetX, float targetY, ImColor color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    float screenW = (float)ImGui::GetIO().DisplaySize.x;
    float screenH = (float)ImGui::GetIO().DisplaySize.y;
    ImVec2 centerBottom = ImVec2(screenW / 2.0f, screenH);

    ImColor fadeColor = ImColor(
        (int)(color.Value.x * 255), (int)(color.Value.y * 255),
        (int)(color.Value.z * 255), 40);

    drawList->AddLine(centerBottom, ImVec2(targetX, targetY), fadeColor, 2.0f);
    drawList->AddLine(centerBottom, ImVec2(targetX, targetY), color, 1.0f);
}

void ESP::DrawHeadDot(float x, float y, float radius, ImColor color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    drawList->AddCircleFilled(ImVec2(x, y), radius + 1.0f, ImColor(0, 0, 0, 160), 12);
    drawList->AddCircleFilled(ImVec2(x, y), radius, color, 12);
    drawList->AddCircle(ImVec2(x, y), radius + 0.5f, ImColor(255, 255, 255, 60), 12, 0.5f);
}

void ESP::DrawEyeLine(float headX, float headY, float eyeForwardX, float eyeForwardY, ImColor color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImVec2 start = ImVec2(headX, headY - 5.0f);
    ImVec2 end = ImVec2(eyeForwardX, eyeForwardY);
    float length = sqrtf((end.x - start.x) * (end.x - start.x) + (end.y - start.y) * (end.y - start.y));
    if (length > 5.0f && length < 100.0f) {
        drawList->AddLine(start, end, ImColor(0, 0, 0, 100), 2.5f);
        drawList->AddLine(start, end, color, 1.5f);
    }
}

void ESP::DrawWeaponIcon(float x, float y, const char* weaponName, ImColor color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImVec2 textSize = ImGui::CalcTextSize(weaponName);
    DrawInfoBadge(x - textSize.x / 2.0f, y, weaponName, ImColor(0, 0, 0, 140), color);
}

void ESP::DrawGrenadeTrajectory(KernelInterface& kernel, ULONG pid, uint64_t grenadePawn, const Matrix4x4& viewMatrix, int screenWidth, int screenHeight) {
    Vector3 grenadePos = kernel.ReadMemory<Vector3>(pid, grenadePawn + 0xD0);
    Vector3 grenadeVelocity = kernel.ReadMemory<Vector3>(pid, grenadePawn + 0x120);
    
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    Vector3 simPos = grenadePos;
    Vector3 simVel = grenadeVelocity;
    float gravity = 800.0f;
    float step = 0.03f;
    
    Vector2 prevScreen = {0, 0};
    bool hasPrev = false;

    for (int i = 0; i < 100; i++) {
        simVel.z -= gravity * step;
        simPos.x += simVel.x * step;
        simPos.y += simVel.y * step;
        simPos.z += simVel.z * step;
        
        Vector2 screenPos;
        if (WorldToScreen(simPos, screenPos, viewMatrix, screenWidth, screenHeight)) {
            float alpha = 1.0f - (i / 100.0f);
            ImColor dotColor = ImColor(255, 200, 50, (int)(alpha * 220));

            if (hasPrev) {
                drawList->AddLine(
                    ImVec2(prevScreen.x, prevScreen.y),
                    ImVec2(screenPos.x, screenPos.y),
                    ImColor(255, 200, 50, (int)(alpha * 120)), 1.0f);
            }
            drawList->AddCircleFilled(ImVec2(screenPos.x, screenPos.y), 2.0f, dotColor);
            prevScreen = screenPos;
            hasPrev = true;
        }
        
        if (simPos.z < 0.0f) break;
    }

    if (hasPrev) {
        drawList->AddCircleFilled(ImVec2(prevScreen.x, prevScreen.y), 5.0f, ImColor(255, 100, 50, 200));
        drawList->AddCircle(ImVec2(prevScreen.x, prevScreen.y), 8.0f, ImColor(255, 100, 50, 100), 12, 1.5f);
    }
}

void ESP::DrawVisibleCheck(float x, float y, bool isVisible) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const char* text = isVisible ? "VISIBLE" : "HIDDEN";
    ImColor bgColor = isVisible ? ImColor(30, 180, 60, 180) : ImColor(180, 40, 40, 180);
    ImColor textColor = ImColor(255, 255, 255, 240);
    ImVec2 textSize = ImGui::CalcTextSize(text);
    DrawInfoBadge(x - textSize.x / 2.0f, y, text, bgColor, textColor);
}

void ESP::DrawC4Timer(float x, float y, float timeRemaining, ImColor color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    
    char timerStr[32];
    sprintf_s(timerStr, "C4: %.1fs", timeRemaining);
    
    ImVec2 textSize = ImGui::CalcTextSize(timerStr);
    float padX = 6.0f, padY = 3.0f;

    ImColor bgColor = timeRemaining < 10.0f ? ImColor(180, 30, 30, 200) : ImColor(0, 0, 0, 200);
    drawList->AddRectFilled(
        ImVec2(x - padX, y - padY),
        ImVec2(x + textSize.x + padX, y + textSize.y + padY),
        bgColor, 4.0f);

    if (timeRemaining < 10.0f) {
        drawList->AddRect(
            ImVec2(x - padX, y - padY),
            ImVec2(x + textSize.x + padX, y + textSize.y + padY),
            ImColor(255, 80, 80, 200), 4.0f, ImDrawFlags_None, 1.0f);
    }
    
    ImColor timerColor = timeRemaining < 10.0f ? ImColor(255, 80, 80, 255) : color;
    drawList->AddText(ImVec2(x, y), timerColor, timerStr);
}
