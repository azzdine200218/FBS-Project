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
        // إذا كان مطفأ أو البيانات غير صالحة، قم بتصفير الارتداد المحفوظ
        if (!config.enabled || !localPawn || !clientBase) {
            previousPunch = {0, 0, 0};
            return;
        }

        // 1. الأمان الأول: التحقق من أن زر الإطلاق مضغوط فعلاً
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) {
            previousPunch = {0, 0, 0};
            return;
        }

        // 2. الأمان الثاني: التحقق من عدد الرصاصات المطلقة (لمنع تفاعل RCS عند تلقي ضرر)
        int32_t shotsFired = kernel.ReadMemory<int32_t>(pid, localPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iShotsFired);
        if (shotsFired <= 1) { // الرصاصة الأولى لا تحتاج إلى تعويض عادةً
            previousPunch = {0, 0, 0};
            return;
        }

        const uint64_t m_aimPunchAngle = 0x14F4; // تأكد من أن هذا الـ Offset يطابق التحديث الحالي
        Vector3 currentPunch = kernel.ReadMemory<Vector3>(pid, localPawn + m_aimPunchAngle);

        // حساب الدلتا (الفرق بين الارتداد السابق والحالي)
        Vector3 delta = {
            currentPunch.x - previousPunch.x,
            currentPunch.y - previousPunch.y,
            0.0f
        };

        // تحديث الارتداد للفريم القادم
        previousPunch = currentPunch;

        // لتوفير موارد المعالج، إذا لم يتغير الارتداد لا نكتب على الذاكرة
        if (delta.x == 0.0f && delta.y == 0.0f) {
            return;
        }

        Vector3 viewAngles = kernel.ReadMemory<Vector3>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewAngles);

        // تطبيق التعويض (ضرب 2.0 لأن محرك Source Engine يعرض الارتداد مضاعفاً)
        viewAngles.x -= delta.x * config.vertical * 2.0f;
        viewAngles.y -= delta.y * config.horizontal * 2.0f;

        // تصحيح زوايا الكاميرا (Clamping) لعدم تجاوز الحدود الطبيعية والمخاطرة بالحظر (Untrusted Ban)
        if (viewAngles.x > 89.0f) viewAngles.x = 89.0f;
        if (viewAngles.x < -89.0f) viewAngles.x = -89.0f;
        while (viewAngles.y > 180.0f) viewAngles.y -= 360.0f;
        while (viewAngles.y < -180.0f) viewAngles.y += 360.0f;

        // كتابة الزوايا الجديدة
        kernel.WriteMemory<Vector3>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewAngles, viewAngles);
    }
}
