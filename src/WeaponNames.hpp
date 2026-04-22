#pragma once
#include <string>
#include <map>
#include <algorithm>

inline std::string GetWeaponName(int id) {
    static const std::map<int, std::string> weaponNames = {
        {1, "Deagle"}, {2, "Dualies"}, {3, "Five-SeveN"}, {4, "Glock-18"},
        {7, "AK-47"}, {8, "AUG"}, {9, "AWP"}, {10, "FAMAS"}, {11, "G3SG1"},
        {13, "Galil AR"}, {14, "M249"}, {16, "M4A4"}, {17, "Mac-10"},
        {19, "P90"}, {23, "MP5-SD"}, {24, "UMP-45"}, {25, "XM1014"},
        {26, "PP-Bizon"}, {27, "Mag-7"}, {28, "Negev"}, {29, "Sawed-Off"},
        {30, "Tec-9"}, {31, "Zeus x27"}, {32, "P2000"}, {33, "MP7"},
        {34, "MP9"}, {35, "Nova"}, {36, "P250"}, {38, "SCAR-20"},
        {39, "SG 553"}, {40, "SSG 08"}, {42, "Knife"}, {43, "Flashbang"},
        {44, "HE Grenade"}, {45, "Smoke"}, {46, "Molotov"}, {47, "Decoy"},
        {48, "Incendiary"}, {49, "C4"}, {60, "M4A1-S"}, {61, "USP-S"},
        {63, "CZ75-Auto"}, {64, "R8 Revolver"}
    };

    auto it = weaponNames.find(id);
    return (it != weaponNames.end()) ? it->second : "Item";
}

inline std::string GetWeaponNameFromDesigner(std::string name) {
    if (name.empty()) return "";
    
    // Convert to lowercase for comparison
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    
    if (name.find("weapon_") != std::string::npos) {
        std::string clean = name.substr(7);
        if (clean == "ak47") return "AK-47";
        if (clean == "awp") return "AWP";
        if (clean == "deagle") return "Deagle";
        if (clean == "m4a1") return "M4A4";
        if (clean == "m4a1_s") return "M4A1-S";
        if (clean == "glock") return "Glock-18";
        if (clean == "usp_silencer") return "USP-S";
        if (clean == "flashbang") return "Flash";
        if (clean == "smokegrenade") return "Smoke";
        if (clean == "hegrenade") return "HE";
        if (clean == "molotov") return "Molotov";
        if (clean == "c4") return "C4 Bomb";
        if (clean == "ssg08") return "Scout";
        if (clean == "sg556") return "SG 553";
        
        // Capitalize first letter as fallback
        if (!clean.empty()) clean[0] = std::toupper(clean[0]);
        return clean;
    }
    
    return "";
}
