#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <vector>

static DWORD FindTeamsPid()
{
    DWORD session;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session)) return 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    std::vector<PROCESSENTRY32W> teams;
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snap, &entry)) {
        do {
            DWORD otherSession;
            if (!_wcsicmp(entry.szExeFile, L"ms-teams.exe") &&
                ProcessIdToSessionId(entry.th32ProcessID, &otherSession) && otherSession == session)
                teams.push_back(entry);
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    for (const auto &candidate : teams) {
        bool child = false;
        for (const auto &parent : teams)
            if (parent.th32ProcessID == candidate.th32ParentProcessID) child = true;
        if (!child) return candidate.th32ProcessID;
    }
    return 0;
}

static BYTE *RemoteModule(DWORD pid, const wchar_t *path)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snap == INVALID_HANDLE_VALUE) return NULL;
    MODULEENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    BYTE *base = NULL;
    if (Module32FirstW(snap, &entry)) {
        do {
            if (!_wcsicmp(entry.szExePath, path)) { base = entry.modBaseAddr; break; }
        } while (Module32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return base;
}

// Resolve the containing module, including forwarded kernel32 exports, in the target.
static LPTHREAD_START_ROUTINE RemoteFunction(DWORD pid, FARPROC function)
{
    HMODULE local;
    wchar_t path[MAX_PATH];
    if (!function || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)function, &local)) return NULL;
    DWORD length = GetModuleFileNameW(local, path, MAX_PATH);
    if (!length || length >= MAX_PATH) return NULL;
    BYTE *remote = RemoteModule(pid, path);
    return remote ? (LPTHREAD_START_ROUTINE)(remote + ((BYTE *)function - (BYTE *)local)) : NULL;
}

// ERROR_TIMEOUT means the remote thread may still be using its argument.
static DWORD RemoteCall(HANDLE process, LPTHREAD_START_ROUTINE function, LPVOID argument, DWORD *result)
{
    if (!function) return ERROR_PROC_NOT_FOUND;
    HANDLE thread = CreateRemoteThread(process, NULL, 0, function, argument, 0, NULL);
    if (!thread) return GetLastError();
    DWORD wait = WaitForSingleObject(thread, 15000);
    DWORD error = ERROR_TIMEOUT;
    if (wait == WAIT_OBJECT_0)
        error = GetExitCodeThread(thread, result) ? ERROR_SUCCESS : GetLastError();
    CloseHandle(thread);
    return error;
}

static DWORD RefreshTeamsHook(DWORD pid, const wchar_t *path, FARPROC refresh, DWORD *hooked)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_CREATE_THREAD, FALSE, pid);
    if (!process) return GetLastError();
    USHORT machine, native;
    if (!IsWow64Process2(process, &machine, &native) || machine != IMAGE_FILE_MACHINE_UNKNOWN ||
        native != IMAGE_FILE_MACHINE_AMD64) {
        CloseHandle(process);
        return ERROR_BAD_EXE_FORMAT;
    }
    DWORD error = ERROR_SUCCESS;
    if (!RemoteModule(pid, path)) {
        SIZE_T size = (wcslen(path) + 1) * sizeof(wchar_t);
        void *buffer = VirtualAllocEx(process, NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!buffer) error = GetLastError();
        else {
            if (!WriteProcessMemory(process, buffer, path, size, NULL)) error = GetLastError();
            else {
                DWORD ignored;
                error = RemoteCall(process, RemoteFunction(pid,
                    GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW")), buffer, &ignored);
            }
            // On timeout leave the argument valid until Teams exits. Do not retry this PID.
            if (error != ERROR_TIMEOUT) VirtualFreeEx(process, buffer, 0, MEM_RELEASE);
        }
    }
    if (!error) {
        // A thread exit code is only 32 bits; verify the actual 64-bit module via snapshot.
        error = RemoteCall(process, RemoteFunction(pid, refresh), NULL, hooked);
        if (!error && (!*hooked || *hooked >= 0x80000000)) error = ERROR_FUNCTION_FAILED;
    }
    CloseHandle(process);
    return error;
}

static void ReportError(const wchar_t *action, DWORD error)
{
    wchar_t message[512] = {};
    DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                  NULL, error, 0, message, 512, NULL);
    while (length && (message[length - 1] == L'\r' || message[length - 1] == L'\n'))
        message[--length] = L'\0';
    fwprintf(stderr, L"%ls: %ls (0x%08lX).\n", action, message, error);
}

int main()
{
    _setmode(_fileno(stderr), _O_U8TEXT);
    // One controller per interactive session; avoid duplicate loads and refresh races.
    HANDLE instance = CreateMutexW(NULL, FALSE, L"Local\\TeamsPresenceKeepAwake");
    if (!instance) { ReportError(L"CreateMutex", GetLastError()); return 1; }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        fputws(L"Keep-awake is already running in this session.\n", stderr);
        CloseHandle(instance);
        return 1;
    }
    wchar_t hookPath[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, hookPath, MAX_PATH);
    wchar_t *slash = length && length < MAX_PATH ? wcsrchr(hookPath, L'\\') : NULL;
    if (!slash || wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - hookPath), L"hook2.dll")) {
        fputws(L"Executable path is too long; use a shorter folder path.\n", stderr);
        return 1;
    }
    // DllMain only records its module handle; no local hooks are installed.
    HMODULE hook = LoadLibraryW(hookPath);
    FARPROC refresh = hook ? GetProcAddress(hook, "RefreshHook") : NULL;
    if (!refresh) { ReportError(L"Load hook2.dll beside app.exe", GetLastError()); return 1; }
    wchar_t teams[32768];
    length = ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Microsoft\\WindowsApps\\ms-teams.exe",
                                        teams, 32768);
    if (!length || length > 32768) { fputws(L"Cannot resolve Teams launcher.\n", stderr); return 1; }
    if (!SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED)) {
        ReportError(L"Windows keep-awake request", GetLastError()); return 1;
    }
    puts("Windows/display keep-awake active. Requesting Teams Available every 60 seconds.");
    puts("Ctrl+C to stop; idle suppression expires within 90 seconds of the last refresh.");
    DWORD timedOutPid = 0;
    for (;;) {
        wchar_t command[] = L"\"ms-teams.exe\" --set-presence-to-available";
        STARTUPINFOW startup = {};
        PROCESS_INFORMATION process = {};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        if (CreateProcessW(teams, command, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                           NULL, NULL, &startup, &process)) {
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            puts("Teams Available command launched (not a presence confirmation).");
        } else ReportError(L"Teams launch failed; keeping Windows awake and retrying", GetLastError());

        DWORD pid = FindTeamsPid();
        if (!pid) puts("Teams not found in this session; retrying in 60 seconds.");
        else if (pid != timedOutPid) {
            DWORD hooked = 0;
            DWORD error = RefreshTeamsHook(pid, hookPath, refresh, &hooked);
            if (!error) printf("Idle hook confirmed: %lu module(s), Teams pid %lu.\n", hooked, pid);
            else {
                ReportError(L"Teams idle hook failed", error);
                if (error == ERROR_TIMEOUT) {
                    timedOutPid = pid;
                    puts("Remote call timed out; restart Teams to retry safely. Windows stays awake.");
                }
            }
        }
        fflush(stdout);
        // Windows releases this thread's power request on process exit.
        Sleep(60000);
    }
}
