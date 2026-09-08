# Teams Presence Keep-Awake

A small Windows console utility that requests **Available** in desktop Teams,
reduces automatic Away transitions caused by OS idle time, and keeps Windows
and the display awake while it runs. No mouse movement, keystrokes, Graph app
registration, credentials, service, or scheduled task is needed.

Tested with a **personal Teams account** on Windows x64. Work/school accounts,
ARM64, classic Teams, and browser Teams are not verified. The Teams launcher
flag and internal idle-detection behavior are undocumented implementation
details, so Teams updates can break this integration. It does not guarantee
server-side Available regardless of calls, connectivity, account state, or policy.

## Setup and build

1. Install the current **desktop Microsoft Teams for Windows**, open it, and
   sign into the account you intend to use. This project targets `ms-teams.exe`.
2. Install [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
   or Visual Studio. Select **Desktop development with C++**, including the
   MSVC x64/x86 compiler tools and a Windows SDK.
3. Clone or copy this repository to a local folder. In PowerShell in that folder:

   ```powershell
   .\build.bat test
   ```

   The script discovers the installed C++ toolchain with `vswhere`, or uses an
   existing x64 Developer Command Prompt. It works from any current directory
   and builds with `/W4 /WX` (warnings are errors). No third-party libraries
   are downloaded. `build.bat` without `test` builds just the application.
4. Confirm the command succeeds and the test prints `PASS`. The outputs are:

   ```text
   build\app.exe
   build\hook2.dll
   ```

   All generated output stays in the Git-ignored `build` folder. This repository
   distributes source only. The C++ runtime is statically linked; on another
   compatible Windows x64 computer, the two files above are the runtime pair.
   Prefer building and testing on the computer where you will use them.

## Run and stop

```powershell
.\build\app.exe
```

Keep `app.exe` and `hook2.dll` together. Run as the same ordinary Windows user
and in the same interactive session as Teams. No administrator privileges are
normally needed. Only one controller can run per session.

Expected console output includes:

```text
Windows/display keep-awake active. Requesting Teams Available every 60 seconds.
Teams Available command launched (not a presence confirmation).
Idle hook confirmed: 3 module(s), Teams pid 12345.
```

The module count and PID vary. A confirmed hook means at least one import table
was patched successfully; it is not a readback of your online presence.
Check your Teams profile for **Available**, then leave the computer untouched
for at least six minutes and check again. For end-to-end confirmation, check
how the account appears from another signed-in Teams account as well.

Stop with **Ctrl+C** or close the console. Windows releases the power request
when the program exits. The DLL stops suppressing idle time within **90 seconds
of its last successful refresh** and forwards calls to the real Windows API.
The DLL remains loaded in Teams until Teams exits. Your last requested Teams
status is not explicitly reset.

Before rebuilding or replacing either binary, close the controller and fully
quit Teams from its tray menu (closing the Teams window alone may leave it
running). Then rebuild and restart Teams and the controller. This also removes
any older injected hook; an old hook from another build may not have the lease.

## What it does

Every cycle, the controller:

1. Requests Available through
   `%LOCALAPPDATA%\Microsoft\WindowsApps\ms-teams.exe --set-presence-to-available`.
   The first request is immediate; subsequent cycles wait 60 seconds after
   completing their work. The launcher may open Teams if it is not running.
2. Finds a main `ms-teams.exe` in the current Windows session, excluding Teams
   helper processes whose parent is another Teams process.
3. Loads `hook2.dll` into that process once and explicitly calls its exported
   refresh function. It checks the actual remote module address and the number
   of hooked modules, rather than treating a DLL load as proof of a working hook.
4. Scans loaded modules' import address tables (IATs) for resolved
   `GetLastInputInfo` calls and redirects those slots to a small stub. While the
   90-second lease is renewed, the stub returns the current tick count as the
   last-input time. The DLL's own import remains untouched so it can forward
   to Windows after expiry. Later-loaded modules are picked up on a later cycle.

The app also makes a continuous Windows system/display power request once at
startup using
[`SetThreadExecutionState`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadexecutionstate).
It does not change your saved power settings. This request covers ordinary
idle sleep and display standby; it does not prevent manual sleep, lid-close
sleep, locking/screensavers, critical-battery actions, shutdown, or update
restarts. Windows policies can override it.

Only the selected Teams process's import tables are changed. The code in
`user32.dll`, other applications, and the real OS last-input timestamp are
unchanged. Dynamic `GetProcAddress` calls, delay imports,
and alternative idle detectors are not covered. Other Teams accounts
or processes are not independently controlled. Renewals may override statuses
you manually choose; stop the controller when you want normal presence behavior.

## Verification

```powershell
.\build.bat test
powershell -NoProfile -ExecutionPolicy Bypass -File .\test-errors.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1
```

Close any running controller before running the PowerShell checks.

- `test-hook.cpp` checks input validation, active and expired leases,
  clock wrap, IAT patching without an import name table, repeat patching, restored
  page protection, a real remote-call timeout, and injection into a disposable child process. It does not
  need Teams or touch your Teams process.
- `test-errors.ps1` checks startup failure without the DLL and verifies that a
  missing Teams launcher produces an error while keep-awake continues.
- `test.ps1` needs installed, signed-in Teams. It starts the controller, checks
  two confirmed hook cycles and two launcher dispatches, and stops the controller.
  Allow up to 150 seconds if Teams needs to start first.
  It changes Teams presence to Available. It proves local integration, not how
  another Teams user sees you or long-term idle behavior.

To inspect Windows power requests, run `powercfg /requests` in an **elevated**
terminal while the controller is running. Look for `app.exe` under DISPLAY and
SYSTEM, then verify its entries disappear after stopping it.

## Troubleshooting

| Symptom | Action |
| --- | --- |
| Compiler/toolchain not found | Install the C++ workload and Windows SDK above, then rerun `build.bat`. |
| Linker cannot write an EXE or DLL | Stop the controller, fully quit Teams, and retry. |
| Cannot load `hook2.dll` | Keep the matching DLL next to the executable; rebuild both together. |
| Teams launch failed | Open Teams manually. Check that the `ms-teams.exe` app execution alias is enabled in Windows Settings under Apps / App execution aliases. The alias is not guaranteed to be present or enabled. Windows keep-awake continues and launch is retried. |
| Teams not found | Sign in and leave desktop Teams running in the same Windows session. The app retries on the next cycle. |
| Hook failed / zero hooked modules | Check the error code, x64 Teams installation, and account/session. An update may have changed the idle detector. Windows keep-awake continues. |
| Access denied | Check whether Teams is elevated or process injection is blocked by your machine's security policy. The app does not bypass these restrictions. |
| Remote call timed out | Fully quit and restart Teams. This PID is not retried, because the remote thread may still be running. Its argument memory is retained until Teams exits to avoid freeing live memory. |
| Hooks confirmed but presence changes | Check the profile, connectivity, calls, and account policy. Hook installation cannot prove server presence or compatibility with a changed Teams client. |

No debug marker files, automatic startup entries, or persistent background
service are created. The console is the application's diagnostic output.
