#include "Triggerbot.hpp"
#include <thread>
#include <chrono>
#include <cmath>
#include "../include/Offsets.hpp"
#include "ESP.hpp"

// تم التصحيح: المسافة الصحيحة للكيانات في CS2 هي 120 بايت (0x78)
constexpr std::ptrdiff_t ENTITY_STRIDE = 0x78; 

namespace Triggerbot {

void Execute(KernelInterface& kernel, ULONG pid, uint64_t clientBase, uint64_t localPlayerPawn, EntityManager& entMgr, const Menu& menu) {
    if (!menu.triggerbotEnabled || !localPlayerPawn) return;

    int crosshairId = kernel.ReadMemory<int>(pid, localPlayerPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iIDEntIndex);
    if (crosshairId <= 0) return;

    uint64_t entityList = kernel.ReadMemory<uint64_t>(pid, clientBase + cs2_dumper::offsets::client_dll::dwEntityList);
    if (!entityList) return;

    uint64_t listEntry = kernel.ReadMemory<uint64_t>(pid, entityList + 0x8 * ((crosshairId & 0x7FFF) >> 9) + 16);
    if (!listEntry) return;

    // الآن سيتمكن من قراءة بيانات اللاعب المستهدف بشكل صحيح
    uint64_t targetPawn = kernel.ReadMemory<uint64_t>(pid, listEntry + ENTITY_STRIDE * (crosshairId & 0x1FF));
    if (!targetPawn || targetPawn == localPlayerPawn) return;

    int health = kernel.ReadMemory<int>(pid, targetPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
    int team = kernel.ReadMemory<uint8_t>(pid, targetPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
    int localTeam = kernel.ReadMemory<uint8_t>(pid, localPlayerPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);

    if (health > 0 && health <= 100 && team != localTeam) {
        
        // --- Headshot Only Check ---
        if (menu.triggerbotHeadshotOnly) {
            uint64_t gameSceneNode = kernel.ReadMemory<uint64_t>(pid, targetPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode);
            if (gameSceneNode) {
                uint64_t boneArray = kernel.ReadMemory<uint64_t>(pid, gameSceneNode + 0x1F8);
                if (!boneArray || boneArray < 0x1000000) {
                    boneArray = kernel.ReadMemory<uint64_t>(pid, gameSceneNode + 0x150 + 0x80);
                }
                
                if (boneArray && boneArray >= 0x1000000) {
                    Vector3 headPos = kernel.ReadMemory<Vector3>(pid, boneArray + Bones::Head * 32);
                    Matrix4x4 viewMatrix = kernel.ReadMemory<Matrix4x4>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewMatrix);
                    
                    float _x = viewMatrix.m[0] * headPos.x + viewMatrix.m[1] * headPos.y + viewMatrix.m[2] * headPos.z + viewMatrix.m[3];
                    float _y = viewMatrix.m[4] * headPos.x + viewMatrix.m[5] * headPos.y + viewMatrix.m[6] * headPos.z + viewMatrix.m[7];
                    float w = viewMatrix.m[12] * headPos.x + viewMatrix.m[13] * headPos.y + viewMatrix.m[14] * headPos.z + viewMatrix.m[15];

                    if (w > 0.01f) {
                        float inv_w = 1.0f / w;
                        _x *= inv_w;
                        _y *= inv_w;
                        
                        float ndc_dist = std::sqrt(_x * _x + _y * _y);
                        
                        if (ndc_dist > 0.05f) {
                            return; // الهدف بعيد عن الرأس
                        }
                    } else {
                        return; 
                    }
                } else {
                    return; 
                }
            } else {
                return; 
            }
        }
        // ---------------------------

        if (menu.triggerbotDelay > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(menu.triggerbotDelay));
        }

        kernel.LeftClick(); // محاكاة الضغط
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
    }
}

}
