#pragma once
#include <windows.h>
#include <cstdint>
#include "../include/KernelInterface.hpp"
#include "../include/Data.hpp"
#include "../include/Offsets.hpp"

class EntityIterator {
private:
    KernelInterface* kernel;
    ULONG pid;
    ULONGLONG entityList;

public:
    struct PlayerInfo {
        ULONGLONG controller;
        ULONGLONG pawn;
        int health;
        int team;
        Vector3 origin;
        Vector3 headPos;
        bool isValid;

        PlayerInfo() : controller(0), pawn(0), health(0), team(0), origin{0,0,0}, headPos{0,0,0}, isValid(false) {}
    };

    struct DroppedWeaponInfo {
        ULONGLONG entity;
        std::string weaponName;
        Vector3 origin;
        bool isValid;

        DroppedWeaponInfo() : entity(0), origin{0,0,0}, isValid(false) {}
    };

    struct BombInfo {
        ULONGLONG entity;
        Vector3 origin;
        bool isPlanted;
        float countdown;
        bool isValid;

        BombInfo() : entity(0), origin{0,0,0}, isPlanted(false), countdown(0), isValid(false) {}
    };

    EntityIterator() : kernel(nullptr), pid(0), entityList(0) {}

    void Initialize(KernelInterface& k, ULONG p, ULONGLONG entList) {
        kernel = &k;
        pid = p;
        entityList = entList;
    }

    PlayerInfo GetPlayerInfo(int index) {
        PlayerInfo info;
        if (!entityList || index < 1 || index >= 64) return info;

        ULONGLONG chunkBase = kernel->ReadMemory<ULONGLONG>(pid, entityList + (ULONGLONG)(8 * ((index & 0x7FFF) >> 9) + 16));
        if (!chunkBase) return info;

        ULONGLONG controller = kernel->ReadMemory<ULONGLONG>(pid, chunkBase + (ULONGLONG)(112 * (index & 0x1FF)));
        if (!controller) return info;

        ULONG pawnHandle = kernel->ReadMemory<ULONG>(pid, controller + cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn);
        if (!pawnHandle || pawnHandle == 0xFFFFFFFF) return info;

        ULONGLONG pawnChunkBase = kernel->ReadMemory<ULONGLONG>(pid, entityList + (ULONGLONG)(8 * ((pawnHandle & 0x7FFF) >> 9) + 16));
        if (!pawnChunkBase) return info;

        ULONGLONG pawn = kernel->ReadMemory<ULONGLONG>(pid, pawnChunkBase + (ULONGLONG)(112 * (pawnHandle & 0x1FF)));
        if (!pawn) return info;

        int health = kernel->ReadMemory<int>(pid, pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
        if (health <= 0 || health > 100) return info;

        int team = kernel->ReadMemory<uint8_t>(pid, pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
        Vector3 origin = kernel->ReadMemory<Vector3>(pid, pawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
        Vector3 headPos = origin;
        headPos.z += 65.0f;

        info.controller = controller;
        info.pawn = pawn;
        info.health = health;
        info.team = team;
        info.origin = origin;
        info.headPos = headPos;
        info.isValid = true;
        return info;
    }

    DroppedWeaponInfo GetDroppedWeaponInfo(int index) {
        DroppedWeaponInfo info;
        if (!entityList || index < 64 || index >= 1024) return info;

        ULONGLONG chunkBase = kernel->ReadMemory<ULONGLONG>(pid, entityList + (ULONGLONG)(8 * ((index & 0x7FFF) >> 9) + 16));
        if (!chunkBase) return info;

        ULONGLONG entity = kernel->ReadMemory<ULONGLONG>(pid, chunkBase + (ULONGLONG)(112 * (index & 0x1FF)));
        if (!entity) return info;

        uint32_t ownerHandle = kernel->ReadMemory<uint32_t>(pid, entity + cs2_dumper::schemas::client_dll::C_BaseEntity::m_hOwnerEntity);
        if (ownerHandle != 0xFFFFFFFF) return info;

        uint64_t entityIdentity = kernel->ReadMemory<uint64_t>(pid, entity + 0x10);
        if (!entityIdentity) return info;

        uint64_t designerNamePtr = kernel->ReadMemory<uint64_t>(pid, entityIdentity + 0x20);
        if (designerNamePtr < 0x10000 || designerNamePtr > 0x7FFFFFFFFFFF) return info;

        char nameBuf[32] = {};
        kernel->ReadMemoryBlock(pid, designerNamePtr, nameBuf, 32);
        std::string designerName(nameBuf);

        extern std::string GetWeaponNameFromDesigner(const std::string&);
        std::string wName = GetWeaponNameFromDesigner(designerName);
        if (wName.empty()) return info;

        uint64_t sceneNode = kernel->ReadMemory<uint64_t>(pid, entity + 0x338);
        Vector3 origin;
        if (sceneNode) {
            origin = kernel->ReadMemory<Vector3>(pid, sceneNode + 0xD0);
        } else {
            origin = kernel->ReadMemory<Vector3>(pid, entity + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
        }

        info.entity = entity;
        info.weaponName = wName;
        info.origin = origin;
        info.isValid = true;
        return info;
    }

    BombInfo GetBombInfo() {
        BombInfo info;
        if (!entityList) return info;

        for (int i = 64; i < 512; i++) {
            ULONGLONG chunkBase = kernel->ReadMemory<ULONGLONG>(pid, entityList + (ULONGLONG)(8 * ((i & 0x7FFF) >> 9) + 16));
            if (!chunkBase) continue;

            ULONGLONG entity = kernel->ReadMemory<ULONGLONG>(pid, chunkBase + (ULONGLONG)(112 * (i & 0x1FF)));
            if (!entity) continue;

            uint64_t entityIdentity = kernel->ReadMemory<uint64_t>(pid, entity + 0x10);
            if (!entityIdentity) continue;

            uint64_t designerNamePtr = kernel->ReadMemory<uint64_t>(pid, entityIdentity + 0x20);
            if (designerNamePtr < 0x10000 || designerNamePtr > 0x7FFFFFFFFFFF) continue;

            char nameBuf[32] = {};
            kernel->ReadMemoryBlock(pid, designerNamePtr, nameBuf, 32);
            std::string name(nameBuf);

            if (name.find("planted_c4") != std::string::npos || name.find("weapon_c4") != std::string::npos) {
                info.entity = entity;
                info.isPlanted = (name.find("planted") != std::string::npos);

                uint64_t sceneNode = kernel->ReadMemory<uint64_t>(pid, entity + 0x338);
                if (sceneNode) {
                    info.origin = kernel->ReadMemory<Vector3>(pid, sceneNode + 0xD0);
                } else {
                    info.origin = kernel->ReadMemory<Vector3>(pid, entity + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
                }

                if (info.isPlanted) {
                    info.countdown = kernel->ReadMemory<float>(pid, entity + cs2_dumper::schemas::client_dll::C_PlantedC4::m_flC4Blow);
                }

                info.isValid = true;
                return info;
            }
        }

        return info;
    }
};
