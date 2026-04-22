#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include "../include/imgui/imgui.h"
#include "../include/Data.hpp"
#include "../include/KernelInterface.hpp"

struct BoneConnection {
    int bone1;
    int bone2;
};

inline const std::vector<BoneConnection> boneConnections = {
    {Bones::Head, Bones::Neck},
    {Bones::Neck, Bones::Spine},
    {Bones::Spine, Bones::Pelvis},
    {Bones::Neck, Bones::ShoulderLeft},
    {Bones::ShoulderLeft, Bones::ElbowLeft},
    {Bones::ElbowLeft, Bones::HandLeft},
    {Bones::Neck, Bones::ShoulderRight},
    {Bones::ShoulderRight, Bones::ElbowRight},
    {Bones::ElbowRight, Bones::HandRight},
    {Bones::Pelvis, Bones::HipLeft},
    {Bones::HipLeft, Bones::KneeLeft},
    {Bones::KneeLeft, Bones::FootLeft},
    {Bones::Pelvis, Bones::HipRight},
    {Bones::HipRight, Bones::KneeRight},
    {Bones::KneeRight, Bones::FootRight}
};

class ESP {
public:
    static bool WorldToScreen(const Vector3& worldPos, Vector2& screenPos, const Matrix4x4& viewMatrix, int screenWidth, int screenHeight);
    static void DrawSkeleton(KernelInterface& kernel, ULONG pid, uint64_t pCSPlayerPawn, const Matrix4x4& viewMatrix, int screenWidth, int screenHeight);
    
    static void DrawCornerBox(float x, float y, float w, float h, ImColor color, float thickness = 1.0f);
    static void DrawFilledBox(float x, float y, float w, float h, ImColor fillColor, ImColor outlineColor);
    static void DrawHealthBar(float x, float y, float h, int health);
    static void DrawArmorBar(float x, float y, float h, int armor);
    static void DrawInfo(float x, float y, const char* text, ImColor color);
    static void DrawSnapline(float targetX, float targetY, ImColor color);
    static void DrawHeadDot(float x, float y, float radius, ImColor color);
    static void DrawEyeLine(float headX, float headY, float eyeForwardX, float eyeForwardY, ImColor color);
    static void DrawWeaponIcon(float x, float y, const char* weaponName, ImColor color);
    static void DrawC4Timer(float x, float y, float timeRemaining, ImColor color);
    static void DrawGrenadeTrajectory(KernelInterface& kernel, ULONG pid, uint64_t grenadePawn, const Matrix4x4& viewMatrix, int screenWidth, int screenHeight);
    static void DrawVisibleCheck(float x, float y, bool isVisible);
};
