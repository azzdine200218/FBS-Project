#pragma once
#include <cmath>
#include <vector>
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

    static float GetFov(const Vector3& viewAngles, const Vector3& targetAngles) {
        float dx = targetAngles.x - viewAngles.x;
        float dy = targetAngles.y - viewAngles.y;
        while (dx >  89.0f) dx -= 180.0f;
        while (dx < -89.0f) dx += 180.0f;
        while (dy >  180.0f) dy -= 360.0f;
        while (dy < -180.0f) dy += 360.0f;
        return sqrtf(dx * dx + dy * dy);
    }

    static void ClampAngles(Vector3& angles) {
        if (std::isnan(angles.x) || std::isnan(angles.y)) angles = {0,0,0};
        if (angles.x > 89.0f) angles.x = 89.0f;
        if (angles.x < -89.0f) angles.x = -89.0f;
        while (angles.y > 180.0f) angles.y -= 360.0f;
        while (angles.y < -180.0f) angles.y += 360.0f;
        angles.z = 0.0f;
    }

    static Vector3 aimbotPreviousPunch = {0, 0, 0};

    static bool Execute(KernelInterface& kernel, ULONG pid, uint64_t clientBase,
                        uint64_t localPawn, EntityManager& entMgr, const Menu& menu) {
        
        if (!menu.aimbotEnabled || !localPawn) {
            aimbotPreviousPunch = {0, 0, 0};
            return false;
        }

        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) {
            aimbotPreviousPunch = {0, 0, 0};
            return false;
        }

        // قراءة الارتداد باستخدام الأوفست الديناميكي الصحيح
        uint64_t m_aimPunchAngle = cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_aimPunchAngle; 
        Vector3 currentPunch = kernel.ReadMemory<Vector3>(pid, localPawn + m_aimPunchAngle);

        Vector3 viewAngles = kernel.ReadMemory<Vector3>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewAngles);

        // 1. تطبيق التعويض اللحظي (Instant RCS) أولاً
        Vector3 punchDelta = {
            currentPunch.x - aimbotPreviousPunch.x,
            currentPunch.y - aimbotPreviousPunch.y,
            0.0f
        };
        aimbotPreviousPunch = currentPunch;

        viewAngles.x -= punchDelta.x * 2.0f;
        viewAngles.y -= punchDelta.y * 2.0f;
        ClampAngles(viewAngles);

        // 2. حساب موقع مؤشر السلاح الفعلي (Crosshair) للبحث عن الأهداف
        Vector3 crosshair = {
            viewAngles.x + currentPunch.x * 2.0f,
            viewAngles.y + currentPunch.y * 2.0f,
            0.0f
        };

        Vector3 localOrigin = kernel.ReadMemory<Vector3>(pid, localPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
        Vector3 localEye = { localOrigin.x, localOrigin.y, localOrigin.z + 64.0f };
        int localTeam = kernel.ReadMemory<uint8_t>(pid, localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);

        float    bestFov   = (float)menu.aimbotFov;
        Vector3  bestAngle = {0.0f, 0.0f, 0.0f};
        bool     hasTarget = false;

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

            if (fov < bestFov) {
                bestFov   = fov;
                bestAngle = aimAngle;
                hasTarget = true;
            }
        }

        // 3. التوجه بنعومة نحو الهدف (إن وُجد)
        if (hasTarget) {
            float smooth = menu.aimbotSmoothness < 1.0f ? 1.0f : menu.aimbotSmoothness;

            float dx = bestAngle.x - crosshair.x;
            float dy = bestAngle.y - crosshair.y;
            
            while (dy >  180.0f) dy -= 360.0f;
            while (dy < -180.0f) dy += 360.0f;

            viewAngles.x += dx / smooth;
            viewAngles.y += dy / smooth;
            ClampAngles(viewAngles);
        }

        kernel.WriteMemory<Vector3>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewAngles, viewAngles);
        return true; 
    }
}
