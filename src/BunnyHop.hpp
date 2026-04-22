#pragma once
#include "../include/KernelInterface.hpp"
#include "../include/Data.hpp"
#include "../include/Offsets.hpp"
#include <cmath>
#include <chrono>
#include <thread>
#include <windows.h>

class BunnyHop {
private:
    bool strafeAssist = true;
    ULONG pid = 0;
    ULONGLONG localPlayerPawn = 0;
    ULONGLONG clientBase = 0;
    KernelInterface* kernel = nullptr;
    
    // Tracking States to avoid spamming
    bool aPressed = false;
    bool dPressed = false;
    float previousYaw = 0.0f;

    inline bool IsOnGround() {
        if (!localPlayerPawn) return false;
        int flags = kernel->ReadMemory<int>(pid, localPlayerPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_fFlags);
        return (flags & (1 << 0)) != 0;
    }

    inline bool IsJumping() {
        // Read async state of spacebar
        return (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    }

    void ForceJump() {
        // Use kernel scroll wheel down if available, else user-mode mouse_event
        // Requires user to have: bind mwheeldown +jump
        if (kernel && kernel->IsConnected()) {
            kernel->ScrollWheel(-120);
        } else {
            mouse_event(MOUSEEVENTF_WHEEL, 0, 0, -120, 0);
        }
    }

    void PressA() {
        if (!aPressed) {
            keybd_event('A', MapVirtualKey('A', 0), 0, 0);
            aPressed = true;
        }
    }
    void ReleaseA() {
        if (aPressed) {
            keybd_event('A', MapVirtualKey('A', 0), KEYEVENTF_KEYUP, 0);
            aPressed = false;
        }
    }

    void PressD() {
        if (!dPressed) {
            keybd_event('D', MapVirtualKey('D', 0), 0, 0);
            dPressed = true;
        }
    }
    void ReleaseD() {
        if (dPressed) {
            keybd_event('D', MapVirtualKey('D', 0), KEYEVENTF_KEYUP, 0);
            dPressed = false;
        }
    }

    void ReleaseStrafeKeys() {
        ReleaseA();
        ReleaseD();
    }

    void Strafe(float currentYaw) {
        if (!strafeAssist || !localPlayerPawn) {
            ReleaseStrafeKeys();
            return;
        }

        Vector3 velocity = kernel->ReadMemory<Vector3>(pid, localPlayerPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_vecAbsVelocity);
        float speed = sqrtf(velocity.x * velocity.x + velocity.y * velocity.y);

        // Don't auto strafe if not moving fast enough
        if (speed < 15.0f) {
            ReleaseStrafeKeys();
            return;
        }

        float delta = currentYaw - previousYaw;
        previousYaw = currentYaw;

        if (delta > 180.0f) delta -= 360.0f;
        if (delta < -180.0f) delta += 360.0f;

        if (delta > 0.5f) { 
            // Mouse moved Left -> Strafe Left
            PressA();
            ReleaseD();
        } 
        else if (delta < -0.5f) { 
            // Mouse moved Right -> Strafe Right
            PressD();
            ReleaseA();
        } 
        else {
            ReleaseStrafeKeys();
        }
    }

public:
    void Initialize(KernelInterface& k, ULONG processId, ULONGLONG base = 0) {
        kernel = &k;
        pid = processId;
        clientBase = base;
    }

    void SetClientBase(ULONGLONG base) { clientBase = base; }
    void SetStrafeAssist(bool state) { strafeAssist = state; }
    bool GetStrafeAssist() { return strafeAssist; }
    void UpdateLocalPlayer(ULONGLONG pawn) { localPlayerPawn = pawn; }

    void Execute() {
        if (!localPlayerPawn || !clientBase) {
            ReleaseStrafeKeys();
            return;
        }

        Vector3 angles = kernel->ReadMemory<Vector3>(pid, clientBase + cs2_dumper::offsets::client_dll::dwViewAngles);

        if (IsJumping()) {
            if (IsOnGround()) {
                // We reached ground while holding space -> inject jump impulse
                ForceJump();
                ReleaseStrafeKeys();
                previousYaw = angles.y;
                // Add small sleep to allow jump to register natively and avoid double-spam on same sub-tick
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
            } else {
                Strafe(angles.y);
            }
        } else {
            ReleaseStrafeKeys();
            previousYaw = angles.y;
        }
    }
};