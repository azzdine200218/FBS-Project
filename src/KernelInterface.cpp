#include "../include/KernelInterface.hpp"
#include "../include/XorStr.hpp"

KernelInterface::KernelInterface(const char* registryPath) : sharedMem(nullptr) {
    UNREFERENCED_PARAMETER(registryPath);
    
    hDriver = CreateFileA(
        XOR_STR("\\\\.\\NUL"),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    // Initialize shared memory channel
    if (IsConnected()) {
        InitSharedMemory();
    }
}

KernelInterface::~KernelInterface() {
    if (IsConnected()) {
        CloseHandle(hDriver);
    }
}

bool KernelInterface::IsConnected() const {
    return hDriver != INVALID_HANDLE_VALUE;
}

bool KernelInterface::MoveMouse(int x, int y) {
    if (x == 0 && y == 0) return true;
    if (!IsConnected()) return false;

    MOUSE_MOVE_INFO info = {};
    info.x = (LONG)x;
    info.y = (LONG)y;
    info.buttonFlags = 0;
    info.scrollDelta = 0;
    info.magic = FBS_MAGIC;

    DWORD bytesReturned = 0;
    return DeviceIoControl(hDriver, IOCTL_MOUSE_MOVE,
        &info, sizeof(info),
        nullptr, 0,
        &bytesReturned, nullptr);
}

void KernelInterface::LeftClick() {
    if (!IsConnected()) return;

    MOUSE_MOVE_INFO info = {};
    info.x = 0;
    info.y = 0;
    info.buttonFlags = MOUSE_BTN_LEFT_DOWN;
    info.scrollDelta = 0;
    info.magic = FBS_MAGIC;

    DWORD bytesReturned = 0;
    DeviceIoControl(hDriver, IOCTL_MOUSE_MOVE,
        &info, sizeof(info),
        nullptr, 0,
        &bytesReturned, nullptr);

    Sleep(20);

    info.buttonFlags = MOUSE_BTN_LEFT_UP;
    info.scrollDelta = 0;
    DeviceIoControl(hDriver, IOCTL_MOUSE_MOVE,
        &info, sizeof(info),
        nullptr, 0,
        &bytesReturned, nullptr);
}

bool KernelInterface::ScrollWheel(int delta) {
    if (delta == 0 || !IsConnected()) return false;

    MOUSE_MOVE_INFO info = {};
    info.x = 0;
    info.y = 0;
    info.buttonFlags = MOUSE_WHEEL;
    info.scrollDelta = (SHORT)delta;
    info.magic = FBS_MAGIC;

    DWORD bytesReturned = 0;
    return DeviceIoControl(hDriver, IOCTL_MOUSE_MOVE,
        &info, sizeof(info),
        nullptr, 0,
        &bytesReturned, nullptr);
}

ULONGLONG KernelInterface::GetBaseAddress(ULONG pid, const std::wstring& moduleName) {
    if (!IsConnected()) return 0;

    // Shared Memory path
    if (sharedMem) {
        sharedMem->ProcessId = pid;
        wcscpy_s(sharedMem->ModuleName, moduleName.c_str());
        sharedMem->Command = SHM_CMD_GET_BASE;

        _ReadWriteBarrier();
        InterlockedExchange(&sharedMem->Status, SHM_STATUS_PENDING);

        volatile LONG* status = &sharedMem->Status;
        while (*status == SHM_STATUS_PENDING) {
            _mm_pause();
        }

        ULONGLONG result = 0;
        if (*status == SHM_STATUS_COMPLETE) {
            result = sharedMem->Result;
        }

        InterlockedExchange(&sharedMem->Status, SHM_STATUS_IDLE);
        return result;
    }

    // IOCTL fallback
    BASE_ADDRESS_REQUEST request{};
    request.ProcessId = pid;
    wcscpy_s(request.ModuleName, moduleName.c_str());
    request.BaseAddress = 0;
    request.magic = FBS_MAGIC;

    DWORD bytesReturned = 0;
    if (DeviceIoControl(hDriver, IOCTL_GET_BASE_ADDRESS, &request, sizeof(request), &request, sizeof(request), &bytesReturned, nullptr)) {
        return request.BaseAddress;
    }
    return 0;
}
