#pragma once
#include "Overlay.hpp"

class Menu {
public:
    // Visuals
    bool showEspBoxes = true;
    bool showFilledBoxes = false;
    bool showEspHealth = true;
    bool showSkeleton = true;
    bool showEspNames = false;
    bool showTeammates = false;
    bool showEspDistance = false;
    bool showEspWeapon = false;
    bool showEspMoney = false;
    bool showEspArmor = false;
    bool showWorldEsp = false;
    bool showBombEsp = false;
    bool showDroppedWeapons = true;
    bool showSnaplines = false;
    bool showHeadDots = false;
    bool showEyeLines = false;
    bool showVisibleCheck = false;
    bool showGrenadeTrajectory = false;
    bool thirdPersonEnabled = false;
    int boxType = 0;

    // Combat - Triggerbot
    bool triggerbotEnabled = true;
    int triggerbotDelay = 5;
    bool triggerbotHeadshotOnly = false;

    // Combat - Aimbot
    bool aimbotEnabled = false;
    int aimbotFov = 90;
    float aimbotSmoothness = 5.0f;
    int aimbotBone = 0;
    bool aimbotTargetTeammates = false;

    // Combat - RCS
    bool rcsEnabled = true;
    float rcsHorizontal = 1.0f;
    float rcsVertical = 1.0f;

    // Bunny Hop
    bool bhopEnabled = false;
    bool bhopStrafeAssist = true;

    // UI State
    int currentTab = 0;

    // Config
    void Draw(Overlay& overlay);
    void SaveConfig(const char* filename);
    void LoadConfig(const char* filename);
};
