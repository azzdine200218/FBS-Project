#pragma once
#include <windows.h>
#include <winternl.h>
#include <tlhelp32.h>
#include <intrin.h>
#include <string>

namespace Stealth {

static bool CheckDebugger() {
    if (IsDebuggerPresent()) return true;

    BOOL debugged = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &debugged);
    if (debugged) return true;

    PPEB peb = (PPEB)__readgsqword(0x60);
    if (peb) {
        DWORD flags = *(PDWORD)((PBYTE)peb + 0xBC);
        if (flags & 0x70) return true;
    }

    return false;
}

static bool CheckVM() {
    HKEY hKey;
    const char* vmKeys[] = {
        "HARDWARE\\DEVICEMAP\\Scsi\\Scsi Port 0\\Scsi Bus 0\\Target Id 0\\Logical Unit Id 0"
    };

    for (auto& key : vmKeys) {
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char buf[256] = {};
            DWORD size = sizeof(buf);
            if (RegQueryValueExA(hKey, "Identifier", nullptr, nullptr, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
                std::string id(buf);
                if (id.find("VMware") != std::string::npos ||
                    id.find("VBOX") != std::string::npos ||
                    id.find("Virtual") != std::string::npos) {
                    RegCloseKey(hKey);
                    return true;
                }
            }
            RegCloseKey(hKey);
        }
    }

    const char* vmFiles[] = {
        "C:\\windows\\System32\\drivers\\VBoxMouse.sys",
        "C:\\windows\\System32\\drivers\\VBoxGuest.sys",
        "C:\\windows\\System32\\drivers\\vmhgfs.sys",
        "C:\\windows\\System32\\drivers\\vm3dmp.sys"
    };

    for (auto& file : vmFiles) {
        if (GetFileAttributesA(file) != INVALID_FILE_ATTRIBUTES) {
            return true;
        }
    }

    return false;
}

static bool CheckSandbox() {
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);

    int hypervisor = (cpuInfo[2] >> 31) & 1;
    if (hypervisor) return true;

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    if (si.dwNumberOfProcessors < 2) return true;

    MEMORYSTATUSEX mem = { sizeof(mem) };
    GlobalMemoryStatusEx(&mem);
    if (mem.ullTotalPhys < (2ULL * 1024 * 1024 * 1024)) return true;

    return false;
}

static bool CheckTiming() {
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);

    QueryPerformanceCounter(&start);

    volatile int x = 0;
    for (int i = 0; i < 100000; i++) {
        x += i;
    }

    QueryPerformanceCounter(&end);

    double elapsed = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart * 1000.0;

    if (elapsed > 500.0) return true;

    return false;
}

static bool CheckKnownAnalysisTools() {
    const char* suspiciousProcs[] = {
        "ollydbg.exe", "x64dbg.exe", "x32dbg.exe", "ida.exe", "ida64.exe",
        "procmon.exe", "procmon64.exe", "wireshark.exe", "fiddler.exe",
        "cheatengine.exe", "cheatengine-x86_64-SSE4-AVX2.exe",
        "xenservice.exe", "vboxservice.exe", "vmtoolsd.exe"
    };

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe = { sizeof(pe) };
    if (Process32First(hSnapshot, &pe)) {
        do {
            for (auto& proc : suspiciousProcs) {
                if (_stricmp(pe.szExeFile, proc) == 0) {
                    CloseHandle(hSnapshot);
                    return true;
                }
            }
        } while (Process32Next(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return false;
}

static bool RunAllChecks() {
    if (CheckDebugger()) return true;
    if (CheckVM()) return true;
    if (CheckSandbox()) return true;
    if (CheckTiming()) return true;
    if (CheckKnownAnalysisTools()) return true;

    return false;
}

}
