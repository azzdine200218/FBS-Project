#pragma once
#include <cmath>
#include "KernelInterface.hpp"
#include "../include/Offsets.hpp"

struct RCSConfig {
    bool enabled = true;
    float horizontal = 1.0f;
    float vertical = 1.0f;
};

namespace RCS {
    static Vector3 previousPunch = {0, 0, 0};

    static void Execute(KernelInterface& kernel, ULONG pid, uint64_t localPawn, uint64_t clientBase, const RCSConfig& config) {
        if (!config.enabled || !localPawn || !clientBase) {
            previousPunch = {0, 0, 0};
            return;
        }

        // Manual offset for current build since dumper missed it in schema
        const uint64_t m_aimPunchAngle = 0x14F4; 
        Vector3 currentPunch = kernel.ReadMemory<Vector3>(pid, localPawn + m_aimPunchAngle);

        if (currentPunch.x == 0 && currentPunch.y == 0 && currentPunch.z == 0) {
            previousPunch = {0, 0, 0};
            return;
        }

        Vector3 delta = {
            currentPunch.x - previousPunch.x,
            currentPunch.y - previousPunch.y,
            currentPunch.z - previousPunch.z
        };

        previousPunch = currentPunch;

        Vector3 viewAngles = kernel.ReadMemory<Vector3>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewAngles);

        viewAngles.x -= delta.x * config.vertical * 2.0f;
        viewAngles.y -= delta.y * config.horizontal * 2.0f;

        if (viewAngles.x > 89.0f) viewAngles.x = 89.0f;
        if (viewAngles.x < -89.0f) viewAngles.x = -89.0f;
        if (viewAngles.y > 180.0f) viewAngles.y -= 360.0f;
        if (viewAngles.y < -180.0f) viewAngles.y += 360.0f;

        kernel.WriteMemory<Vector3>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewAngles, viewAngles);
    }
}
