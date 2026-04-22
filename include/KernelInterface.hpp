#pragma once
#include <windows.h>
#include <string>
#include <cstdint>
#include <intrin.h>

#define FBS_MAGIC 0xFB542069

#define IOCTL_MOUSE_MOVE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_READ_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WRITE_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_BASE_ADDRESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x903, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_BULK_READ CTL_CODE(FILE_DEVICE_UNKNOWN, 0x904, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INIT_SHM CTL_CODE(FILE_DEVICE_UNKNOWN, 0x905, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PROTECT_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x906, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Mouse button flags for kernel injection (must match mouse_driver.h)
#define MOUSE_BTN_LEFT_DOWN   0x0001
#define MOUSE_BTN_LEFT_UP     0x0002
#define MOUSE_BTN_RIGHT_DOWN  0x0004
#define MOUSE_BTN_RIGHT_UP    0x0008
#define MOUSE_WHEEL           0x0400
#define WHEEL_DELTA           120

// ============================================================
// Shared Memory Definitions (must match mouse_driver.h)
// ============================================================
#define SHM_CMD_NONE      0
#define SHM_CMD_READ      1
#define SHM_CMD_WRITE     2
#define SHM_CMD_GET_BASE  3

#define SHM_STATUS_IDLE     0
#define SHM_STATUS_PENDING  1
#define SHM_STATUS_COMPLETE 2
#define SHM_STATUS_ERROR    3

// Max spin iterations before timeout (~5ms at 50us per kernel poll)
#define SHM_SPIN_TIMEOUT 100000

struct FBS_SHARED_MEMORY {
    volatile LONG   Command;
    volatile LONG   Status;
    ULONG           ProcessId;
    ULONGLONG       Address;
    SIZE_T          Size;
    ULONGLONG       Result;
    WCHAR           ModuleName[256];
    UCHAR           Buffer[2048];
};

struct SHM_INIT_RESPONSE {
    ULONGLONG SharedMemoryAddress;
    ULONG magic;
};

// Legacy structs (for IOCTL fallback)
struct MOUSE_MOVE_INFO {
    LONG x;
    LONG y;
    USHORT buttonFlags;
    SHORT scrollDelta;
    ULONG magic;
};

struct PROTECT_REQUEST {
    ULONG ProcessId;
    ULONG magic;
};

struct MEMORY_REQUEST {
    ULONG ProcessId;
    ULONGLONG Address;
    SIZE_T Size;
    ULONG magic;
};

struct BASE_ADDRESS_REQUEST {
    ULONG ProcessId;
    WCHAR ModuleName[256];
    ULONGLONG BaseAddress;
    ULONG magic;
};

class KernelInterface {
private:
    HANDLE hDriver;
    FBS_SHARED_MEMORY* sharedMem;  // Shared memory pointer (NULL = use IOCTL fallback)

    // Initialize shared memory channel
    bool InitSharedMemory() {
        if (!IsConnected()) return false;

        ULONG magic = FBS_MAGIC;
        SHM_INIT_RESPONSE resp = {};
        DWORD bytes = 0;

        BOOL result = DeviceIoControl(hDriver, IOCTL_INIT_SHM,
            &magic, sizeof(magic),
            &resp, sizeof(resp),
            &bytes, nullptr);

        if (result && bytes == sizeof(SHM_INIT_RESPONSE) && resp.magic == FBS_MAGIC && resp.SharedMemoryAddress != 0) {
            // Validate the pointer is in user-space range
            ULONGLONG addr = resp.SharedMemoryAddress;
            if (addr > 0x10000 && addr < 0x7FFFFFFFFFFF) {
                sharedMem = (FBS_SHARED_MEMORY*)(ULONG_PTR)addr;
                
                // Test write to shared memory to verify it's accessible
                __try {
                    sharedMem->Command = SHM_CMD_NONE;
                    sharedMem->Status = SHM_STATUS_IDLE;
                    return true;
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    sharedMem = nullptr;
                    return false;
                }
            }
        }
        return false;
    }

    // Disable shared memory and fall back to IOCTL
    void DisableSharedMemory() {
        sharedMem = nullptr;
    }

    // Helper: wait for shared memory completion with timeout
    bool WaitForCompletion() {
        volatile LONG* status = &sharedMem->Status;
        for (int i = 0; i < SHM_SPIN_TIMEOUT; i++) {
            if (*status != SHM_STATUS_PENDING) return true;
            _mm_pause();
        }
        // Timeout — disable shared memory for future calls
        DisableSharedMemory();
        return false;
    }

public:
    KernelInterface(const char* registryPath = "\\\\.\\NUL");
    ~KernelInterface();

    bool IsConnected() const;
    bool HasSharedMemory() const { return sharedMem != nullptr; }

    bool ProtectProcess(ULONG pid) {
        if (!IsConnected()) return false;
        PROTECT_REQUEST req = {0};
        req.ProcessId = pid;
        req.magic = FBS_MAGIC;
        DWORD bytes = 0;
        return DeviceIoControl(hDriver, IOCTL_PROTECT_PROCESS, &req, sizeof(req), nullptr, 0, &bytes, nullptr);
    }

    bool MoveMouse(int x, int y);
    void LeftClick();
    bool ScrollWheel(int delta);

    // ============================================================
    // ReadMemory: Shared Memory path (fast, stealthy)
    // Falls back to IOCTL if shared memory not available
    // ============================================================
    template <typename T>
    T ReadMemory(ULONG pid, ULONGLONG address) {
        T buffer = {};
        if (!IsConnected()) return buffer;

        // Shared Memory path
        if (sharedMem) {
            __try {
                sharedMem->ProcessId = pid;
                sharedMem->Address = address;
                sharedMem->Size = sizeof(T);
                sharedMem->Command = SHM_CMD_READ;

                _ReadWriteBarrier();
                InterlockedExchange(&sharedMem->Status, SHM_STATUS_PENDING);

                if (WaitForCompletion() && sharedMem && sharedMem->Status == SHM_STATUS_COMPLETE) {
                    memcpy(&buffer, (void*)sharedMem->Buffer, sizeof(T));
                }

                if (sharedMem) {
                    InterlockedExchange(&sharedMem->Status, SHM_STATUS_IDLE);
                }
                return buffer;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                DisableSharedMemory();
            }
        }

        // IOCTL fallback
        MEMORY_REQUEST request{};
        request.ProcessId = pid;
        request.Address = address;
        request.Size = sizeof(T);
        request.magic = FBS_MAGIC;

        DWORD bytesReturned = 0;
        DeviceIoControl(hDriver, IOCTL_READ_MEMORY,
            &request, sizeof(request),
            &buffer, sizeof(T),
            &bytesReturned, nullptr);
        return buffer;
    }

    // ============================================================
    // WriteMemory: Shared Memory path
    // ============================================================
    template <typename T>
    bool WriteMemory(ULONG pid, ULONGLONG address, const T& value) {
        if (!IsConnected()) return false;

        // Shared Memory path
        if (sharedMem) {
            __try {
                sharedMem->ProcessId = pid;
                sharedMem->Address = address;
                sharedMem->Size = sizeof(T);
                memcpy((void*)sharedMem->Buffer, &value, sizeof(T));
                sharedMem->Command = SHM_CMD_WRITE;

                _ReadWriteBarrier();
                InterlockedExchange(&sharedMem->Status, SHM_STATUS_PENDING);

                bool ok = false;
                if (WaitForCompletion() && sharedMem) {
                    ok = (sharedMem->Status == SHM_STATUS_COMPLETE);
                }
                if (sharedMem) {
                    InterlockedExchange(&sharedMem->Status, SHM_STATUS_IDLE);
                }
                return ok;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                DisableSharedMemory();
            }
        }

        // IOCTL fallback
        const SIZE_T totalSize = sizeof(MEMORY_REQUEST) + sizeof(T);
        BYTE inputBuffer[sizeof(MEMORY_REQUEST) + sizeof(T)];

        MEMORY_REQUEST* request = (MEMORY_REQUEST*)inputBuffer;
        request->ProcessId = pid;
        request->Address = address;
        request->Size = sizeof(T);
        request->magic = FBS_MAGIC;

        memcpy(inputBuffer + sizeof(MEMORY_REQUEST), &value, sizeof(T));

        DWORD bytesReturned = 0;
        return DeviceIoControl(hDriver, IOCTL_WRITE_MEMORY,
            inputBuffer, (DWORD)totalSize,
            nullptr, 0,
            &bytesReturned, nullptr);
    }

    ULONGLONG GetBaseAddress(ULONG pid, const std::wstring& moduleName = L"client.dll");

    void* DumpModuleToLocal(ULONG pid, ULONGLONG baseAddress, SIZE_T size) {
        if (!IsConnected()) return nullptr;
        void* localDump = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!localDump) return nullptr;

        // Read in 1MB chunks to avoid overwhelming driver IOCTL
        SIZE_T chunkSize = 1024 * 1024;
        for (SIZE_T offset = 0; offset < size; offset += chunkSize) {
            SIZE_T toRead = size - offset;
            if (toRead > chunkSize) toRead = chunkSize;
            ReadMemoryBlock(pid, baseAddress + offset, (uint8_t*)localDump + offset, toRead);
        }
        return localDump;
    }

    bool ReadMemoryBlock(ULONG pid, ULONGLONG address, void* buffer, SIZE_T size) {
        if (!IsConnected()) return false;

        // Shared Memory path
        if (sharedMem && size <= sizeof(sharedMem->Buffer)) {
            __try {
                sharedMem->ProcessId = pid;
                sharedMem->Address = address;
                sharedMem->Size = size;
                sharedMem->Command = SHM_CMD_READ;

                _ReadWriteBarrier();
                InterlockedExchange(&sharedMem->Status, SHM_STATUS_PENDING);

                if (WaitForCompletion() && sharedMem && sharedMem->Status == SHM_STATUS_COMPLETE) {
                    memcpy(buffer, (void*)sharedMem->Buffer, size);
                    InterlockedExchange(&sharedMem->Status, SHM_STATUS_IDLE);
                    return true;
                }
                if (sharedMem) {
                    InterlockedExchange(&sharedMem->Status, SHM_STATUS_IDLE);
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                DisableSharedMemory();
            }
        }

        // IOCTL fallback
        MEMORY_REQUEST request{};
        request.ProcessId = pid;
        request.Address = address;
        request.Size = size;
        request.magic = FBS_MAGIC;
        DWORD bytesReturned = 0;
        return DeviceIoControl(hDriver, IOCTL_READ_MEMORY,
            &request, sizeof(request),
            buffer, (DWORD)size,
            &bytesReturned, nullptr);
    }

    std::string ReadString(ULONG pid, ULONGLONG address, size_t maxLen = 32) {
        char buf[64] = {};
        size_t readLen = (maxLen < 63) ? maxLen : 63;
        ReadMemoryBlock(pid, address, buf, readLen);
        buf[readLen] = '\0';
        return std::string(buf);
    }

    bool BulkRead(ULONG pid, ULONGLONG address, void* buffer, SIZE_T size) {
        if (!IsConnected() || size > 2048) return false;
        return ReadMemoryBlock(pid, address, buffer, size);
    }
};
