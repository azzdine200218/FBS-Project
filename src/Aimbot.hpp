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

    // أضف هذا المتغير خارج الدالة لحفظ الارتداد السابق وتطبيق التعويض اللحظي
    static Vector3 aimbotPreviousPunch = {0, 0, 0};

    static bool Execute(KernelInterface& kernel, ULONG pid, uint64_t clientBase,
                        uint64_t localPawn, EntityManager& entMgr, const Menu& menu) {
        // تصفير الارتداد المحفوظ عند إيقاف التفعيل لتجنب القفزات العشوائية
        if (!menu.aimbotEnabled || !localPawn) {
            aimbotPreviousPunch = {0, 0, 0};
            return false;
        }
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) {
            aimbotPreviousPunch = {0, 0, 0};
            return false;
        }

        Vector3 localOrigin = kernel.ReadMemory<Vector3>(pid,
            localPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
        Vector3 localEye = { localOrigin.x, localOrigin.y, localOrigin.z + 64.0f };

        Vector3 viewAngles = kernel.ReadMemory<Vector3>(pid,
            clientBase + cs2_dumper::offsets::client_dll::dwViewAngles);

        const uint64_t m_aimPunchAngle = 0x14F4; 
        Vector3 aimPunch = kernel.ReadMemory<Vector3>(pid, localPawn + m_aimPunchAngle);

        int localTeam = kernel.ReadMemory<uint8_t>(pid,
            localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);

        // 1. التعويض اللحظي للارتداد (Instant RCS) بدون Smooth
        Vector3 punchDelta = {
            aimPunch.x - aimbotPreviousPunch.x,
            aimPunch.y - aimbotPreviousPunch.y,
            0.0f
        };
        aimbotPreviousPunch = aimPunch; // تحديث الارتداد القديم

        // تطبيق التعويض على زاوية الرؤية مباشرة (مثل ملف RCS.hpp)
        viewAngles.x -= punchDelta.x * 2.0f;
        viewAngles.y -= punchDelta.y * 2.0f;

        // تصحيح الزوايا لعدم الخروج عن النطاق المسموح
        if (viewAngles.x > 89.0f) viewAngles.x = 89.0f;
        if (viewAngles.x < -89.0f) viewAngles.x = -89.0f;
        while (viewAngles.y > 180.0f) viewAngles.y -= 360.0f;
        while (viewAngles.y < -180.0f) viewAngles.y += 360.0f;

        // 2. حساب موقع مؤشر التصويب الفعلي (Crosshair) بعد الارتداد
        // هذا مهم جداً لحساب الـ FOV وحساب مسافة السحب بشكل صحيح
        Vector3 crosshairAngles = {
            viewAngles.x + aimPunch.x * 2.0f,
            viewAngles.y + aimPunch.y * 2.0f,
            0.0f
        };

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
                int boneId = Bones::Head;
                if      (menu.aimbotBone == 1) boneId = Bones::Spine;
                else if (menu.aimbotBone == 2) boneId = Bones::Pelvis;

                targetPos = kernel.ReadMemory<Vector3>(pid, player.boneArray + boneId * 32);
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

            // حساب الـ FOV بناءً على موقع السلاح الفعلي (Crosshair) وليس منتصف الشاشة
            float fov = GetFov(crosshairAngles, aimAngle);

            if (fov < bestFov) {
                bestFov   = fov;
                bestAngle = aimAngle; // نحفظ زاوية الهدف الصافية
                hasTarget = true;
            }
        }

        // إذا لم نجد هدفاً في الشاشة، نقوم بكتابة الارتداد اللحظي (يعمل كأنه RCS عادي)
        if (!hasTarget) {
            kernel.WriteMemory<Vector3>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewAngles, viewAngles);
            return true; 
        }

        float smooth = menu.aimbotSmoothness;
        if (smooth < 1.0f) smooth = 1.0f;

        // 3. نحسب المسافة بين هدفنا وبين موقع السلاح الحالي، ونطبق النعومة (Smooth) عليها فقط
        float dx = bestAngle.x - crosshairAngles.x;
        float dy = bestAngle.y - crosshairAngles.y;
        
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

        kernel.WriteMemory<Vector3>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewAngles, finalAngles);

        return true; 
    }
}
