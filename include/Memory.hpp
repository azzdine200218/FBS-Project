#pragma once
#include <windows.h>
#include <string>
#include <tlhelp32.h>
#include "KernelInterface.hpp"
#include "Offsets.hpp"

class Memory {
private:
    KernelInterface& kernel;
    ULONG processId;
    ULONGLONG clientBase;

public:
    Memory(KernelInterface& kernelDriver);

    bool Initialize(const std::wstring& processName = L"cs2.exe");

    ULONG GetProcessId() const { return processId; }
    ULONGLONG GetClientBase() const { return clientBase; }
};
