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

void ESP::DrawSkeleton(KernelInterface& kernel, ULONG pid, uint64_t pCSPlayerPawn, const Matrix4x4& viewMatrix, int screenWidth, int screenHeight) {
    // Dynamic offsets from Offsets.hpp (updated by cs2-dumper)
    uint64_t gameSceneNode = kernel.ReadMemory<uint64_t>(pid, pCSPlayerPawn + offsets::m_pGameSceneNode);
    if (!gameSceneNode) return;

    // Bone array is typically at m_modelState + 0x80 (0x1D0 total) or 0x1F8 in some builds
    uint64_t boneArray = kernel.ReadMemory<uint64_t>(pid, gameSceneNode + offsets::m_modelState + 0x80);
    if (!boneArray || boneArray < 0x1000000) {
        boneArray = kernel.ReadMemory<uint64_t>(pid, gameSceneNode + offsets::m_modelState + 0xA8); // Fallback to 0x1F8
    }

    if (!boneArray || boneArray < 0x1000000) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    // CS2 bones are stored as 32-byte structs. Bulk read the first 32 bones (32 * 32 = 1024 bytes).
    // This reduces 30 kernel calls per player down to a single BulkRead.
    char boneData[1024] = {};
    if (!kernel.BulkRead(pid, boneArray, boneData, sizeof(boneData))) return;

    for (const auto& connection : boneConnections) {
        if (connection.bone1 * 32 >= sizeof(boneData) || connection.bone2 * 32 >= sizeof(boneData)) continue;

        Vector3 bone1Pos = *(Vector3*)(boneData + connection.bone1 * 32);
        Vector3 bone2Pos = *(Vector3*)(boneData + connection.bone2 * 32);

        // Skip drawing if bone positions are (0,0,0) - fixes lines stretching to map origin due to LOD
        if ((bone1Pos.x == 0.0f && bone1Pos.y == 0.0f && bone1Pos.z == 0.0f) ||
            (bone2Pos.x == 0.0f && bone2Pos.y == 0.0f && bone2Pos.z == 0.0f)) {
            continue;
        }

        Vector2 screenPos1, screenPos2;
        if (WorldToScreen(bone1Pos, screenPos1, viewMatrix, screenWidth, screenHeight) &&
            WorldToScreen(bone2Pos, screenPos2, viewMatrix, screenWidth, screenHeight)) {
            drawList->AddLine(ImVec2(screenPos1.x, screenPos1.y), ImVec2(screenPos2.x, screenPos2.y), ImColor(255, 255, 255, 140), 1.0f);
        }
    }
}

void ESP::DrawCornerBox(float x, float y, float w, float h, ImColor color, float thickness) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    float lineW = w / 4.0f;
    float lineH = h / 4.0f;
    ImColor outlineClr = ImColor(0, 0, 0, 200);

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
    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), fillColor);
    drawList->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), outlineColor, 0.0f, ImDrawFlags_None, 1.5f);
}

void ESP::DrawHealthBar(float x, float y, float h, int health) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (health < 0) health = 0;
    if (health > 100) health = 100;

    float barH = h * (health / 100.0f);
    ImColor healthClr = ImColor::HSV((health / 100.0f) * 0.33f, 1.0f, 1.0f);

    drawList->AddRectFilled(ImVec2(x - 6, y - 1), ImVec2(x - 2, y + h + 1), ImColor(0, 0, 0, 150));
    drawList->AddRectFilled(ImVec2(x - 5, y + h - barH), ImVec2(x - 3, y + h), healthClr);
}

void ESP::DrawArmorBar(float x, float y, float h, int armor) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (armor < 0) armor = 0;
    if (armor > 100) armor = 100;

    float barH = h * (armor / 100.0f);

    drawList->AddRectFilled(ImVec2(x - 10, y - 1), ImVec2(x - 6, y + h + 1), ImColor(0, 0, 0, 150));
    drawList->AddRectFilled(ImVec2(x - 9, y + h - barH), ImVec2(x - 7, y + h), ImColor(100, 150, 255, 220));
}

void ESP::DrawInfo(float x, float y, const char* text, ImColor color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    drawList->AddText(ImVec2(x + 1, y + 1), ImColor(0, 0, 0, 200), text);
    drawList->AddText(ImVec2(x, y), color, text);
}

void ESP::DrawSnapline(float targetX, float targetY, ImColor color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    float screenW = (float)ImGui::GetIO().DisplaySize.x;
    float screenH = (float)ImGui::GetIO().DisplaySize.y;
    ImVec2 centerBottom = ImVec2(screenW / 2.0f, screenH);
    drawList->AddLine(centerBottom, ImVec2(targetX, targetY), color, 1.2f);
}

void ESP::DrawHeadDot(float x, float y, float radius, ImColor color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    drawList->AddCircleFilled(ImVec2(x, y), radius, ImColor(0, 0, 0, 180));
    drawList->AddCircleFilled(ImVec2(x, y), radius - 1.0f, color);
}

void ESP::DrawEyeLine(float headX, float headY, float eyeForwardX, float eyeForwardY, ImColor color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImVec2 start = ImVec2(headX, headY - 5.0f);
    ImVec2 end = ImVec2(eyeForwardX, eyeForwardY);
    float length = sqrtf((end.x - start.x) * (end.x - start.x) + (end.y - start.y) * (end.y - start.y));
    if (length > 5.0f && length < 100.0f) {
        drawList->AddLine(start, end, color, 1.5f);
    }
}

void ESP::DrawWeaponIcon(float x, float y, const char* weaponName, ImColor color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImVec2 textSize = ImGui::CalcTextSize(weaponName);
    drawList->AddText(ImVec2(x - textSize.x / 2.0f, y), color, weaponName);
}

void ESP::DrawGrenadeTrajectory(KernelInterface& kernel, ULONG pid, uint64_t grenadePawn, const Matrix4x4& viewMatrix, int screenWidth, int screenHeight) {
    Vector3 grenadePos = kernel.ReadMemory<Vector3>(pid, grenadePawn + 0xD0);
    Vector3 grenadeVelocity = kernel.ReadMemory<Vector3>(pid, grenadePawn + 0x120);
    
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    Vector3 simPos = grenadePos;
    Vector3 simVel = grenadeVelocity;
    float gravity = 800.0f;
    float step = 0.03f;
    
    for (int i = 0; i < 100; i++) {
        simVel.z -= gravity * step;
        simPos.x += simVel.x * step;
        simPos.y += simVel.y * step;
        simPos.z += simVel.z * step;
        
        Vector2 screenPos;
        if (WorldToScreen(simPos, screenPos, viewMatrix, screenWidth, screenHeight)) {
            float alpha = 1.0f - (i / 100.0f);
            drawList->AddCircleFilled(ImVec2(screenPos.x, screenPos.y), 2.0f, ImColor(255, 200, 50, (int)(alpha * 200)));
        }
        
        if (simPos.z < 0.0f) break;
    }
}

void ESP::DrawVisibleCheck(float x, float y, bool isVisible) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const char* text = isVisible ? "VISIBLE" : "OCCLUDED";
    ImColor color = isVisible ? ImColor(0, 255, 0, 200) : ImColor(255, 0, 0, 200);
    ImVec2 textSize = ImGui::CalcTextSize(text);
    drawList->AddText(ImVec2(x - textSize.x / 2.0f, y), color, text);
}

void ESP::DrawC4Timer(float x, float y, float timeRemaining, ImColor color) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    
    char timerStr[32];
    sprintf_s(timerStr, "C4: %.1fs", timeRemaining);
    
    ImVec2 textSize = ImGui::CalcTextSize(timerStr);
    drawList->AddRectFilled(
        ImVec2(x - 4, y - 2),
        ImVec2(x + textSize.x + 4, y + textSize.y + 2),
        ImColor(0, 0, 0, 180)
    );
    
    ImColor timerColor = timeRemaining < 10.0f ? ImColor(255, 50, 50, 255) : color;
    drawList->AddText(ImVec2(x, y), timerColor, timerStr);
}
