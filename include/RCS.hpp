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

        int32_t shotsFired = kernel.ReadMemory<int32_t>(pid, localPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iShotsFired);
        Vector3 currentPunch = {0, 0, 0};

        // نقرأ الارتداد الفعلي فقط في حال إطلاق أكثر من رصاصة (لتجنب الاهتزاز مع النقرات الفردية)
        // وإذا توقفنا عن الإطلاق، سيصبح currentPunch أصفاراً مما يؤدي لإرجاع الماوس لمكانه الطبيعي بنعومة.
        if (shotsFired > 1) {
            currentPunch = kernel.ReadMemory<Vector3>(pid, localPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_aimPunchAngle);
        }

        Vector3 delta = {
            currentPunch.x - previousPunch.x,
            currentPunch.y - previousPunch.y,
            0.0f
        };

        previousPunch = currentPunch;

        if (delta.x == 0.0f && delta.y == 0.0f) {
            return; // لا يوجد تغير في الارتداد، لذا لا نكتب على الذاكرة لتخفيف الضغط
        }

        Vector3 viewAngles = kernel.ReadMemory<Vector3>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewAngles);

        viewAngles.x -= delta.x * config.vertical * 2.0f;
        viewAngles.y -= delta.y * config.horizontal * 2.0f;

        // حماية الزوايا (Untrusted Clamp)
        if (std::isnan(viewAngles.x) || std::isnan(viewAngles.y)) return;
        if (viewAngles.x > 89.0f) viewAngles.x = 89.0f;
        if (viewAngles.x < -89.0f) viewAngles.x = -89.0f;
        while (viewAngles.y > 180.0f) viewAngles.y -= 360.0f;
        while (viewAngles.y < -180.0f) viewAngles.y += 360.0f;

        kernel.WriteMemory<Vector3>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewAngles, viewAngles);
    }
}
