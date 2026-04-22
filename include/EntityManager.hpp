#pragma once
#include <vector>
#include <cstring>
#include "KernelInterface.hpp"
#include "../include/Offsets.hpp"
#include "../include/Data.hpp"

// Build 14151: CEntityIdentity stride changed from 0x78 to 0x70
constexpr std::ptrdiff_t ENTITY_LIST_ENTRY_STRIDE = 0x70;

struct PlayerCache {
    uint64_t controller;
    uint64_t pawn;
    Vector3 origin;
    Vector3 headPos;
    int health;
    int team;
    int armor;
    uint64_t moneyServices;
    uint64_t gameSceneNode;
    uint64_t boneArray;
    char playerName[128];
    bool isValid;
};

#include <mutex>

class EntityManager {
private:
    KernelInterface* kernel;
    ULONG pid;
    ULONGLONG clientBase;
    
    std::vector<PlayerCache> backBuffer;
    std::vector<PlayerCache> frontBuffer;
    mutable std::mutex mtx;

public:
    EntityManager() : kernel(nullptr), pid(0), clientBase(0) {}

    void Initialize(KernelInterface& k, ULONG p, ULONGLONG base) {
        kernel = &k;
        pid = p;
        clientBase = base;
    }

    void UpdateEntities(int maxEntities = 64) {
        if (!kernel || !kernel->IsConnected() || clientBase == 0) {
            std::lock_guard<std::mutex> lock(mtx);
            frontBuffer.clear();
            return;
        }

        ULONGLONG entityList = kernel->ReadMemory<ULONGLONG>(pid, clientBase + cs2_dumper::offsets::client_dll::dwEntityList);
        if (!entityList) {
            std::lock_guard<std::mutex> lock(mtx);
            frontBuffer.clear();
            return;
        }

        backBuffer.clear();

        ULONGLONG localPawnAddr = kernel->ReadMemory<ULONGLONG>(pid, clientBase + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);

        for (int i = 1; i <= maxEntities; i++) {
            ULONGLONG chunkBase = kernel->ReadMemory<ULONGLONG>(pid, entityList + (ULONGLONG)(8 * ((i & 0x7FFF) >> 9) + 16));
            if (!chunkBase) continue;

            // Entity pointer is at offset 0 within CEntityIdentity (stride = 0x70)
            ULONGLONG controller = kernel->ReadMemory<ULONGLONG>(pid, chunkBase + (ULONGLONG)(ENTITY_LIST_ENTRY_STRIDE * (i & 0x1FF)));
            if (!controller) continue;

            // Use m_hPawn from CBasePlayerController
            ULONG pawnHandle = kernel->ReadMemory<ULONG>(pid, controller + offsets::m_hPawn);
            if (!pawnHandle || pawnHandle == 0xFFFFFFFF) continue;

            int pawnIndex = pawnHandle & 0x7FFF;
            if (pawnIndex <= 0 || pawnIndex > 8192) continue;

            ULONGLONG pawnChunkBase = kernel->ReadMemory<ULONGLONG>(pid, entityList + (ULONGLONG)(8 * (pawnIndex >> 9) + 16));
            if (!pawnChunkBase) continue;

            ULONGLONG pawn = kernel->ReadMemory<ULONGLONG>(pid, pawnChunkBase + (ULONGLONG)(ENTITY_LIST_ENTRY_STRIDE * (pawnIndex & 0x1FF)));
            if (!pawn) continue;

            int health = kernel->ReadMemory<int>(pid, pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
            if (health <= 0 || health > 100) continue;

            int team = kernel->ReadMemory<uint8_t>(pid, pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
            Vector3 origin = kernel->ReadMemory<Vector3>(pid, pawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
            int armor = kernel->ReadMemory<int>(pid, pawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_ArmorValue);
            uint64_t moneyServices = kernel->ReadMemory<uint64_t>(pid, controller + cs2_dumper::schemas::client_dll::CCSPlayerController::m_pInGameMoneyServices);
            // Build 14151: m_pGameSceneNode = 0x330 (was 0x328)
            uint64_t gameSceneNode = kernel->ReadMemory<uint64_t>(pid, pawn + offsets::m_pGameSceneNode);

            char playerName[128] = {};
            kernel->ReadMemoryBlock(pid, controller + cs2_dumper::schemas::client_dll::CBasePlayerController::m_iszPlayerName, playerName, 127);

            uint64_t boneArray = 0;
            if (gameSceneNode && gameSceneNode > 0x1000000) {
                // Try primary bone array offset
                boneArray = kernel->ReadMemory<uint64_t>(pid, gameSceneNode + offsets::m_modelState + 0x80);
                if (!boneArray || boneArray < 0x1000000) {
                    // Fallback for some CS2 builds
                    boneArray = kernel->ReadMemory<uint64_t>(pid, gameSceneNode + offsets::m_modelState + 0xA8);
                }
            }

            PlayerCache cache = {0};
            cache.controller = controller;
            cache.pawn = pawn;
            cache.health = health;
            cache.team = team;
            cache.origin = origin;
            cache.headPos = origin;
            cache.headPos.z += 65.0f;
            cache.armor = armor;
            cache.moneyServices = moneyServices;
            cache.gameSceneNode = gameSceneNode;
            cache.boneArray = boneArray;
            cache.isValid = true;
            memcpy(cache.playerName, playerName, 128);

            backBuffer.push_back(cache);
        }

        std::lock_guard<std::mutex> lock(mtx);
        frontBuffer = backBuffer;
    }

    std::vector<PlayerCache> GetPlayers() const { 
        std::lock_guard<std::mutex> lock(mtx);
        return frontBuffer; 
    }
};
