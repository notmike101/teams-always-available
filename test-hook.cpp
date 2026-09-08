#define main ControllerMain
#include "app.cpp"
#undef main
#include "hookdll.cpp"
#include <assert.h>

static DWORD WINAPI SlowCall(LPVOID done)
{
    Sleep(16000); // Deliberately exceed the production remote-call deadline.
    SetEvent((HANDLE)done);
    return 1;
}

int main(int argc, char **argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc > 1 && !strcmp(argv[1], "--target")) {
        // Real second process: a broken remote address or missing patch fails this check.
        auto real = (ULONG_PTR)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetLastInputInfo");
        BYTE *base = (BYTE *)GetModuleHandleW(NULL);
        auto dos = (IMAGE_DOS_HEADER *)base;
        auto nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
        auto desc = (IMAGE_IMPORT_DESCRIPTOR *)(base +
            nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        ULONG_PTR volatile *inputSlot = NULL;
        for (; desc->Name; ++desc) {
            auto slot = (ULONG_PTR *)(base + desc->FirstThunk);
            for (; *slot; ++slot) if (*slot == real) inputSlot = slot;
        }
        assert(inputSlot);
        assert(argc == 4);
        HANDLE ready = (HANDLE)(ULONG_PTR)_strtoui64(argv[2], NULL, 10);
        HANDLE done = (HANDLE)(ULONG_PTR)_strtoui64(argv[3], NULL, 10);
        assert(SetEvent(ready));
        assert(WaitForSingleObject(done, 10000) == WAIT_OBJECT_0);
        assert(*inputSlot != real);
        LASTINPUTINFO info = {sizeof(info), 0};
        assert(GetLastInputInfo(&info));
        assert(GetTickCount() - info.dwTime < 100);
        return 0;
    }
    LASTINPUTINFO invalid = {};
    assert(!HookedGetLastInputInfo(&invalid));
    assert(!HookedGetLastInputInfo(NULL));
    LASTINPUTINFO info = {sizeof(info), 0};
    InterlockedExchange64(&refreshed, (LONG64)GetTickCount64());
    assert(HookedGetLastInputInfo(&info));
    assert(GetTickCount() - info.dwTime < 100);
    // Force an expired lease without sleeping for 90 seconds; compare with the real API.
    InterlockedExchange64(&refreshed, (LONG64)(GetTickCount64() - 90000));
    LASTINPUTINFO before = {sizeof(before), 0}, after = {sizeof(after), 0};
    assert(GetLastInputInfo(&before));
    assert(HookedGetLastInputInfo(&info));
    assert(GetLastInputInfo(&after));
    assert(info.dwTime == before.dwTime || info.dwTime == after.dwTime);
    // A stale lease must not reactivate after one full 32-bit tick-count wrap.
    InterlockedExchange64(&refreshed, (LONG64)(GetTickCount64() - 0x100000000ULL));
    assert(GetLastInputInfo(&before));
    assert(HookedGetLastInputInfo(&info));
    assert(GetLastInputInfo(&after));
    assert(info.dwTime == before.dwTime || info.dwTime == after.dwTime);

    // A loaded-image fixture with no OriginalFirstThunk: still patch its resolved IAT.
    BYTE *image = (BYTE *)VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    assert(image);
    auto dos = (IMAGE_DOS_HEADER *)image;
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 128;
    auto nt = (IMAGE_NT_HEADERS *)(image + 128);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = 512;
    auto desc = (IMAGE_IMPORT_DESCRIPTOR *)(image + 512);
    desc->Name = 768;
    desc->FirstThunk = 1024;
    auto slot = (ULONG_PTR *)(image + 1024);
    *slot = (ULONG_PTR)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetLastInputInfo");
    DWORD old;
    assert(VirtualProtect(image, 4096, PAGE_READONLY, &old));
    assert(HookModule((HMODULE)image));
    assert(*slot == (ULONG_PTR)HookedGetLastInputInfo);
    assert(HookModule((HMODULE)image)); // repeated refresh must remain valid
    MEMORY_BASIC_INFORMATION memory;
    assert(VirtualQuery(slot, &memory, sizeof(memory)));
    assert(memory.Protect == PAGE_READONLY);
    VirtualFree(image, 0, MEM_RELEASE);

    HANDLE completed = CreateEventW(NULL, TRUE, FALSE, NULL);
    assert(completed);
    DWORD outcome = 0;
    assert(RemoteCall(GetCurrentProcess(), SlowCall, completed, &outcome) == ERROR_TIMEOUT);
    assert(WaitForSingleObject(completed, 5000) == WAIT_OBJECT_0);
    CloseHandle(completed);

    wchar_t path[MAX_PATH];
    assert(GetModuleFileNameW(NULL, path, MAX_PATH));
    SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
    HANDLE ready = CreateEventW(&security, TRUE, FALSE, NULL);
    HANDLE done = CreateEventW(&security, TRUE, FALSE, NULL);
    assert(ready && done);
    wchar_t command[MAX_PATH + 80];
    assert(swprintf_s(command, L"\"%ls\" --target %llu %llu", path,
                     (ULONGLONG)(ULONG_PTR)ready, (ULONGLONG)(ULONG_PTR)done) > 0);
    assert(wcscpy_s(wcsrchr(path, L'\\') + 1, 10, L"hook2.dll") == 0);
    HMODULE dll = LoadLibraryW(path);
    assert(dll);
    FARPROC refresh = GetProcAddress(dll, "RefreshHook");
    assert(refresh);
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child = {};
    assert(CreateProcessW(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &startup, &child));
    assert(WaitForSingleObject(ready, 10000) == WAIT_OBJECT_0);
    DWORD hooked = 0;
    DWORD error = RefreshTeamsHook(child.dwProcessId, path, refresh, &hooked);
    assert(!error && hooked > 0);
    assert(RefreshTeamsHook(child.dwProcessId, path, refresh, &hooked) == ERROR_SUCCESS);
    assert(hooked > 0);
    assert(SetEvent(done));
    assert(WaitForSingleObject(child.hProcess, 15000) == WAIT_OBJECT_0);
    DWORD result;
    assert(GetExitCodeProcess(child.hProcess, &result) && result == 0);
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    CloseHandle(ready);
    CloseHandle(done);
    FreeLibrary(dll);
    puts("PASS: input validation, active/expired lease, clock wrap, IAT without name table, repeat patch, page protection, timeout, real remote injection.");
}
