// Only this process's import tables are changed; user32.dll stays untouched.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

static HMODULE self;
static volatile LONG64 refreshed;
static SRWLOCK installLock = SRWLOCK_INIT;

static BOOL WINAPI HookedGetLastInputInfo(LASTINPUTINFO *info)
{
    if (!info || info->cbSize != sizeof(*info)) return FALSE;
    ULONGLONG now = GetTickCount64();
    if (now - (ULONGLONG)InterlockedCompareExchange64(&refreshed, 0, 0) < 90000) {
        info->dwTime = (DWORD)now;
        return TRUE;
    }
    // Our own import table is excluded, so this calls the real Windows API.
    return GetLastInputInfo(info);
}

static BOOL HookModule(HMODULE mod)
{
    if (mod == self) return FALSE;
    BYTE *base = (BYTE *)mod;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < sizeof(*dos)) return FALSE;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return FALSE;
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return FALSE;
    auto real = (ULONG_PTR)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetLastInputInfo");
    BOOL hooked = FALSE;
    // Match resolved pointers: also works when OriginalFirstThunk is absent.
    auto desc = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir.VirtualAddress);
    for (; desc->Name; ++desc) {
        if (!desc->FirstThunk) continue;
        auto slot = (ULONG_PTR *)(base + desc->FirstThunk);
        for (; *slot; ++slot) {
            if (*slot == (ULONG_PTR)HookedGetLastInputInfo) { hooked = TRUE; continue; }
            if (*slot != real) continue;
            DWORD oldProtect;
            if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtect)) continue;
            InterlockedExchangePointer((PVOID volatile *)slot, (PVOID)HookedGetLastInputInfo);
            DWORD ignored;
            if (!VirtualProtect(slot, sizeof(*slot), oldProtect, &ignored)) return FALSE;
            hooked = TRUE;
        }
    }
    return hooked;
}

// Called explicitly after LoadLibrary has completed, outside the loader lock.
// Repeating this picks up modules loaded later and renews a 90-second lease.
extern "C" __declspec(dllexport) DWORD WINAPI RefreshHook(LPVOID)
{
    AcquireSRWLockExclusive(&installLock);
    DWORD hooked = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W entry = {};
        entry.dwSize = sizeof(entry);
        if (Module32FirstW(snap, &entry)) {
            do {
                HMODULE module;
                // Hold a reference while reading/patching: modules may unload concurrently.
                if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                                      (LPCWSTR)entry.modBaseAddr, &module)) {
                    if (HookModule(module)) ++hooked;
                    FreeLibrary(module);
                }
            } while (Module32NextW(snap, &entry));
        }
        CloseHandle(snap);
    }
    if (hooked) InterlockedExchange64(&refreshed, (LONG64)GetTickCount64());
    ReleaseSRWLockExclusive(&installLock);
    return hooked;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) self = module;
    return TRUE;
}
