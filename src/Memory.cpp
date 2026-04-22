#include "../include/Memory.hpp"

Memory::Memory(KernelInterface& kernelDriver) : kernel(kernelDriver), processId(0), clientBase(0) {}

bool Memory::Initialize(const std::wstring& processName) {
    // Step 1: Find the process ID via process snapshot
    // TH32CS_SNAPPROCESS is safe — FaceIt does NOT monitor process enumeration
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (processName == pe32.szExeFile) {
                processId = pe32.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);

    if (processId == 0) {
        return false;
    }

    // Step 2: Find client.dll base via KERNEL DRIVER (PEB walk)
    // This replaces CreateToolhelp32Snapshot(TH32CS_SNAPMODULE) which
    // FaceIt AC hooks and bans INSTANTLY on cs2.exe processes.
    // The driver walks PEB->Ldr->InLoadOrderModuleList from kernel mode.
    clientBase = kernel.GetBaseAddress(processId, L"client.dll");

    return (processId != 0 && clientBase != 0);
}
