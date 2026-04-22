#pragma once
#include <cmath>
#include <windows.h>
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

        // التحقق من ضغط زر الماوس الأيسر
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) {
            previousPunch = {0, 0, 0};
            return;
        }

        // استخدام الـ Offset الديناميكي من الـ Dumper بدلاً من رقم ثابت
        uint64_t m_aimPunchAngle = cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_aimPunchAngle;
        Vector3 currentPunch = kernel.ReadMemory<Vector3>(pid, localPawn + m_aimPunchAngle);

        Vector3 delta = {
            currentPunch.x - previousPunch.x,
            currentPunch.y - previousPunch.y,
            0.0f
        };

        previousPunch = currentPunch;

        if (delta.x == 0.0f && delta.y == 0.0f) {
            return;
        }

        Vector3 viewAngles = kernel.ReadMemory<Vector3>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewAngles);

        viewAngles.x -= delta.x * config.vertical * 2.0f;
        viewAngles.y -= delta.y * config.horizontal * 2.0f;

        // حماية الزوايا من التلف (Untrusted Clamp)
        if (std::isnan(viewAngles.x) || std::isnan(viewAngles.y)) return;
        if (viewAngles.x > 89.0f) viewAngles.x = 89.0f;
        if (viewAngles.x < -89.0f) viewAngles.x = -89.0f;
        while (viewAngles.y > 180.0f) viewAngles.y -= 360.0f;
        while (viewAngles.y < -180.0f) viewAngles.y += 360.0f;

        kernel.WriteMemory<Vector3>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewAngles, viewAngles);
    }
}
