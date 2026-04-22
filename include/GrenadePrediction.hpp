#pragma once
#include "../include/KernelInterface.hpp"
#include "../include/Data.hpp"
#include "../include/Offsets.hpp"
#include <cmath>

namespace GrenadePrediction {

    struct TrajectoryPoint {
        Vector3 position;
        bool hasBounced;
        float time;
    };

    static void SimulateTrajectory(KernelInterface& kernel, ULONG pid, uint64_t localPawn, TrajectoryPoint* points, int& pointCount, int maxPoints = 100) {
        pointCount = 0;

        if (!localPawn) return;

        Vector3 eyePos = kernel.ReadMemory<Vector3>(pid, localPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
        eyePos.z += 64.0f;

        Vector3 eyeAngles = kernel.ReadMemory<Vector3>(pid, localPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_angEyeAngles);
        float pitch = eyeAngles.x * (3.14159265358979323846f / 180.0f);
        float yaw = eyeAngles.y * (3.14159265358979323846f / 180.0f);

        float forward_x = cosf(pitch) * cosf(yaw);
        float forward_y = cosf(pitch) * sinf(yaw);
        float forward_z = sinf(pitch);

        float velocity = 750.0f;
        Vector3 vel = {
            eyePos.x + forward_x * velocity,
            eyePos.y + forward_y * velocity,
            eyePos.z + forward_z * velocity
        };

        Vector3 pos = eyePos;
        float gravity = 800.0f;
        float step = 0.03f;
        float currentTime = 0.0f;

        for (int i = 0; i < maxPoints; i++) {
            points[i].position = pos;
            points[i].hasBounced = false;
            points[i].time = currentTime;

            vel.z -= gravity * step;

            pos.x += vel.x * step;
            pos.y += vel.y * step;
            pos.z += vel.z * step;

            if (pos.z < 0.0f) {
                pos.z = 0.0f;
                vel.z = -vel.z * 0.4f;
                vel.x *= 0.6f;
                vel.y *= 0.6f;
                points[i].hasBounced = true;

                if (fabsf(vel.z) < 20.0f) break;
            }

            currentTime += step;
            pointCount++;
        }
    }

}
