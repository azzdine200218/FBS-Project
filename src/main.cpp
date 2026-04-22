#include <windows.h>
#include <thread>
#include <chrono>
#include <vector>
#include "../include/ProcessGuard.hpp"
#include "../include/KernelInterface.hpp"
#include "../include/Memory.hpp"
#include "../include/Offsets.hpp"
#include "../include/Data.hpp"
#include "../include/XorStr.hpp"
#include "../include/EntityManager.hpp"
#include "../include/RCS.hpp"
#include "Overlay.hpp"
#include "Menu.hpp"
#include "ESP.hpp"
#include "Triggerbot.hpp"
#include "BunnyHop.hpp"
#include "Aimbot.hpp"
#include "KeyBind.hpp"
#include "ConfigSystem.hpp"
#include "WeaponNames.hpp"
#include "../include/imgui/imgui.h"

using namespace offsets;

int main() {
    KernelInterface kernel;
    if (!kernel.IsConnected()) {
        system(XOR_STR("pause"));
        return 1;
    }

    // Stealth: Hide this process handles immediately
    kernel.ProtectProcess(GetCurrentProcessId());

    Memory mem(kernel);
    while (!mem.Initialize(XOR_WSTR("cs2.exe"))) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    Overlay overlay;
    if (!overlay.Initialize()) {
        system(XOR_STR("pause"));
        return 1;
    }

    Menu menu;
    KeyBindManager keybinds;
    BunnyHop bhop;

    menu.LoadConfig("default");

    static bool running = true;

    // Stealth: Hide console after all init succeeded
    ProcessGuard::HideConsole();
    ULONG pid = mem.GetProcessId();
    ULONGLONG clientBase = mem.GetClientBase();

    bhop.Initialize(kernel, pid, clientBase);

    std::thread bhopThread([&]() {
        while (running) {
            if (menu.bhopEnabled) {
                bhop.SetClientBase(clientBase);
                bhop.SetStrafeAssist(menu.bhopStrafeAssist);
                ULONGLONG localPlayerPawn = kernel.ReadMemory<ULONGLONG>(pid, clientBase + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
                bhop.UpdateLocalPlayer(localPlayerPawn);
                bhop.Execute();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    bhopThread.detach();

    // === ENTITY MANAGER ===
    EntityManager entityManager;
    entityManager.Initialize(kernel, pid, clientBase);

    std::thread espUpdateThread([&]() {
        while (running) {
            clientBase = mem.GetClientBase();
            pid = mem.GetProcessId();
            if (clientBase != 0 && pid != 0) {
                entityManager.Initialize(kernel, pid, clientBase);
                entityManager.UpdateEntities(64);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Update entities at ~200Hz max
        }
    });
    espUpdateThread.detach();

    std::thread triggerbotThread([&]() {
        while (running) {
            if (menu.triggerbotEnabled) {
                ULONGLONG localPlayerPawn = kernel.ReadMemory<ULONGLONG>(pid, clientBase + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
                if (localPlayerPawn != 0) {
                    Triggerbot::Execute(kernel, pid, clientBase, localPlayerPawn, entityManager, menu);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    triggerbotThread.detach();

    std::thread aimRcsThread([&]() {
        while (running) {
            ULONGLONG localPlayerPawn = kernel.ReadMemory<ULONGLONG>(pid, clientBase + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
            if (localPlayerPawn != 0) {
                bool aimbotLocked = false;
                if (menu.aimbotEnabled) {
                    aimbotLocked = Aimbot::Execute(kernel, pid, clientBase, localPlayerPawn, entityManager, menu);
                }
                
                // Only run RCS if aimbot is NOT writing angles this frame, to prevent conflict
                if (menu.rcsEnabled && !aimbotLocked) {
                    RCSConfig rcsConfig;
                    rcsConfig.enabled = menu.rcsEnabled;
                    rcsConfig.horizontal = menu.rcsHorizontal;
                    rcsConfig.vertical = menu.rcsVertical;
                    RCS::Execute(kernel, pid, localPlayerPawn, clientBase, rcsConfig);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    aimRcsThread.detach();

    while (overlay.RenderBegin()) {
        keybinds.CheckForKeyPress();

        clientBase = mem.GetClientBase();
        pid = mem.GetProcessId();

        if (clientBase == 0 || pid == 0) {
            overlay.RenderEnd();
            continue;
        }

        // Local player info
        Matrix4x4 viewMatrix = kernel.ReadMemory<Matrix4x4>(pid, clientBase + dwViewMatrix);
        ULONGLONG localPlayerPawn = kernel.ReadMemory<ULONGLONG>(pid, clientBase + dwLocalPlayerPawn);
        int localTeam = 0;
        Vector3 localOrigin = {0, 0, 0};

        static int previousObserverModeState = 0;

        if (localPlayerPawn != 0) {
            localTeam = kernel.ReadMemory<uint8_t>(pid, localPlayerPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
            localOrigin = kernel.ReadMemory<Vector3>(pid, localPlayerPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);

            // Third Person feature
            uint32_t health = kernel.ReadMemory<int32_t>(pid, localPlayerPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
            if (health > 0) {
                ULONGLONG observerServices = kernel.ReadMemory<ULONGLONG>(pid, localPlayerPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pObserverServices); // Use schema offset
                if (observerServices) {
                    if (menu.thirdPersonEnabled) {
                        kernel.WriteMemory<uint8_t>(pid, observerServices + 0x48, 5); // 5 = OBS_MODE_CHASE
                        kernel.WriteMemory<bool>(pid, observerServices + 0x54, true); // m_bForcedObserverMode = true
                        previousObserverModeState = 1;
                    } else if (previousObserverModeState == 1) {
                        kernel.WriteMemory<bool>(pid, observerServices + 0x54, false); // m_bForcedObserverMode = false
                        kernel.WriteMemory<uint8_t>(pid, observerServices + 0x48, 0); // Restore First Person
                        previousObserverModeState = 0;
                    }
                }
            }
        }

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();

        // === ESP LOOP (Uses Cached Data from EntityManager) ===
        if (menu.showEspBoxes || menu.showEspHealth || menu.showEspNames ||
            menu.showEspDistance || menu.showEspMoney || menu.showEspArmor ||
            menu.showSnaplines || menu.showHeadDots || menu.showFilledBoxes || menu.showVisibleCheck ||
            menu.showSkeleton) {

            for (const auto& player : entityManager.GetPlayers()) {
                if (!menu.showTeammates && localTeam != 0 && player.team == localTeam) continue;

                Vector2 sPos, sHead;
                if (ESP::WorldToScreen(player.origin, sPos, viewMatrix, overlay.screenWidth, overlay.screenHeight) &&
                    ESP::WorldToScreen(player.headPos, sHead, viewMatrix, overlay.screenWidth, overlay.screenHeight)) {

                    float height = sPos.y - sHead.y;
                    float width = height / 2.0f;
                    float x = sPos.x - width / 2.0f;
                    float y = sHead.y;

                    if (menu.showFilledBoxes) ESP::DrawFilledBox(x, y, width, height, player.team == 2 ? ImColor(255, 50, 50, 120) : ImColor(50, 100, 255, 120), player.team == 2 ? ImColor(255, 50, 50, 200) : ImColor(50, 100, 255, 200));
                    if (menu.showEspBoxes) {
                        if (menu.boxType == 0) ESP::DrawCornerBox(x, y, width, height, player.team == 2 ? ImColor(255, 50, 50, 200) : ImColor(50, 100, 255, 200));
                        else if (menu.boxType == 2) ESP::DrawCornerBox(x, y, width, height, player.team == 2 ? ImColor(255, 50, 50, 200) : ImColor(50, 100, 255, 200));
                    }
                    if (menu.showEspHealth) ESP::DrawHealthBar(x - 6, y, height, player.health);
                    if (menu.showEspArmor && player.armor > 0) ESP::DrawArmorBar(x + width + 6, y, height, player.armor);
                    if (menu.showEspNames) {
                        std::string name = (player.playerName[0] != '\0') ? player.playerName : "Unknown";
                        ESP::DrawInfo(sPos.x, y - 14, name.c_str(), ImColor(255, 255, 255, 200));
                    }
                    if (menu.showEspDistance) {
                        float dist = sqrtf(powf(player.origin.x - localOrigin.x, 2) + powf(player.origin.y - localOrigin.y, 2) + powf(player.origin.z - localOrigin.z, 2)) * 0.0254f;
                        char distStr[32];
                        sprintf_s(distStr, "%.1fm", dist);
                        ESP::DrawInfo(sPos.x, sPos.y + 4, distStr, ImColor(200, 200, 200, 200));
                    }
                    if (menu.showEspMoney && player.moneyServices) {
                        int money = kernel.ReadMemory<int>(pid, player.moneyServices + cs2_dumper::schemas::client_dll::CCSPlayerController_InGameMoneyServices::m_iAccount);
                        char moneyStr[32];
                        sprintf_s(moneyStr, "$%d", money);
                        ESP::DrawInfo(sPos.x, y - 28, moneyStr, ImColor(100, 255, 100, 200));
                    }
                    if (menu.showSnaplines) drawList->AddLine(ImVec2(overlay.screenWidth / 2.0f, (float)overlay.screenHeight), ImVec2(sPos.x, sPos.y), ImColor(255, 255, 255, 100), 1.0f);
                    if (menu.showHeadDots) drawList->AddCircleFilled(ImVec2(sHead.x, sHead.y), 3.0f, ImColor(255, 50, 50, 200));
                    if (menu.showVisibleCheck) ESP::DrawVisibleCheck(sPos.x, y - 42, true);
                }

                // Skeleton ESP — uses cached pawn to draw bones
                if (menu.showSkeleton && player.pawn) {
                    ESP::DrawSkeleton(kernel, pid, player.pawn, viewMatrix, overlay.screenWidth, overlay.screenHeight);
                }
            }
        }

        // === DROPPED WEAPONS ESP ===
        static std::vector<std::pair<std::string, Vector3>> cachedWeapons;
        static auto lastWeaponUpdate = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();

        if (menu.showDroppedWeapons) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastWeaponUpdate).count() > 500) {
                cachedWeapons.clear();
                ULONGLONG entityList = kernel.ReadMemory<ULONGLONG>(pid, clientBase + dwEntityList);
                if (entityList != 0) {
                    for (int i = 64; i < 512; i++) {
                        ULONGLONG chunkBase = kernel.ReadMemory<ULONGLONG>(pid, entityList + (ULONGLONG)(8 * ((i & 0x7FFF) >> 9) + 16));
                        if (!chunkBase) continue;
                        ULONGLONG entity = kernel.ReadMemory<ULONGLONG>(pid, chunkBase + (ULONGLONG)(ENTITY_LIST_ENTRY_STRIDE * (i & 0x1FF)));
                        if (!entity) continue;
                        uint32_t ownerHandle = kernel.ReadMemory<uint32_t>(pid, entity + cs2_dumper::schemas::client_dll::C_BaseEntity::m_hOwnerEntity);
                        if (ownerHandle != 0xFFFFFFFF) continue;
                        uint64_t entityIdentity = kernel.ReadMemory<uint64_t>(pid, entity + cs2_dumper::schemas::client_dll::CEntityInstance::m_pEntity);
                        if (!entityIdentity) continue;
                        uint64_t designerNamePtr = kernel.ReadMemory<uint64_t>(pid, entityIdentity + cs2_dumper::schemas::client_dll::CEntityIdentity::m_designerName);
                        if (designerNamePtr < 0x10000 || designerNamePtr > 0x7FFFFFFFFFFF) continue;
                        char nameBuf[32] = {};
                        kernel.ReadMemoryBlock(pid, designerNamePtr, nameBuf, 32);
                        std::string designerName(nameBuf);
                        std::string wName = GetWeaponNameFromDesigner(designerName);
                        if (wName.empty()) continue;
                        uint64_t sceneNode = kernel.ReadMemory<uint64_t>(pid, entity + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode);
                        Vector3 origin;
                        if (sceneNode) origin = kernel.ReadMemory<Vector3>(pid, sceneNode + cs2_dumper::schemas::client_dll::CGameSceneNode::m_vecAbsOrigin);
                        else origin = kernel.ReadMemory<Vector3>(pid, entity + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
                        
                        cachedWeapons.push_back({wName, origin});
                    }
                }
                lastWeaponUpdate = now;
            }

            for (const auto& w : cachedWeapons) {
                Vector2 sPos;
                if (ESP::WorldToScreen(w.second, sPos, viewMatrix, overlay.screenWidth, overlay.screenHeight)) {
                    ESP::DrawInfo(sPos.x, sPos.y, w.first.c_str(), ImColor(255, 200, 50, 200));
                }
            }
        }

        // === C4 BOMB ESP ===
        if (menu.showBombEsp) {
            ULONGLONG entityList = kernel.ReadMemory<ULONGLONG>(pid, clientBase + dwEntityList);
            if (entityList != 0) {
                // 1. Planted C4
                for (int i = 64; i < 512; i++) {
                    ULONGLONG chunkBase = kernel.ReadMemory<ULONGLONG>(pid, entityList + (ULONGLONG)(8 * ((i & 0x7FFF) >> 9) + 16));
                    if (!chunkBase) continue;
                    ULONGLONG entity = kernel.ReadMemory<ULONGLONG>(pid, chunkBase + (ULONGLONG)(ENTITY_LIST_ENTRY_STRIDE * (i & 0x1FF)));
                    if (!entity) continue;

                    uint64_t entityIdentity = kernel.ReadMemory<uint64_t>(pid, entity + 0x10);
                    if (!entityIdentity) continue;
                    uint64_t designerNamePtr = kernel.ReadMemory<uint64_t>(pid, entityIdentity + 0x20);
                    if (designerNamePtr < 0x10000 || designerNamePtr > 0x7FFFFFFFFFFF) continue;

                    char nameBuf[32] = {};
                    kernel.ReadMemoryBlock(pid, designerNamePtr, nameBuf, 31);
                    std::string entityName(nameBuf);

                    if (entityName.find("planted_c4") != std::string::npos) {
                        uint64_t c4SceneNode = kernel.ReadMemory<uint64_t>(pid, entity + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode);
                        Vector3 bombOrigin;
                        if (c4SceneNode) {
                            bombOrigin = kernel.ReadMemory<Vector3>(pid, c4SceneNode + cs2_dumper::schemas::client_dll::CGameSceneNode::m_vecAbsOrigin);
                        } else {
                            bombOrigin = kernel.ReadMemory<Vector3>(pid, entity + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
                        }

                        Vector2 sBomb;
                        if (ESP::WorldToScreen(bombOrigin, sBomb, viewMatrix, overlay.screenWidth, overlay.screenHeight)) {
                            drawList->AddCircle(ImVec2(sBomb.x, sBomb.y), 12.0f, ImColor(255, 50, 50, 220), 16, 2.5f);
                            drawList->AddCircleFilled(ImVec2(sBomb.x, sBomb.y), 6.0f, ImColor(255, 255, 255, 100));
                            ESP::DrawInfo(sBomb.x - 30.0f, sBomb.y - 20.0f, "C4 PLANTED", ImColor(255, 100, 100, 255));

                            float bombTimer = kernel.ReadMemory<float>(pid, entity + cs2_dumper::schemas::client_dll::C_PlantedC4::m_flC4Blow);
                            if (bombTimer > 0) {
                                ESP::DrawC4Timer(sBomb.x - 25.0f, sBomb.y + 18.0f, bombTimer, ImColor(255, 200, 50, 255));
                            }
                        }
                    }
                }

                // 2. C4 Carrier - check each player's active weapon
                for (const auto& player : entityManager.GetPlayers()) {
                    if (!player.pawn) continue;
                    uint64_t weaponServices = kernel.ReadMemory<uint64_t>(pid, player.pawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pWeaponServices);
                    if (!weaponServices) continue;
                    uint32_t activeWeaponHandle = kernel.ReadMemory<uint32_t>(pid, weaponServices + cs2_dumper::schemas::client_dll::CPlayer_WeaponServices::m_hActiveWeapon);
                    if (!activeWeaponHandle || activeWeaponHandle == 0xFFFFFFFF) continue;

                    ULONGLONG weaponChunk = kernel.ReadMemory<ULONGLONG>(pid, entityList + (ULONGLONG)(8 * ((activeWeaponHandle & 0x7FFF) >> 9) + 16));
                    if (!weaponChunk) continue;
                    ULONGLONG activeWeapon = kernel.ReadMemory<ULONGLONG>(pid, weaponChunk + (ULONGLONG)(ENTITY_LIST_ENTRY_STRIDE * (activeWeaponHandle & 0x1FF)));
                    if (!activeWeapon) continue;

                    uint64_t weaponIdentity = kernel.ReadMemory<uint64_t>(pid, activeWeapon + 0x10);
                    if (!weaponIdentity) continue;
                    uint64_t weaponNamePtr = kernel.ReadMemory<uint64_t>(pid, weaponIdentity + 0x20);
                    if (weaponNamePtr < 0x10000 || weaponNamePtr > 0x7FFFFFFFFFFF) continue;

                    char wNameBuf[32] = {};
                    kernel.ReadMemoryBlock(pid, weaponNamePtr, wNameBuf, 31);
                    std::string weaponName(wNameBuf);

                    if (weaponName.find("weapon_c4") != std::string::npos) {
                        Vector2 sCarrier;
                        if (ESP::WorldToScreen(player.origin, sCarrier, viewMatrix, overlay.screenWidth, overlay.screenHeight)) {
                            drawList->AddCircle(ImVec2(sCarrier.x, sCarrier.y), 12.0f, ImColor(255, 165, 0, 220), 16, 2.5f);
                            drawList->AddCircleFilled(ImVec2(sCarrier.x, sCarrier.y), 6.0f, ImColor(255, 255, 255, 100));
                            ESP::DrawInfo(sCarrier.x - 30.0f, sCarrier.y - 20.0f, "C4 CARRIER", ImColor(255, 165, 0, 255));
                        }
                    }
                }
            }
        }

        menu.Draw(overlay);

        overlay.RenderEnd();

        std::this_thread::yield();
    }

    running = false;
    overlay.Terminate();
    return 0;
}



