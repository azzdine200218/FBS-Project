#pragma once
#include <windows.h>
#include <string>
#include <unordered_map>

class KeyBind {
private:
    int keyCode = 0;
    std::string keyName = "";
    bool isToggle = false;
    bool state = false;
    bool prevState = false;
    
public:
    KeyBind() = default;
    KeyBind(int code, const std::string& name, bool toggle = false) 
        : keyCode(code), keyName(name), isToggle(toggle) {}
    
    void SetKey(int code, const std::string& name) { keyCode = code; keyName = name; }
    int GetKeyCode() const { return keyCode; }
    std::string GetKeyName() const { return keyName.empty() ? "NONE" : keyName; }
    
    void SetToggle(bool toggle) { isToggle = toggle; }
    bool IsToggle() const { return isToggle; }
    
    bool IsPressed() {
        if (keyCode == 0) return false;
        bool pressed = (GetAsyncKeyState(keyCode) & 0x8000) != 0;
        
        if (isToggle) {
            if (pressed && !prevState) {
                state = !state;
            }
            prevState = pressed;
            return state;
        } else {
            return pressed;
        }
    }
    
    void Clear() { keyCode = 0; keyName = ""; }
};

class KeyBindManager {
private:
    std::unordered_map<std::string, KeyBind> binds;
    bool waitingForKey = false;
    std::string bindToSet;
    
public:
    KeyBindManager() {
        // Default binds
        binds["toggle_menu"] = KeyBind(VK_INSERT, "INSERT", true);
        binds["bhop"] = KeyBind(VK_SPACE, "SPACE", false);
    }
    
    bool IsWaitingForKey() const { return waitingForKey; }
    void StartWaiting(const std::string& bindName) { waitingForKey = true; bindToSet = bindName; }
    void StopWaiting() { waitingForKey = false; bindToSet = ""; }
    
    void CheckForKeyPress() {
        if (!waitingForKey) return;
        
        for (int i = 8; i < 256; i++) {
            if (GetAsyncKeyState(i) & 0x8000) {
                std::string name = GetKeyName(i);
                if (!name.empty() && name != "LMOUSE" && name != "RMOUSE") {
                    SetKeyBind(bindToSet, i, name);
                    StopWaiting();
                    break;
                }
            }
        }
    }
    
    void SetKeyBind(const std::string& name, int keyCode, const std::string& keyName) {
        binds[name] = KeyBind(keyCode, keyName, binds[name].IsToggle());
    }
    
    KeyBind* GetKeyBind(const std::string& name) {
        if (binds.find(name) != binds.end()) {
            return &binds[name];
        }
        return nullptr;
    }
    
    std::string GetKeyName(const std::string& bindName) {
        if (binds.find(bindName) != binds.end()) {
            return binds[bindName].GetKeyName();
        }
        return "NONE";
    }
    
    std::string GetKeyName(int keyCode) {
        switch (keyCode) {
            case 0x01: return "LMOUSE";
            case 0x02: return "RMOUSE";
            case 0x04: return "MMOUSE";
            case 0x08: return "BACKSPACE";
            case 0x09: return "TAB";
            case 0x0D: return "ENTER";
            case 0x10: return "SHIFT";
            case 0x11: return "CTRL";
            case 0x12: return "ALT";
            case 0x13: return "PAUSE";
            case 0x14: return "CAPS_LOCK";
            case 0x1B: return "ESC";
            case 0x20: return "SPACE";
            case 0x2E: return "DELETE";
            case 0x25: return "LEFT";
            case 0x26: return "UP";
            case 0x27: return "RIGHT";
            case 0x28: return "DOWN";
            case 0x2D: return "INSERT";
            case 0x2F: return "HELP";
            case 0x60: return "NUM0";
            case 0x61: return "NUM1";
            case 0x62: return "NUM2";
            case 0x63: return "NUM3";
            case 0x64: return "NUM4";
            case 0x65: return "NUM5";
            case 0x66: return "NUM6";
            case 0x67: return "NUM7";
            case 0x68: return "NUM8";
            case 0x69: return "NUM9";
            case 0x70: return "F1";
            case 0x71: return "F2";
            case 0x72: return "F3";
            case 0x73: return "F4";
            case 0x74: return "F5";
            case 0x75: return "F6";
            case 0x76: return "F7";
            case 0x77: return "F8";
            case 0x78: return "F9";
            case 0x79: return "F10";
            case 0x7A: return "F11";
            case 0x7B: return "F12";
            case 0xA0: return "L_SHIFT";
            case 0xA1: return "R_SHIFT";
            case 0xA2: return "L_CTRL";
            case 0xA3: return "R_CTRL";
            case 0xA4: return "L_ALT";
            case 0xA5: return "R_ALT";
            default:
                if (keyCode >= 0x41 && keyCode <= 0x5A) {
                    return std::string(1, (char)keyCode);
                }
                return "UNKNOWN";
        }
    }
};