#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include "../include/KernelInterface.hpp"
#include "../include/Offsets.hpp"
#include "../include/Data.hpp"
#include "../include/EntityManager.hpp"
#include "Menu.hpp"
#include <windows.h>

static constexpr float AIMBOT_PI = 3.14159265358979323846f;

namespace Aimbot {

    static void CalculateAngles(const Vector3& from, const Vector3& to, Vector3& angles) {
        float dx = to.x - from.x;
        float dy = to.y - from.y;
        float dz = to.z - from.z;
        float hyp = sqrtf(dx * dx + dy * dy);
        angles.x = atan2f(-dz, hyp) * (180.0f / AIMBOT_PI);
        angles.y = atan2f(dy, dx)   * (180.0f / AIMBOT_PI);
        angles.z = 0.0f;
    }

    static float NormalizeYaw(float yaw) {
        while (yaw > 180.0f) yaw -= 360.0f;
        while (yaw < -180.0f) yaw += 360.0f;
        return yaw;
    }

    static float NormalizePitch(float pitch) {
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        return pitch;
    }

    static float GetFov(const Vector3& viewAngles, const Vector3& targetAngles) {
        float dx = NormalizePitch(targetAngles.x - viewAngles.x);
        float dy = NormalizeYaw(targetAngles.y - viewAngles.y);
        return sqrtf(dx * dx + dy * dy);
    }

    static void ClampAngles(Vector3& angles) {
        if (std::isnan(angles.x) || std::isnan(angles.y) || std::isinf(angles.x) || std::isinf(angles.y)) {
            angles = {0, 0, 0};
            return;
        }
        angles.x = NormalizePitch(angles.x);
        angles.y = NormalizeYaw(angles.y);
        angles.z = 0.0f;
    }

    static float GetDistance3D(const Vector3& a, const Vector3& b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return sqrtf(dx * dx + dy * dy + dz * dz);
    }

    struct AimTarget {
        Vector3 angle;
        float fov;
        float distance;
        float score;
        int health;
    };

    static float CalculateTargetScore(float fov, float distance, int health, float maxFov) {
        float fovWeight = 1.0f - (fov / maxFov);
        float distWeight = 1.0f / (1.0f + distance * 0.001f);
        float hpWeight = (health <= 50) ? 1.2f : 1.0f;
        return (fovWeight * 0.6f + distWeight * 0.3f) * hpWeight;
    }

    static float GetAdaptiveSmooth(float baseSmoothness, float fov, float distance) {
        float smooth = baseSmoothness;

        // Closer to target = more smoothing to avoid overshooting
        if (fov < 2.0f) {
            smooth *= 1.5f;
        } else if (fov < 5.0f) {
            smooth *= 1.2f;
        } else if (fov > 30.0f) {
            smooth *= 0.7f;
        }

        // Far targets need less smoothing for faster acquisition
        if (distance > 3000.0f) {
            smooth *= 0.85f;
        }

        return (smooth < 1.0f) ? 1.0f : smooth;
    }

    static bool Execute(KernelInterface& kernel, ULONG pid, uint64_t clientBase,
                        uint64_t localPawn, EntityManager& entMgr, const Menu& menu) {
        
        if (!menu.aimbotEnabled || !localPawn) return false;
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) return false;

        Vector3 viewAngles = kernel.ReadMemory<Vector3>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewAngles);
        
        Vector3 currentPunch = kernel.ReadMemory<Vector3>(pid, localPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_aimPunchAngle);
        int32_t shotsFired = kernel.ReadMemory<int32_t>(pid, localPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iShotsFired);

        if (shotsFired <= 0) currentPunch = {0, 0, 0};

        Vector3 localOrigin = kernel.ReadMemory<Vector3>(pid, localPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
        Vector3 localEye = { localOrigin.x, localOrigin.y, localOrigin.z + 64.0f };
        int localTeam = kernel.ReadMemory<uint8_t>(pid, localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);

        float maxFov = (float)menu.aimbotFov;

        Vector3 crosshair = {
            viewAngles.x + currentPunch.x * 2.0f,
            viewAngles.y + currentPunch.y * 2.0f,
            0.0f
        };

        std::vector<AimTarget> candidates;

        for (const auto& player : entMgr.GetPlayers()) {
            if (!player.pawn || player.pawn == localPawn) continue;
            if (!menu.aimbotTargetTeammates && player.team == localTeam) continue;
            if (player.health <= 0) continue;

            Vector3 targetPos = player.origin;
            if (player.boneArray && player.boneArray >= 0x1000000) {
                int boneId = Bones::Head;
                if      (menu.aimbotBone == 1) boneId = Bones::Spine;
                else if (menu.aimbotBone == 2) boneId = Bones::Pelvis;
                
                Vector3 bonePos = kernel.ReadMemory<Vector3>(pid, player.boneArray + boneId * 32);
                if (bonePos.x != 0.0f || bonePos.y != 0.0f) targetPos = bonePos;
            } else {
                if      (menu.aimbotBone == 0) targetPos.z += 70.0f;
                else if (menu.aimbotBone == 1) targetPos.z += 50.0f;
                else                           targetPos.z += 25.0f;
            }

            Vector3 aimAngle;
            CalculateAngles(localEye, targetPos, aimAngle);

            float fov = GetFov(crosshair, aimAngle);
            if (fov >= maxFov) continue;

            float dist = GetDistance3D(localEye, targetPos);
            float score = CalculateTargetScore(fov, dist, player.health, maxFov);

            candidates.push_back({ aimAngle, fov, dist, score, player.health });
        }

        if (candidates.empty()) return false;

        // Select best target by composite score
        auto& best = *std::max_element(candidates.begin(), candidates.end(),
            [](const AimTarget& a, const AimTarget& b) { return a.score < b.score; });

        Vector3 bestAngle = best.angle;
        bestAngle.x -= currentPunch.x * 2.0f;
        bestAngle.y -= currentPunch.y * 2.0f;
        ClampAngles(bestAngle);

        float dx = NormalizePitch(bestAngle.x - viewAngles.x);
        float dy = NormalizeYaw(bestAngle.y - viewAngles.y);

        float smooth = GetAdaptiveSmooth(menu.aimbotSmoothness, best.fov, best.distance);

        // Cubic ease-out for natural mouse movement feel
        float totalDelta = sqrtf(dx * dx + dy * dy);
        if (totalDelta > 0.01f) {
            float t = 1.0f / smooth;
            float easedT = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);

            viewAngles.x += dx * easedT;
            viewAngles.y += dy * easedT;
            ClampAngles(viewAngles);

            kernel.WriteMemory<Vector3>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewAngles, viewAngles);
        }

        return true;
    }
}
