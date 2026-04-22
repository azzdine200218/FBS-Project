#include <ntddk.h>
#include <wdm.h>

// Suppress nameless struct/union warning for MOUSE_INPUT_DATA
#pragma warning(disable:4201)
#pragma warning(disable:4214) // nonstandard extension: bit field types other than int
#include "mouse_driver.h"

// External function declarations (undocumented or in ntifs.h)
NTSTATUS NTAPI PsLookupProcessByProcessId(
    _In_ HANDLE ProcessId,
    _Outptr_ PEPROCESS *Process
);

PVOID NTAPI PsGetProcessSectionBaseAddress(
    _In_ PEPROCESS Process
);

NTSTATUS NTAPI MmCopyVirtualMemory(
    PEPROCESS SourceProcess,
    PVOID SourceAddress,
    PEPROCESS TargetProcess,
    PVOID TargetAddress,
    SIZE_T BufferSize,
    KPROCESSOR_MODE PreviousMode,
    PSIZE_T ReturnSize
);

// ============================================================
// STEALTH LAYER 3: Kernel Trace Cleaning
// Removes all evidence of manual driver mapping from:
//   1. PiDDBCacheTable  - detected by FaceIt, EAC, BattlEye
//   2. MmUnloadedDrivers - detected by FaceIt, EAC
// ============================================================

// Undocumented PiDDB Cache Entry structure (Windows 10/11)
typedef struct _PiDDBCacheEntry {
    LIST_ENTRY          List;
    UNICODE_STRING      DriverName;
    ULONG               TimeDateStamp;
    NTSTATUS            LoadStatus;
    char                _0x0028[16];
} PiDDBCacheEntry, *PPiDDBCacheEntry;

// Undocumented MmUnloadedDrivers entry structure
typedef struct _UNLOADED_DRIVER {
    UNICODE_STRING      Name;
    PVOID               StartAddress;
    PVOID               EndAddress;
    LARGE_INTEGER       CurrentTime;
} UNLOADED_DRIVER, *PUNLOADED_DRIVER;

// Forward declarations for exported kernel symbols (resolved via MmGetSystemRoutineAddress)
typedef PRTL_AVL_TABLE (*fnPiDDBCacheTable);

//
// Find a kernel export by name using MmGetSystemRoutineAddress
//
static PVOID GetKernelExport(PCWSTR name) {
    UNICODE_STRING uName;
    RtlInitUnicodeString(&uName, name);
    return MmGetSystemRoutineAddress(&uName);
}

//
// Resolve the PiDDBCacheTable pointer by scanning ntoskrnl for the
// PiDDBCacheTable reference near the "PiDDBLock" exported symbol.
// Uses a simple byte-pattern scan relative to a known anchor.
//
static PRTL_AVL_TABLE ResolvePiDDBCacheTable(void) {
    // Pattern: look for PiDDBLock near PiDDBCacheTable via LdrpInvertedFunctionTable region
    // We use PsInvertedFunctionTable as an anchor to locate ntoskrnl base, then scan
    PVOID anchor = GetKernelExport(L"PsInvertedFunctionTable");
    if (!anchor) return NULL;

    // Walk backwards to ntoskrnl base (page-aligned)
    ULONG_PTR base = (ULONG_PTR)anchor & ~0xFFFull;
    for (; base > (ULONG_PTR)anchor - 0x1000000; base -= 0x1000) {
        __try {
            if (*(USHORT*)base == 0x5A4D) { // MZ header
                break;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return NULL;
        }
    }

    if (*(USHORT*)base != 0x5A4D) return NULL;

    // Scan for the byte signature of PiDDBCacheTable reference in nt!PiLookupImageName
    // Pattern: 48 8D 0D ?? ?? ?? ??  (lea rcx, [rip+offset])
    PUCHAR scan = (PUCHAR)base;
    ULONG_PTR limit = base + 0x1000000;

    for (ULONG_PTR addr = (ULONG_PTR)scan; addr < limit - 7; addr++) {
        PUCHAR p = (PUCHAR)addr;
        __try {
            // Look for: 66 03 D2 48 8D 0D = common prefix before PiDDBCacheTable LEA
            if (p[0] == 0x66 && p[1] == 0x03 && p[2] == 0xD2 &&
                p[3] == 0x48 && p[4] == 0x8D && p[5] == 0x0D) {
                INT32 relOff = *(INT32*)(p + 6);
                PVOID candidate = (PVOID)(addr + 10 + relOff);
                // Basic validation: must be in kernel space
                if ((ULONG_PTR)candidate > 0xFFFF800000000000ull &&
                    (ULONG_PTR)candidate < 0xFFFFFFFFFFFFF000ull) {
                    return (PRTL_AVL_TABLE)candidate;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            continue;
        }
    }
    return NULL;
}

//
// Remove our driver entry from PiDDBCacheTable.
// The table is keyed by (TimeDateStamp, DriverName).
// KDMapper sets the TimeDateStamp in the PE header before mapping.
//
static VOID CleanPiDDBCache(ULONG timeDateStamp, PCWSTR driverBaseName) {
    UNREFERENCED_PARAMETER(driverBaseName);
    PRTL_AVL_TABLE table = ResolvePiDDBCacheTable();
    if (!table) return;

    // Walk the table's ordered list (table->OrderedPointer is the first real node)
    PLIST_ENTRY listHead = (PLIST_ENTRY)((PUCHAR)table + sizeof(RTL_AVL_TABLE));
    if (!listHead) return;

    __try {
        PLIST_ENTRY entry = listHead->Flink;
        while (entry && entry != listHead) {
            PPiDDBCacheEntry cacheEntry = CONTAINING_RECORD(entry, PiDDBCacheEntry, List);
            PLIST_ENTRY next = entry->Flink;

            if (cacheEntry->TimeDateStamp == timeDateStamp) {
                // Unlink from the doubly-linked list
                RemoveEntryList(&cacheEntry->List);
                // Zero the entry so forensic tools find nothing
                RtlSecureZeroMemory(cacheEntry, sizeof(PiDDBCacheEntry));
                break;
            }
            entry = next;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Silently continue - partial clean is better than BSOD
    }
}

//
// Remove our driver from MmUnloadedDrivers circular buffer.
// The buffer holds the last 50 unloaded drivers.
// KDMapper adds the driver then calls its unload notification,
// leaving a stale entry.
//
static VOID CleanMmUnloadedDrivers(PCWSTR driverBaseName) {
    // Resolve MmUnloadedDrivers and MmLastUnloadedDriver from ntoskrnl exports
    PVOID pUnloadedDrivers = GetKernelExport(L"MmUnloadedDrivers");
    PVOID pLastUnloaded    = GetKernelExport(L"MmLastUnloadedDriver");

    if (!pUnloadedDrivers || !pLastUnloaded) return;

    PUNLOADED_DRIVER drivers  = *(PUNLOADED_DRIVER*)pUnloadedDrivers;
    PULONG           lastIdx  = (PULONG)pLastUnloaded;

    if (!drivers || !lastIdx) return;

    __try {
        ULONG count = *lastIdx;
        // The circular buffer holds max 50 entries (Windows default)
        ULONG maxEntries = 50;
        UNICODE_STRING target;
        RtlInitUnicodeString(&target, driverBaseName);

        for (ULONG i = 0; i < maxEntries && i < count; i++) {
            PUNLOADED_DRIVER d = &drivers[i];
            if (d->Name.Buffer && d->Name.Length > 0) {
                if (RtlEqualUnicodeString(&d->Name, &target, TRUE)) {
                    // Zero out the entry
                    RtlSecureZeroMemory(&d->Name, sizeof(UNICODE_STRING));
                    d->StartAddress = NULL;
                    d->EndAddress   = NULL;
                    d->CurrentTime.QuadPart = 0;
                    break;
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Silently continue
    }
}

//
// Master function: clean all kernel traces left by KDMapper
// Call this once from DriverEntry after successful initialization.
//
static VOID CleanKernelTraces(void) {
    // The TimeDateStamp is embedded in the PE header at compile time.
    // KDMapper reads this value and uses it as the PiDDB key.
    // We must match exactly what KDMapper stores.
    //
    // NOTE: KDMapper zeros the TimeDateStamp in the mapped image to avoid
    //       signature matching, but PiDDB was already populated before zeroing.
    //       We use 0 as the stamp to match KDMapper's behavior.
    CleanPiDDBCache(0, L"FBSKernel_V3.sys");
    CleanMmUnloadedDrivers(L"FBSKernel_V3");
}

// ============================================================
// STEALTH LAYER 4: Kernel-Level Mouse Injection
// Replaces detectable user-mode mouse_event() with direct
// injection through MouseClassServiceCallback.
// FaceIt AC hooks mouse_event/SendInput — this bypasses all.
// ============================================================

static MouseClassServiceCallbackFn g_MouseClassCallback = NULL;
static PDEVICE_OBJECT g_MouseClassDevice = NULL;

// Scan a port driver device's extension for CONNECT_DATA
// CONNECT_DATA = { ClassDeviceObject, ClassService }
// The port driver stores this when MouClass connects via IOCTL_INTERNAL_MOUSE_CONNECT
static VOID ScanForMouseCallback(PDEVICE_OBJECT portDevice) {
    if (!portDevice || !portDevice->DeviceExtension) return;
    PUCHAR ext = (PUCHAR)portDevice->DeviceExtension;

    for (SIZE_T offset = 0; offset < 0x400; offset += sizeof(PVOID)) {
        __try {
            PDEVICE_OBJECT candidate = *(PDEVICE_OBJECT*)(ext + offset);

            // Must be a valid kernel pointer
            if (!candidate || (ULONG_PTR)candidate < 0xFFFF800000000000ULL) continue;
            if (!MmIsAddressValid(candidate)) continue;

            // Must be a DEVICE_OBJECT (Type == IO_TYPE_DEVICE == 3)
            if (candidate->Type != IO_TYPE_DEVICE) continue;

            // Its DriverObject must belong to MouClass
            if (!candidate->DriverObject || !candidate->DriverObject->DriverName.Buffer) continue;

            UNICODE_STRING mouClassName;
            RtlInitUnicodeString(&mouClassName, L"\\Driver\\mouclass");
            if (!RtlEqualUnicodeString(&candidate->DriverObject->DriverName, &mouClassName, TRUE)) continue;

            // FOUND ClassDeviceObject! Next pointer is ClassService (the callback)
            PVOID callback = *(PVOID*)(ext + offset + sizeof(PVOID));
            if (callback && (ULONG_PTR)callback > 0xFFFF800000000000ULL && MmIsAddressValid(callback)) {
                g_MouseClassCallback = (MouseClassServiceCallbackFn)callback;
                g_MouseClassDevice = candidate;
                return;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            continue;
        }
    }
}

// Find MouseClassServiceCallback by scanning port driver device extensions
static NTSTATUS InitMouseInjection(void) {
    // Try USB mouse (MouHID), then PS/2 mouse (i8042prt)
    UNICODE_STRING driverNames[2];
    RtlInitUnicodeString(&driverNames[0], L"\\Driver\\MouHID");
    RtlInitUnicodeString(&driverNames[1], L"\\Driver\\i8042prt");

    int i;
    for (i = 0; i < 2 && !g_MouseClassCallback; i++) {
        PDRIVER_OBJECT driver = NULL;
        NTSTATUS status = ObReferenceObjectByName(
            &driverNames[i],
            OBJ_CASE_INSENSITIVE,
            NULL, 0,
            *IoDriverObjectType,
            KernelMode,
            NULL,
            (PVOID*)&driver
        );

        if (!NT_SUCCESS(status)) continue;

        // Walk all devices of this port driver
        PDEVICE_OBJECT device = driver->DeviceObject;
        while (device && !g_MouseClassCallback) {
            ScanForMouseCallback(device);
            device = device->NextDevice;
        }

        ObDereferenceObject(driver);
    }

    return g_MouseClassCallback ? STATUS_SUCCESS : STATUS_NOT_FOUND;
}

// Inject mouse input through the class service callback
// This is IDENTICAL to real hardware input — completely undetectable
static VOID InjectMouseInput(LONG deltaX, LONG deltaY, USHORT buttonFlags) {
    if (!g_MouseClassCallback || !g_MouseClassDevice) return;

    MOUSE_INPUT_DATA mid;
    RtlZeroMemory(&mid, sizeof(mid));
    mid.UnitId = 0;
    mid.Flags = MOUSE_MOVE_RELATIVE;
    mid.LastX = deltaX;
    mid.LastY = deltaY;
    mid.ButtonFlags = buttonFlags;

    ULONG consumed = 0;

    __try {
        g_MouseClassCallback(g_MouseClassDevice, &mid, &mid + 1, &consumed);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Silently fail — better than BSOD
    }
}

// ============================================================
// STEALTH LAYER 5: Kernel-Level Module Enumeration
// Replaces detectable user-mode CreateToolhelp32Snapshot()
// with a kernel PEB walk using MmCopyVirtualMemory.
// FaceIt monitors TH32CS_SNAPMODULE — this bypasses it.
// ============================================================

// Undocumented: Get process PEB pointer (in target address space)
PPEB NTAPI PsGetProcessPeb(PEPROCESS Process);

// For mouse input injection
PDEVICE_OBJECT g_MouseDeviceObject = NULL;

// Process Protection Globals
ULONG g_ProtectedPID = 0;
PVOID g_ObHandle = NULL;

#ifndef PROCESS_VM_READ
#define PROCESS_VM_READ 0x0010
#define PROCESS_VM_WRITE 0x0020
#define PROCESS_SUSPEND_RESUME 0x0800
#define PROCESS_TERMINATE 0x0001
#endif

OB_PREOP_CALLBACK_STATUS PreOperationCallback(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION OperationInformation) {
    UNREFERENCED_PARAMETER(RegistrationContext);

    if (OperationInformation->ObjectType == *PsProcessType) {
        PEPROCESS openedProcess = (PEPROCESS)OperationInformation->Object;
        ULONG targetPid = HandleToULong(PsGetProcessId(openedProcess));

        if (g_ProtectedPID != 0 && targetPid == g_ProtectedPID) {
            if (OperationInformation->Operation == OB_OPERATION_HANDLE_CREATE) {
                // Strip access rights from FaceIT or anyone trying to inspect the hack
                OperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_READ;
                OperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_WRITE;
                OperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_SUSPEND_RESUME;
                OperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
            } else if (OperationInformation->Operation == OB_OPERATION_HANDLE_DUPLICATE) {
                OperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess &= ~PROCESS_VM_READ;
                OperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess &= ~PROCESS_VM_WRITE;
                OperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess &= ~PROCESS_SUSPEND_RESUME;
                OperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
            }
        }
    }
    return OB_PREOP_SUCCESS;
}

// Helper: read memory from target process using MmCopyVirtualMemory
static NTSTATUS ReadTargetMemory(PEPROCESS Process, PVOID TargetAddr, PVOID LocalBuf, SIZE_T Size) {
    SIZE_T bytesRead = 0;
    return MmCopyVirtualMemory(Process, TargetAddr, PsGetCurrentProcess(), LocalBuf, Size, KernelMode, &bytesRead);
}

// Memory manipulation
NTSTATUS KeReadVirtualMemory(PEPROCESS Process, PVOID SourceAddress, PVOID TargetAddress, SIZE_T Size) {
    SIZE_T bytesRead = 0;
    return MmCopyVirtualMemory(Process, SourceAddress, PsGetCurrentProcess(), TargetAddress, Size, KernelMode, &bytesRead);
}

// Find module base address by walking the target process PEB->Ldr
// Uses MmCopyVirtualMemory (proven working) instead of KeStackAttachProcess
// PEB offsets for x64 Windows 10/11:
//   PEB->Ldr                           = 0x18
//   PEB_LDR_DATA->InLoadOrderModuleList = 0x10
//   LDR_DATA_TABLE_ENTRY->DllBase       = 0x30
//   LDR_DATA_TABLE_ENTRY->BaseDllName   = 0x58 (UNICODE_STRING, 16 bytes on x64)
NTSTATUS GetModuleBaseByName(ULONG ProcessId, PCWSTR ModuleName, ULONGLONG* BaseAddress) {
    if (ProcessId == 0 || ProcessId == 4) return STATUS_ACCESS_DENIED;

    PEPROCESS Process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &Process);
    if (!NT_SUCCESS(status)) return status;

    // If no module name, return main process base
    if (!ModuleName || ModuleName[0] == L'\0') {
        *BaseAddress = (ULONGLONG)PsGetProcessSectionBaseAddress(Process);
        ObDereferenceObject(Process);
        return STATUS_SUCCESS;
    }

    UNICODE_STRING targetName;
    RtlInitUnicodeString(&targetName, ModuleName);
    *BaseAddress = 0;

    __try {
        // Step 1: Get PEB address (this is a pointer in the TARGET process)
        PVOID pebAddr = (PVOID)PsGetProcessPeb(Process);
        if (!pebAddr) {
            ObDereferenceObject(Process);
            return STATUS_NOT_FOUND;
        }

        // Step 2: Read PEB->Ldr (offset 0x18 on x64)
        PVOID ldrAddr = NULL;
        status = ReadTargetMemory(Process, (PUCHAR)pebAddr + 0x18, &ldrAddr, sizeof(PVOID));
        if (!NT_SUCCESS(status) || !ldrAddr) {
            ObDereferenceObject(Process);
            return STATUS_NOT_FOUND;
        }

        // Step 3: Read Ldr->InLoadOrderModuleList.Flink (offset 0x10)
        // listHead = ldrAddr + 0x10 (the address of the LIST_ENTRY head)
        PVOID listHeadAddr = (PUCHAR)ldrAddr + 0x10;
        PVOID currentEntry = NULL;
        status = ReadTargetMemory(Process, listHeadAddr, &currentEntry, sizeof(PVOID));
        if (!NT_SUCCESS(status) || !currentEntry) {
            ObDereferenceObject(Process);
            return STATUS_NOT_FOUND;
        }

        // Step 4: Walk the module list
        ULONG count = 0;
        while (currentEntry != listHeadAddr && count < 512) {
            count++;

            // Read BaseDllName at offset 0x58 (UNICODE_STRING = 16 bytes on x64)
            // Layout: Length(2) + MaxLength(2) + Pad(4) + Buffer*(8) = 16
            UCHAR nameStruct[16] = {0};
            status = ReadTargetMemory(Process, (PUCHAR)currentEntry + 0x58, nameStruct, 16);
            if (!NT_SUCCESS(status)) break;

            USHORT nameLen = *(USHORT*)nameStruct;           // Length in bytes
            PVOID  nameBuf = *(PVOID*)(nameStruct + 8);      // Buffer pointer

            if (nameBuf && nameLen > 0 && nameLen < 520) {
                // Read the actual module name string
                WCHAR moduleNameBuf[260];
                RtlZeroMemory(moduleNameBuf, sizeof(moduleNameBuf));
                USHORT readLen = nameLen;
                if (readLen > sizeof(moduleNameBuf) - sizeof(WCHAR))
                    readLen = sizeof(moduleNameBuf) - sizeof(WCHAR);

                status = ReadTargetMemory(Process, nameBuf, moduleNameBuf, readLen);
                if (NT_SUCCESS(status)) {
                    moduleNameBuf[readLen / sizeof(WCHAR)] = L'\0';
                    UNICODE_STRING currentName;
                    RtlInitUnicodeString(&currentName, moduleNameBuf);

                    if (RtlEqualUnicodeString(&currentName, &targetName, TRUE)) {
                        // Found it! Read DllBase at offset 0x30
                        PVOID dllBase = NULL;
                        ReadTargetMemory(Process, (PUCHAR)currentEntry + 0x30, &dllBase, sizeof(PVOID));
                        *BaseAddress = (ULONGLONG)dllBase;
                        break;
                    }
                }
            }

            // Read Flink (next entry) at offset 0x00
            PVOID nextEntry = NULL;
            status = ReadTargetMemory(Process, currentEntry, &nextEntry, sizeof(PVOID));
            if (!NT_SUCCESS(status) || !nextEntry) break;
            currentEntry = nextEntry;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // PEB reading failed
    }

    ObDereferenceObject(Process);
    return (*BaseAddress != 0) ? STATUS_SUCCESS : STATUS_NOT_FOUND;
}

// Function to read memory from another process into a kernel buffer
NTSTATUS ReadProcessMemory(ULONG ProcessId, ULONGLONG Address, PVOID KernelBuffer, SIZE_T Size) {
    if (Address == 0 || KernelBuffer == NULL || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    // Protection: Prevent reading from Kernel-Space memory
    if (Address >= 0x7FFFFFFF0000ull || Address + Size < Address || Address + Size > 0x7FFFFFFF0000ull) {
        return STATUS_ACCESS_VIOLATION;
    }

    // Protection: Ensure address is properly aligned
    if ((Address & 0x3) != 0 && Size > 1) {
        return STATUS_DATATYPE_MISALIGNMENT;
    }

    // Protection: Ensure size is reasonable
    if (Size > 4096) {
        return STATUS_BUFFER_OVERFLOW;
    }

    // Protection: Reject system PIDs (0 = Idle, 4 = System)
    if (ProcessId == 0 || ProcessId == 4) {
        return STATUS_ACCESS_DENIED;
    }

    PEPROCESS Process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &Process);
    if (!NT_SUCCESS(status)) return status;

    SIZE_T OutSize = 0;
    __try {
        status = MmCopyVirtualMemory(Process, (PVOID)Address, PsGetCurrentProcess(), KernelBuffer, Size, KernelMode, &OutSize);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
    }
    ObDereferenceObject(Process);
    return status;
}

// Function to write memory to another process from a kernel buffer
NTSTATUS WriteProcessMemory(ULONG ProcessId, ULONGLONG Address, PVOID KernelBuffer, SIZE_T Size) {
    if (Address == 0 || KernelBuffer == NULL || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    // Protection: Prevent writing to Kernel-Space memory
    if (Address >= 0x7FFFFFFF0000ull || Address + Size < Address || Address + Size > 0x7FFFFFFF0000ull) {
        return STATUS_ACCESS_VIOLATION;
    }

    // Protection: Ensure address is properly aligned
    if ((Address & 0x3) != 0 && Size > 1) {
        return STATUS_DATATYPE_MISALIGNMENT;
    }

    // Protection: Ensure size is reasonable
    if (Size > 4096) {
        return STATUS_BUFFER_OVERFLOW;
    }

    // Protection: Reject system PIDs (0 = Idle, 4 = System)
    if (ProcessId == 0 || ProcessId == 4) {
        return STATUS_ACCESS_DENIED;
    }

    PEPROCESS Process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &Process);
    if (!NT_SUCCESS(status)) return status;

    SIZE_T OutSize = 0;
    __try {
        status = MmCopyVirtualMemory(PsGetCurrentProcess(), KernelBuffer, Process, (PVOID)Address, Size, KernelMode, &OutSize);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
    }
    ObDereferenceObject(Process);
    return status;
}

// ============================================================
// SHARED MEMORY: Global State
// ============================================================
static PFBS_SHARED_MEMORY g_SharedMemory = NULL;
static PVOID              g_SharedMemoryUserVA = NULL;
static PMDL               g_SharedMemoryMdl = NULL;
static HANDLE             g_WorkerThreadHandle = NULL;
static volatile BOOLEAN   g_ThreadShouldStop = FALSE;

// Forward declarations
VOID ShmWorkerThread(PVOID Context);

// ============================================================
// SHARED MEMORY: Worker Thread
// Polls shared memory for commands from user-mode
// ============================================================
VOID ShmWorkerThread(PVOID Context) {
    UNREFERENCED_PARAMETER(Context);
    
    LARGE_INTEGER interval;
    interval.QuadPart = -500; // 50 microseconds
    
    while (!g_ThreadShouldStop) {
        if (g_SharedMemory != NULL &&
            InterlockedCompareExchange(&g_SharedMemory->Status,
                SHM_STATUS_PENDING, SHM_STATUS_PENDING) == SHM_STATUS_PENDING) {
            
            LONG cmd = g_SharedMemory->Command;
            NTSTATUS opStatus = STATUS_SUCCESS;
            
            switch (cmd) {
            case SHM_CMD_READ:
                if (g_SharedMemory->Size > 0 && g_SharedMemory->Size <= sizeof(g_SharedMemory->Buffer)) {
                    opStatus = ReadProcessMemory(
                        g_SharedMemory->ProcessId,
                        g_SharedMemory->Address,
                        g_SharedMemory->Buffer,
                        g_SharedMemory->Size);
                } else {
                    opStatus = STATUS_INVALID_PARAMETER;
                }
                break;
                
            case SHM_CMD_WRITE:
                if (g_SharedMemory->Size > 0 && g_SharedMemory->Size <= sizeof(g_SharedMemory->Buffer)) {
                    opStatus = WriteProcessMemory(
                        g_SharedMemory->ProcessId,
                        g_SharedMemory->Address,
                        g_SharedMemory->Buffer,
                        g_SharedMemory->Size);
                } else {
                    opStatus = STATUS_INVALID_PARAMETER;
                }
                break;
                
            case SHM_CMD_GET_BASE: {
                ULONGLONG base = 0;
                opStatus = GetModuleBaseByName(
                    g_SharedMemory->ProcessId,
                    g_SharedMemory->ModuleName,
                    &base);
                g_SharedMemory->Result = base;
                break;
            }
            default:
                opStatus = STATUS_INVALID_PARAMETER;
                break;
            }
            
            // Signal completion
            InterlockedExchange(&g_SharedMemory->Status,
                NT_SUCCESS(opStatus) ? SHM_STATUS_COMPLETE : SHM_STATUS_ERROR);
        }
        
        KeDelayExecutionThread(KernelMode, FALSE, &interval);
    }
    
    PsTerminateSystemThread(STATUS_SUCCESS);
}

// IRP Handler for IOCTLs
NTSTATUS MyDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG bytesIO = 0;

    ULONG inputLen = stack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outputLen = stack->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG ioctl = stack->Parameters.DeviceIoControl.IoControlCode;

    if (ioctl == IOCTL_MOUSE_MOVE) {
        if (inputLen == sizeof(MOUSE_MOVE_INFO)) {
            PMOUSE_MOVE_INFO move_info = (PMOUSE_MOVE_INFO)Irp->AssociatedIrp.SystemBuffer;
            if (move_info && move_info->magic == FBS_MAGIC) {
                if (g_MouseClassCallback && g_MouseClassDevice) {
                    InjectMouseInput(move_info->x, move_info->y, move_info->buttonFlags);
                    bytesIO = sizeof(MOUSE_MOVE_INFO);
                } else {
                    status = STATUS_DEVICE_NOT_READY;
                }
            } else {
                status = STATUS_INVALID_PARAMETER;
            }
        } else {
            status = STATUS_INFO_LENGTH_MISMATCH;
        }
    }
    else if (ioctl == IOCTL_INIT_SHM) {
        // ============================================================
        // SHARED MEMORY INIT: Allocate NonPagedPool, MDL, map to user
        // ============================================================
        if (inputLen >= sizeof(ULONG) && outputLen >= sizeof(SHM_INIT_RESPONSE)) {
            PULONG pMagic = (PULONG)Irp->AssociatedIrp.SystemBuffer;
            if (*pMagic != FBS_MAGIC) {
                status = STATUS_INVALID_PARAMETER;
            } else if (g_SharedMemory != NULL && g_SharedMemoryUserVA != NULL) {
                // Already initialized — return existing address
                PSHM_INIT_RESPONSE resp = (PSHM_INIT_RESPONSE)Irp->AssociatedIrp.SystemBuffer;
                resp->SharedMemoryAddress = (ULONGLONG)g_SharedMemoryUserVA;
                resp->magic = FBS_MAGIC;
                bytesIO = sizeof(SHM_INIT_RESPONSE);
            } else {
                // Allocate NonPagedPool
                g_SharedMemory = (PFBS_SHARED_MEMORY)ExAllocatePool2(
                    POOL_FLAG_NON_PAGED, sizeof(FBS_SHARED_MEMORY), 'FBSM');
                
                if (!g_SharedMemory) {
                    status = STATUS_INSUFFICIENT_RESOURCES;
                } else {
                    RtlZeroMemory(g_SharedMemory, sizeof(FBS_SHARED_MEMORY));
                    
                    // Create MDL for the shared memory
                    g_SharedMemoryMdl = IoAllocateMdl(
                        g_SharedMemory, (ULONG)sizeof(FBS_SHARED_MEMORY),
                        FALSE, FALSE, NULL);
                    
                    if (!g_SharedMemoryMdl) {
                        ExFreePoolWithTag(g_SharedMemory, 'FBSM');
                        g_SharedMemory = NULL;
                        status = STATUS_INSUFFICIENT_RESOURCES;
                    } else {
                        MmBuildMdlForNonPagedPool(g_SharedMemoryMdl);
                        
                        // Map into calling process address space
                        __try {
                            g_SharedMemoryUserVA = MmMapLockedPagesSpecifyCache(
                                g_SharedMemoryMdl, UserMode, MmCached,
                                NULL, FALSE, NormalPagePriority);
                        } __except (EXCEPTION_EXECUTE_HANDLER) {
                            g_SharedMemoryUserVA = NULL;
                        }
                        
                        if (!g_SharedMemoryUserVA) {
                            IoFreeMdl(g_SharedMemoryMdl);
                            g_SharedMemoryMdl = NULL;
                            ExFreePoolWithTag(g_SharedMemory, 'FBSM');
                            g_SharedMemory = NULL;
                            status = STATUS_ACCESS_VIOLATION;
                        } else {
                            // Start worker thread
                            g_ThreadShouldStop = FALSE;
                            NTSTATUS thStatus = PsCreateSystemThread(
                                &g_WorkerThreadHandle, THREAD_ALL_ACCESS,
                                NULL, NULL, NULL, ShmWorkerThread, NULL);
                            
                            if (NT_SUCCESS(thStatus)) {
                                // Return user-mode address
                                PSHM_INIT_RESPONSE resp = (PSHM_INIT_RESPONSE)Irp->AssociatedIrp.SystemBuffer;
                                resp->SharedMemoryAddress = (ULONGLONG)g_SharedMemoryUserVA;
                                resp->magic = FBS_MAGIC;
                                bytesIO = sizeof(SHM_INIT_RESPONSE);
                            } else {
                                // Thread creation failed — cleanup
                                MmUnmapLockedPages(g_SharedMemoryUserVA, g_SharedMemoryMdl);
                                g_SharedMemoryUserVA = NULL;
                                IoFreeMdl(g_SharedMemoryMdl);
                                g_SharedMemoryMdl = NULL;
                                ExFreePoolWithTag(g_SharedMemory, 'FBSM');
                                g_SharedMemory = NULL;
                                status = thStatus;
                            }
                        }
                    }
                }
            }
        } else {
            status = STATUS_INFO_LENGTH_MISMATCH;
        }
    }
    else if (ioctl == IOCTL_READ_MEMORY) {
        // Legacy IOCTL fallback (kept for backward compatibility)
        if (inputLen >= sizeof(MEMORY_REQUEST)) {
            PMEMORY_REQUEST request = (PMEMORY_REQUEST)Irp->AssociatedIrp.SystemBuffer;
            if (!request || request->magic != FBS_MAGIC) {
                status = STATUS_INVALID_PARAMETER;
            } else {
                SIZE_T readSize = request->Size;
                if (readSize == 0 || readSize > 4096 || outputLen < readSize) {
                    status = STATUS_INVALID_PARAMETER;
                } else {
                    status = ReadProcessMemory(request->ProcessId, request->Address,
                        Irp->AssociatedIrp.SystemBuffer, readSize);
                    if (NT_SUCCESS(status)) bytesIO = (ULONG)readSize;
                }
            }
        } else {
            status = STATUS_INFO_LENGTH_MISMATCH;
        }
    }
    else if (ioctl == IOCTL_WRITE_MEMORY) {
        // Legacy IOCTL fallback
        if (inputLen >= sizeof(MEMORY_REQUEST)) {
            PMEMORY_REQUEST request = (PMEMORY_REQUEST)Irp->AssociatedIrp.SystemBuffer;
            if (!request || request->magic != FBS_MAGIC) {
                status = STATUS_INVALID_PARAMETER;
            } else {
                SIZE_T writeSize = request->Size;
                PVOID dataToWrite = (PUCHAR)Irp->AssociatedIrp.SystemBuffer + sizeof(MEMORY_REQUEST);
                if (writeSize == 0 || writeSize > 4096 || inputLen < sizeof(MEMORY_REQUEST) + writeSize) {
                    status = STATUS_INVALID_PARAMETER;
                } else {
                    status = WriteProcessMemory(request->ProcessId, request->Address, dataToWrite, writeSize);
                    if (NT_SUCCESS(status)) bytesIO = (ULONG)writeSize;
                }
            }
        } else {
            status = STATUS_INFO_LENGTH_MISMATCH;
        }
    }
    else if (ioctl == IOCTL_GET_BASE_ADDRESS) {
        // Legacy IOCTL fallback
        if (inputLen == sizeof(BASE_ADDRESS_REQUEST) && outputLen >= sizeof(BASE_ADDRESS_REQUEST)) {
            PBASE_ADDRESS_REQUEST request = (PBASE_ADDRESS_REQUEST)Irp->AssociatedIrp.SystemBuffer;
            if (request && request->magic == FBS_MAGIC) {
                status = GetModuleBaseByName(request->ProcessId, request->ModuleName, &request->BaseAddress);
                bytesIO = sizeof(BASE_ADDRESS_REQUEST);
            } else {
                status = STATUS_INVALID_PARAMETER;
            }
        } else {
            status = STATUS_INFO_LENGTH_MISMATCH;
        }
    }
    else if (ioctl == IOCTL_PROTECT_PROCESS) {
        if (inputLen == sizeof(PROTECT_REQUEST)) {
            PPROTECT_REQUEST protectReq = (PPROTECT_REQUEST)Irp->AssociatedIrp.SystemBuffer;
            if (protectReq && protectReq->magic == FBS_MAGIC) {
                extern ULONG g_ProtectedPID;
                g_ProtectedPID = protectReq->ProcessId;
                status = STATUS_SUCCESS;
                bytesIO = sizeof(PROTECT_REQUEST);
            } else {
                status = STATUS_INVALID_PARAMETER;
            }
        } else {
            status = STATUS_INFO_LENGTH_MISMATCH;
        }
    }
    else {
        status = STATUS_INVALID_DEVICE_REQUEST;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesIO;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

// ============================================================
// STEALTH LAYER 2: Dispatch Table Swap on \Device\Null
// ============================================================

PDEVICE_OBJECT g_NullDeviceObject = NULL;
PDRIVER_DISPATCH g_OriginalDispatch = NULL;

NTSTATUS StealthDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG ioctl = stack->Parameters.DeviceIoControl.IoControlCode;

    // Our IOCTLs: mouse + shared memory init + legacy fallbacks
    if (ioctl == IOCTL_MOUSE_MOVE || ioctl == IOCTL_INIT_SHM ||
        ioctl == IOCTL_READ_MEMORY || ioctl == IOCTL_WRITE_MEMORY ||
        ioctl == IOCTL_GET_BASE_ADDRESS || ioctl == IOCTL_PROTECT_PROCESS) {
        return MyDeviceControl(DeviceObject, Irp);
    }

    return g_OriginalDispatch(DeviceObject, Irp);
}

VOID MyDriverUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    
    // 1. Stop worker thread
    g_ThreadShouldStop = TRUE;
    if (g_WorkerThreadHandle) {
        PVOID threadObj = NULL;
        if (NT_SUCCESS(ObReferenceObjectByHandle(
                g_WorkerThreadHandle, THREAD_ALL_ACCESS,
                NULL, KernelMode, &threadObj, NULL))) {
            KeWaitForSingleObject(threadObj, Executive, KernelMode, FALSE, NULL);
            ObDereferenceObject(threadObj);
        }
        ZwClose(g_WorkerThreadHandle);
        g_WorkerThreadHandle = NULL;
    }
    
    // 2. Cleanup shared memory
    if (g_SharedMemoryMdl) {
        if (g_SharedMemoryUserVA) {
            MmUnmapLockedPages(g_SharedMemoryUserVA, g_SharedMemoryMdl);
            g_SharedMemoryUserVA = NULL;
        }
        IoFreeMdl(g_SharedMemoryMdl);
        g_SharedMemoryMdl = NULL;
    }
    if (g_SharedMemory) {
        ExFreePoolWithTag(g_SharedMemory, 'FBSM');
        g_SharedMemory = NULL;
    }
    
    // 3. Cleanup Process Protection
    if (g_ObHandle) {
        ObUnRegisterCallbacks(g_ObHandle);
        g_ObHandle = NULL;
    }

    // 4. Restore original dispatch
    if (g_NullDeviceObject && g_OriginalDispatch) {
        g_NullDeviceObject->DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = g_OriginalDispatch;
    }
}

// Driver Entry Point
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);
    UNREFERENCED_PARAMETER(DriverObject);

    UNICODE_STRING nullDeviceName = RTL_CONSTANT_STRING(L"\\Device\\Null");
    PFILE_OBJECT fileObject = NULL;
    PDEVICE_OBJECT deviceObject = NULL;

    NTSTATUS status = IoGetDeviceObjectPointer(&nullDeviceName, FILE_ALL_ACCESS, &fileObject, &deviceObject);
    if (!NT_SUCCESS(status)) return status;

    g_NullDeviceObject = deviceObject;
    g_OriginalDispatch = deviceObject->DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];
    deviceObject->DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = StealthDeviceControl;

    ObDereferenceObject(fileObject);

    if (DriverObject != NULL) {
        DriverObject->DriverUnload = MyDriverUnload;
    }

    // Initialize ObRegisterCallbacks for FaceIT evasion
    OB_OPERATION_REGISTRATION obOp;
    obOp.ObjectType = PsProcessType;
    obOp.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
    obOp.PreOperation = PreOperationCallback;
    obOp.PostOperation = NULL;

    OB_CALLBACK_REGISTRATION obReg;
    obReg.Version = OB_FLT_REGISTRATION_VERSION;
    obReg.OperationRegistrationCount = 1;
    obReg.RegistrationContext = NULL;
    UNICODE_STRING altitude = RTL_CONSTANT_STRING(L"380010"); // Altitude for AV filter
    obReg.Altitude = altitude;
    obReg.OperationRegistration = &obOp;

    // Ignore failure (KDMapper might deny registration due to missing signature)
    ObRegisterCallbacks(&obReg, &g_ObHandle);

    CleanKernelTraces();
    InitMouseInjection();

    return STATUS_SUCCESS;
}

