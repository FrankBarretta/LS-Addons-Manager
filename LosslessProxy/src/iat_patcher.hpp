#pragma once

#include <windows.h>

// IAT patching utilities
namespace IatPatcher {

    // Patch a function in the Import Address Table
    // Module: The module to patch
    // ImportDllName: Name of DLL being imported from (e.g. "kernel32.dll")
    // ImportName: Name of function being imported (e.g. "FindResourceW")
    // NewFunction: Replacement function pointer
    // Returns: Original function pointer
    
    template<typename FuncPtr>
    FuncPtr PatchIat(HMODULE moduleToPatc, const char* importDllName, const char* importName, FuncPtr newFunction) {
        // Get the DOS header
        PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)moduleToPatc;
        if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            return nullptr;
        }

        // Get the NT headers
        PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((BYTE*)moduleToPatc + pDosHeader->e_lfanew);
        if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
            return nullptr;
        }

        // Get the import table directory
        PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)
            ((BYTE*)moduleToPatc + pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

        if (!pImportDesc) {
            return nullptr;
        }

        // Find the import descriptor for the target DLL
        for (; pImportDesc->Name; pImportDesc++) {
            const char* dllName = (const char*)((BYTE*)moduleToPatc + pImportDesc->Name);
            if (_stricmp(dllName, importDllName) != 0) {
                continue;
            }

            // Found the DLL, now find the function
            PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)((BYTE*)moduleToPatc + pImportDesc->FirstThunk);
            PIMAGE_THUNK_DATA pOrigThunk = (PIMAGE_THUNK_DATA)((BYTE*)moduleToPatc + pImportDesc->OriginalFirstThunk);

            for (; pThunk->u1.Function; pThunk++, pOrigThunk++) {
                // Check if this is an imported by name (not by ordinal)
                if (IMAGE_SNAP_BY_ORDINAL(pOrigThunk->u1.Ordinal)) {
                    continue;
                }

                PIMAGE_IMPORT_BY_NAME pImportByName = (PIMAGE_IMPORT_BY_NAME)
                    ((BYTE*)moduleToPatc + pOrigThunk->u1.AddressOfData);

                if (strcmp((const char*)pImportByName->Name, importName) != 0) {
                    continue;
                }

                // Found the function! Patch it
                FuncPtr originalFunc = (FuncPtr)pThunk->u1.Function;

                // Unprotect the memory page
                DWORD oldProtect = 0;
                VirtualProtect(pThunk, sizeof(*pThunk), PAGE_READWRITE, &oldProtect);

                // Patch the IAT entry
                pThunk->u1.Function = (ULONG_PTR)newFunction;

                // Restore the old protection
                VirtualProtect(pThunk, sizeof(*pThunk), oldProtect, &oldProtect);

                return originalFunc;
            }
        }

        return nullptr;
    }
}

