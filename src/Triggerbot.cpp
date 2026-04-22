#include "Triggerbot.hpp"
#include <thread>
#include <chrono>
#include <cmath>
#include <windows.h> // ضروري لاستخدام mouse_event
#include "../include/Offsets.hpp"
#include "ESP.hpp"

constexpr std::ptrdiff_t ENTITY_STRIDE = 0x78; 

namespace Triggerbot {

void Execute(KernelInterface& kernel, ULONG pid, uint64_t clientBase, uint64_t localPlayerPawn, EntityManager& entMgr, const Menu& menu) {
    if (!menu.triggerbotEnabled || !localPlayerPawn) return;

    // 1. قراءة الـ ID الخاص باللاعب الذي أمامنا في مؤشر التصويب (Crosshair)
    // تم التصحيح: مسمى الكلاس الصحيح هو C_CSPlayerPawn
    int crosshairId = kernel.ReadMemory<int>(pid, localPlayerPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iIDEntIndex);
    if (crosshairId <= 0) return;

    uint64_t entityList = kernel.ReadMemory<uint64_t>(pid, clientBase + cs2_dumper::offsets::client_dll::dwEntityList);
    if (!entityList) return;

    uint64_t listEntry = kernel.ReadMemory<uint64_t>(pid, entityList + 0x8 * ((crosshairId & 0x7FFF) >> 9) + 16);
    if (!listEntry) return;

    uint64_t targetPawn = kernel.ReadMemory<uint64_t>(pid, listEntry + ENTITY_STRIDE * (crosshairId & 0x1FF));
    if (!targetPawn || targetPawn == localPlayerPawn) return;

    int health = kernel.ReadMemory<int>(pid, targetPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
    int team = kernel.ReadMemory<uint8_t>(pid, targetPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
    int localTeam = kernel.ReadMemory<uint8_t>(pid, localPlayerPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);

    if (health > 0 && health <= 150 && team != localTeam) {
        
        // 2. فحص الرأس (Headshot Only) باستخدام الـ EntityManager الآمن
        if (menu.triggerbotHeadshotOnly) {
            bool isHead = false;
            for (const auto& player : entMgr.GetPlayers()) {
                if (player.pawn == targetPawn) {
                    if (player.boneArray && player.boneArray >= 0x1000000) {
                        Vector3 headPos = kernel.ReadMemory<Vector3>(pid, player.boneArray + Bones::Head * 32);
                        Matrix4x4 viewMatrix = kernel.ReadMemory<Matrix4x4>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewMatrix);
                        
                        float _x = viewMatrix.m[0] * headPos.x + viewMatrix.m[1] * headPos.y + viewMatrix.m[2] * headPos.z + viewMatrix.m[3];
                        float _y = viewMatrix.m[4] * headPos.x + viewMatrix.m[5] * headPos.y + viewMatrix.m[6] * headPos.z + viewMatrix.m[7];
                        float w = viewMatrix.m[12] * headPos.x + viewMatrix.m[13] * headPos.y + viewMatrix.m[14] * headPos.z + viewMatrix.m[15];

                        if (w > 0.01f) {
                            float inv_w = 1.0f / w;
                            _x *= inv_w;
                            _y *= inv_w;
                            float ndc_dist = std::sqrt(_x * _x + _y * _y);
                            
                            // 0.08f مساحة أفضل لتسجيل الرأس بدلاً من 0.05f التي كانت صارمة جداً
                            if (ndc_dist <= 0.08f) { 
                                isHead = true;
                            }
                        }
                    }
                    break;
                }
            }
            if (!isHead) return; // الهدف ليس في منطقة الرأس
        }

        // 3. تطبيق التأخير (Delay) بذكاء
        if (menu.triggerbotDelay > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(menu.triggerbotDelay));
            
            // فحص أمان: التأكد أن العدو لا يزال في المؤشر بعد انتهاء وقت التأخير!
            int currentCrosshairId = kernel.ReadMemory<int>(pid, localPlayerPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iIDEntIndex);
            if (currentCrosshairId != crosshairId) return; // العدو تحرك، نلغي الطلقة
        }

        // 4. محاكاة الضغط باستخدام Windows API لضمان عملها بشكل 100%
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0); // ضغط
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); // تثبيت الضغطة قليلاً لتسجلها اللعبة
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);   // إفلات الزر

        // 5. تأخير بسيط لمنع إطلاق النار العشوائي المستمر (Spam) وتوفير الرصاص
        std::this_thread::sleep_for(std::chrono::milliseconds(150)); 
    }
}

}
