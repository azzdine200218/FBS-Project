#pragma once
#include <ntddk.h>

#define FBS_MAGIC 0xFB542069

#define IOCTL_MOUSE_MOVE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_READ_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WRITE_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_BASE_ADDRESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x903, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_BULK_READ CTL_CODE(FILE_DEVICE_UNKNOWN, 0x904, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INIT_SHM CTL_CODE(FILE_DEVICE_UNKNOWN, 0x905, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PROTECT_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x906, METHOD_BUFFERED, FILE_ANY_ACCESS)

// ============================================================
// Shared Memory Communication (Phase 3 Stealth)
// ============================================================
#define SHM_CMD_NONE      0
#define SHM_CMD_READ      1
#define SHM_CMD_WRITE     2
#define SHM_CMD_GET_BASE  3

#define SHM_STATUS_IDLE     0
#define SHM_STATUS_PENDING  1
#define SHM_STATUS_COMPLETE 2
#define SHM_STATUS_ERROR    3

typedef struct _FBS_SHARED_MEMORY {
    volatile LONG   Command;         // SHM_CMD_*
    volatile LONG   Status;          // SHM_STATUS_*
    ULONG           ProcessId;
    ULONGLONG       Address;
    SIZE_T          Size;
    ULONGLONG       Result;          // For base address results
    WCHAR           ModuleName[256];
    UCHAR           Buffer[2048];    // Read/Write data
} FBS_SHARED_MEMORY, *PFBS_SHARED_MEMORY;

// IOCTL_INIT_SHM response — returns mapped user-mode address
typedef struct _SHM_INIT_RESPONSE {
    ULONGLONG SharedMemoryAddress;
    ULONG magic;
} SHM_INIT_RESPONSE, *PSHM_INIT_RESPONSE;

typedef struct _MOUSE_MOVE_INFO {
    LONG x;
    LONG y;
    USHORT buttonFlags;
    ULONG magic;
} MOUSE_MOVE_INFO, *PMOUSE_MOVE_INFO;

typedef struct _MEMORY_REQUEST {
    ULONG ProcessId;
    ULONGLONG Address;
    SIZE_T Size;
    ULONG magic;
} MEMORY_REQUEST, *PMEMORY_REQUEST;

typedef struct _BASE_ADDRESS_REQUEST {
    ULONG ProcessId;
    WCHAR ModuleName[256];
    ULONGLONG BaseAddress;
    ULONG magic;
} BASE_ADDRESS_REQUEST, *PBASE_ADDRESS_REQUEST;

typedef struct _PROTECT_REQUEST {
    ULONG ProcessId;
    ULONG magic;
} PROTECT_REQUEST, *PPROTECT_REQUEST;

typedef struct _BULK_READ_REQUEST {
    ULONG ProcessId;
    ULONGLONG Address;
    SIZE_T Size;
    ULONG magic;
    char Buffer[4096];
} BULK_READ_REQUEST, *PBULK_READ_REQUEST;

typedef struct _MOUSE_INPUT_DATA {
    USHORT UnitId;
    USHORT Flags;
    union {
        ULONG Buttons;
        struct {
            USHORT ButtonFlags;
            USHORT ButtonData;
        };
    };
    ULONG RawButtons;
    LONG LastX;
    LONG LastY;
    ULONG ExtraInformation;
} MOUSE_INPUT_DATA, *PMOUSE_INPUT_DATA;

// Mouse button flags for MOUSE_INPUT_DATA injection
#define MOUSE_LEFT_BUTTON_DOWN   0x0001
#define MOUSE_LEFT_BUTTON_UP     0x0002
#define MOUSE_RIGHT_BUTTON_DOWN  0x0004
#define MOUSE_RIGHT_BUTTON_UP    0x0008
#define MOUSE_MIDDLE_BUTTON_DOWN 0x0010
#define MOUSE_MIDDLE_BUTTON_UP   0x0020

#define MOUSE_MOVE_RELATIVE 0
#define MOUSE_MOVE_ABSOLUTE 1

#ifndef IO_TYPE_DEVICE
#define IO_TYPE_DEVICE 3
#endif

// Mouse class service callback function pointer type
// This is the internal function that MouClass exposes to port drivers
typedef VOID (*MouseClassServiceCallbackFn)(
    PDEVICE_OBJECT DeviceObject,
    PMOUSE_INPUT_DATA InputDataStart,
    PMOUSE_INPUT_DATA InputDataEnd,
    PULONG InputDataConsumed
);

// Undocumented function for finding driver objects by name
NTSTATUS NTAPI ObReferenceObjectByName(
    PUNICODE_STRING ObjectName,
    ULONG Attributes,
    PACCESS_STATE AccessState,
    ACCESS_MASK DesiredAccess,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode,
    PVOID ParseContext,
    PVOID* Object
);

extern POBJECT_TYPE *IoDriverObjectType;
