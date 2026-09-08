#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>

int main(void)
{
    _setmode(_fileno(stderr), _O_U8TEXT);
    wchar_t teams[32768];
    DWORD length = ExpandEnvironmentStringsW(
        L"%LOCALAPPDATA%\\Microsoft\\WindowsApps\\ms-teams.exe",
        teams,
        (DWORD)(sizeof teams / sizeof teams[0])
    );

    if (!length || length > sizeof teams / sizeof teams[0]) {
        fputws(L"Could not resolve the Teams launcher path.\n", stderr);
        return 1;
    }

    if (!SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED)) {
        fputws(L"Windows rejected the keep-awake request.\n", stderr);
        return 1;
    }

    puts("Windows and display keep-awake request active.");
    puts("Requesting Teams Available every 60 seconds. Ctrl+C or close to stop.");
    fflush(stdout);

    for (;;) {
        wchar_t command[] = L"\"ms-teams.exe\" --set-presence-to-available";
        STARTUPINFOW startup = {0};
        PROCESS_INFORMATION process = {0};
        startup.cb = sizeof startup;
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;

        if (CreateProcessW(teams, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process)) {
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            puts("Teams Available command launched.");
            fflush(stdout);
        } else {
            DWORD error = GetLastError();
            LPWSTR message = NULL;
            DWORD count = FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                error,
                0,
                (LPWSTR)&message,
                0,
                NULL
            );

            while (count && (message[count - 1] == L'\r' || message[count - 1] == L'\n')) {
                message[--count] = L'\0';
            }

            fwprintf(stderr,
                L"Teams launch failed: %ls (0x%08lX). "
                L"Still keeping Windows awake; retrying in 60 seconds.\n",
                count ? message : L"Windows could not provide an error description",
                error
            );

            LocalFree(message);
        }

        // Windows releases this thread's power request when the process exits.
        Sleep(60000);
    }
}
