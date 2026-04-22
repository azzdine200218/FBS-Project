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

        if (dx >  89.0f) dx =  89.0f;
        if (dx < -89.0f) dx = -89.0f;
        while (dy >  180.0f) dy -= 360.0f;
        while (dy < -180.0f) dy += 360.0f;

        return sqrtf(dx * dx + dy * dy);
    }

    // Returns true if aimbot is actively writing angles (so RCS skips its write that frame)
    static bool Execute(KernelInterface& kernel, ULONG pid, uint64_t clientBase,
                        uint64_t localPawn, EntityManager& entMgr, const Menu& menu) {
        if (!menu.aimbotEnabled || !localPawn) return false;
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) return false;

        // Local eye position: origin + 64 unit standing eye height
        Vector3 localOrigin = kernel.ReadMemory<Vector3>(pid,
            localPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
        Vector3 localEye = { localOrigin.x, localOrigin.y, localOrigin.z + 64.0f };

        Vector3 viewAngles = kernel.ReadMemory<Vector3>(pid,
            clientBase + cs2_dumper::offsets::client_dll::dwViewAngles);

        int localTeam = kernel.ReadMemory<uint8_t>(pid,
            localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);

        float    bestFov   = (float)menu.aimbotFov;
        Vector3  bestAngle = {0.0f, 0.0f, 0.0f};
        bool     hasTarget = false;

        for (const auto& player : entMgr.GetPlayers()) {
            if (!player.pawn || player.pawn == localPawn) continue;
            if (!menu.aimbotTargetTeammates && player.team == localTeam) continue;
            if (player.health <= 0) continue;

            Vector3 targetPos;
            bool useBackup = true;

            if (player.boneArray && player.boneArray >= 0x1000000) {
                // Use real bone positions for accurate pitch (Z axis matters!)
                int boneId = Bones::Head;
                if      (menu.aimbotBone == 1) boneId = Bones::Spine;
                else if (menu.aimbotBone == 2) boneId = Bones::Pelvis;

                targetPos = kernel.ReadMemory<Vector3>(pid, player.boneArray + boneId * 32);
                // Sanity check – bone must be non-zero
                if (targetPos.x != 0.0f || targetPos.y != 0.0f || targetPos.z != 0.0f)
                    useBackup = false;
            }

            if (useBackup) {
                targetPos = player.origin;
                if      (menu.aimbotBone == 0) targetPos.z += 70.0f;
                else if (menu.aimbotBone == 1) targetPos.z += 50.0f;
                else                           targetPos.z += 25.0f;
            }

            Vector3 aimAngle;
            CalculateAngles(localEye, targetPos, aimAngle);
            float fov = GetFov(viewAngles, aimAngle);

            if (fov < bestFov) {
                bestFov   = fov;
                bestAngle = aimAngle;
                hasTarget = true;
            }
        }

        if (!hasTarget) return false;

        float smooth = menu.aimbotSmoothness;
        if (smooth < 1.0f) smooth = 1.0f;

        float dx = bestAngle.x - viewAngles.x;
        float dy = bestAngle.y - viewAngles.y;
        while (dy >  180.0f) dy -= 360.0f;
        while (dy < -180.0f) dy += 360.0f;

        Vector3 finalAngles = {
            viewAngles.x + dx / smooth,
            viewAngles.y + dy / smooth,
            0.0f
        };

        if (finalAngles.x >  89.0f) finalAngles.x =  89.0f;
        if (finalAngles.x < -89.0f) finalAngles.x = -89.0f;
        while (finalAngles.y >  180.0f) finalAngles.y -= 360.0f;
        while (finalAngles.y < -180.0f) finalAngles.y += 360.0f;

        kernel.WriteMemory<Vector3>(pid,
            clientBase + cs2_dumper::offsets::client_dll::dwViewAngles, finalAngles);

        return true; // Prevent RCS from overwriting angles this frame
    }
}
