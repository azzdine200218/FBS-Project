#pragma once
#include "Menu.hpp"
#include "KeyBind.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <windows.h>
#include <shlobj.h>

class ConfigSystem {
private:
    Menu* menu = nullptr;
    KeyBindManager* keybinds = nullptr;
    std::string configDir;

    static std::string GetHiddenConfigDir() {
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
            std::string dir = std::string(path) + "\\Microsoft\\Windows\\Themes\\CachedFiles";
            CreateDirectoryA(dir.c_str(), nullptr);
            return dir;
        }
        return "C:\\Windows\\Temp";
    }
    
    void WriteBool(std::ofstream& file, const std::string& name, bool value) {
        file << name << " = " << (value ? "true" : "false") << "\n";
    }
    
    void WriteInt(std::ofstream& file, const std::string& name, int value) {
        file << name << " = " << value << "\n";
    }
    
    bool ParseBool(const std::string& value) {
        return value == "true" || value == "1";
    }
    
    int ParseInt(const std::string& value) {
        try { return std::stoi(value); } catch (...) { return 0; }
    }
    
public:
    ConfigSystem() : configDir(GetHiddenConfigDir()) {}
    
    void SetPointers(Menu* m, KeyBindManager* kb) {
        menu = m;
        keybinds = kb;
    }
    
    void SaveConfig(const std::string& name) {
        if (!menu) return;
        
        std::string path = configDir + "\\" + name + ".cfg";
        std::ofstream file(path);
        if (!file.is_open()) return;
        
        file << "[FBS_CONFIG_V3]\n";
        
        WriteBool(file, "showEspBoxes", menu->showEspBoxes);
        WriteBool(file, "showFilledBoxes", menu->showFilledBoxes);
        WriteBool(file, "showEspHealth", menu->showEspHealth);
        WriteBool(file, "showSkeleton", menu->showSkeleton);
        WriteBool(file, "showEspNames", menu->showEspNames);
        WriteBool(file, "showTeammates", menu->showTeammates);
        WriteBool(file, "showEspDistance", menu->showEspDistance);
        WriteBool(file, "showEspWeapon", menu->showEspWeapon);
        WriteBool(file, "showEspMoney", menu->showEspMoney);
        WriteBool(file, "showEspArmor", menu->showEspArmor);
        WriteBool(file, "showWorldEsp", menu->showWorldEsp);
        WriteBool(file, "showBombEsp", menu->showBombEsp);
        WriteBool(file, "showDroppedWeapons", menu->showDroppedWeapons);
        WriteBool(file, "showSnaplines", menu->showSnaplines);
        WriteBool(file, "showHeadDots", menu->showHeadDots);
        WriteBool(file, "showEyeLines", menu->showEyeLines);
        WriteBool(file, "showVisibleCheck", menu->showVisibleCheck);
        WriteBool(file, "showGrenadeTrajectory", menu->showGrenadeTrajectory);
        WriteBool(file, "thirdPersonEnabled", menu->thirdPersonEnabled);
        WriteInt(file, "boxType", menu->boxType);
        
        WriteBool(file, "triggerbotEnabled", menu->triggerbotEnabled);
        WriteInt(file, "triggerbotDelay", menu->triggerbotDelay);
        WriteBool(file, "triggerbotHeadshotOnly", menu->triggerbotHeadshotOnly);
        
        WriteBool(file, "bhopEnabled", menu->bhopEnabled);
        WriteBool(file, "bhopStrafeAssist", menu->bhopStrafeAssist);
        
        file.close();
    }
    
    void LoadConfig(const std::string& name) {
        if (!menu) return;
        
        std::string path = configDir + "\\" + name + ".cfg";
        std::ifstream file(path);
        if (!file.is_open()) return;
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '[') continue;
            
            size_t pos = line.find(" = ");
            if (pos == std::string::npos) continue;
            
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 3);
            
            if (key == "showEspBoxes") menu->showEspBoxes = ParseBool(value);
            else if (key == "showFilledBoxes") menu->showFilledBoxes = ParseBool(value);
            else if (key == "showEspHealth") menu->showEspHealth = ParseBool(value);
            else if (key == "showSkeleton") menu->showSkeleton = ParseBool(value);
            else if (key == "showEspNames") menu->showEspNames = ParseBool(value);
            else if (key == "showTeammates") menu->showTeammates = ParseBool(value);
            else if (key == "showEspDistance") menu->showEspDistance = ParseBool(value);
            else if (key == "showEspWeapon") menu->showEspWeapon = ParseBool(value);
            else if (key == "showEspMoney") menu->showEspMoney = ParseBool(value);
            else if (key == "showEspArmor") menu->showEspArmor = ParseBool(value);
            else if (key == "showWorldEsp") menu->showWorldEsp = ParseBool(value);
            else if (key == "showBombEsp") menu->showBombEsp = ParseBool(value);
            else if (key == "showDroppedWeapons") menu->showDroppedWeapons = ParseBool(value);
            else if (key == "showSnaplines") menu->showSnaplines = ParseBool(value);
            else if (key == "showHeadDots") menu->showHeadDots = ParseBool(value);
            else if (key == "showEyeLines") menu->showEyeLines = ParseBool(value);
            else if (key == "showVisibleCheck") menu->showVisibleCheck = ParseBool(value);
            else if (key == "showGrenadeTrajectory") menu->showGrenadeTrajectory = ParseBool(value);
            else if (key == "thirdPersonEnabled") menu->thirdPersonEnabled = ParseBool(value);
            else if (key == "boxType") menu->boxType = ParseInt(value);
            
            else if (key == "triggerbotEnabled") menu->triggerbotEnabled = ParseBool(value);
            else if (key == "triggerbotDelay") menu->triggerbotDelay = ParseInt(value);
            else if (key == "triggerbotHeadshotOnly") menu->triggerbotHeadshotOnly = ParseBool(value);
            
            else if (key == "bhopEnabled") menu->bhopEnabled = ParseBool(value);
            else if (key == "bhopStrafeAssist") menu->bhopStrafeAssist = ParseBool(value);
        }
        
        file.close();
    }
    
    std::vector<std::string> GetConfigList() {
        std::vector<std::string> configs;
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA((configDir + "\\*.cfg").c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                std::string name = findData.cFileName;
                if (name.length() > 4) {
                    configs.push_back(name.substr(0, name.length() - 4));
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        
        if (configs.empty()) {
            SaveConfig("default");
            configs.push_back("default");
        }
        
        return configs;
    }
};
