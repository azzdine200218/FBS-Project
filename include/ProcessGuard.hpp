#pragma once
#include <windows.h>

// ============================================================
// STEALTH LAYER 6: Process Self-Defense
// ============================================================

namespace ProcessGuard {

    // Hide Console Window — simple and safe
    inline void HideConsole() {
        HWND console = GetConsoleWindow();
        if (console) {
            ShowWindow(console, SW_HIDE);
        }
    }

    // Erase PE signatures from memory — call AFTER all init
    inline void ErasePEHeader() {
        HMODULE base = GetModuleHandleA(NULL);
        if (!base) return;

        PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)base;
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return;

        PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)base + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return;

        DWORD headerSize = ntHeaders->OptionalHeader.SizeOfHeaders;
        DWORD oldProtect = 0;

        if (VirtualProtect(base, headerSize, PAGE_READWRITE, &oldProtect)) {
            // Zero only signatures
            dosHeader->e_magic = 0;
            ntHeaders->Signature = 0;

            // Zero section names
            PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);
            for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
                memset(section[i].Name, 0, IMAGE_SIZEOF_SHORT_NAME);
            }

            VirtualProtect(base, headerSize, oldProtect, &oldProtect);
        }
    }

    // Safe init — only hide console
    inline void Initialize() {
        HideConsole();
    }

} // namespace ProcessGuard
