# Instructions for the agent: set up and run Keep Awake

This document is addressed to **you, the AI agent**, not to the user.
Follow the steps in order. Do not assume a step succeeded because a command
returned without an error. Check the stated result before proceeding.

## Your task and completion criteria

Help the user **compile `keep-awake.c` on their intended Windows computer,
then run the resulting `keep-awake.exe`**. Assume no executable is supplied.
Its absence is expected, not a missing-delivery problem. Do not ask the sender
for a binary, download a replacement binary, or substitute your own computer's
build. This is a build-and-setup task; do not rewrite the program.

At handoff, you must know:

1. Which computer and source folder were used, and that compilation there
   succeeded and produced the executable the user is running.
2. Whether the intended account is signed into **installed desktop Teams**.
3. Whether the program is still running and reporting successful Teams launches.
4. Whether Available was observed in that account's Teams profile.
5. Which power/presence checks passed and which remain unverified.

If you cannot control or inspect the destination computer, give the user one
step at a time and ask for its result. Do not run commands on your own computer
and report them as verification of the user's different computer.

## Understand what this program does

- It requests that Windows keep the system and display awake while it runs.
  This covers ordinary idle sleep/hibernation and display standby, subject to
  Windows policies. It does not modify saved power settings.
- It asks installed Teams to set Available immediately and every 60 seconds,
  using `ms-teams.exe --set-presence-to-available`.
- Teams handles its own sign-in and sends the presence update. The program
  does not use Graph credentials, a browser, an extension, or a userscript.
- It continues keeping Windows awake if launching Teams fails, and retries.
- Closing it releases its power request and stops Teams renewals. It does
  **not** reset Teams' last status. Renewals may replace manually chosen statuses.

Do not promise prevention of manual sleep/hibernate, lid-close actions,
critical-battery actions, locking/screensavers, shutdown, or update restarts.
Do not promise permanent server-side Available status from launcher success.

## 1. Locate the destination and files

Use the user's intended Windows x64 computer and normal signed-in Windows
account. The instructions below build for x64. Other architectures are not verified
by this guide; do not assume compatibility or rebuild for them silently.

Locate these files in the supplied project or transfer package:

| File | Purpose |
| --- | --- |
| `keep-awake.c` | Required source. Compile this file on the destination computer. |
| `test.ps1` | Optional automated check of delivery to desktop Teams. |
| `README.md` and `AGENT-SETUP.md` | Usage and these agent instructions. |
| `keep-awake.exe` | Build output; do not expect it to exist yet. |

Use the actual destination path. `D:\keep-awake` was the development folder;
it is **not** a required path on the user's computer.

If the project arrived as a ZIP, extract it to an ordinary user-writable folder.
Do not try to build inside the ZIP viewer, a protected program folder, or a
temporary attachment preview. Confirm `keep-awake.c` exists in the extracted
folder. If the source is missing, obtain the source project before proceeding.

### 1A. Get the compiler ready

1. Check whether Visual Studio or **Build Tools for Visual Studio** is already
   installed with C++ desktop tools. Reuse a suitable existing installation.
2. If it is missing, help the user obtain **Build Tools for Visual Studio** from
   [Microsoft's official download page](https://visualstudio.microsoft.com/downloads/).
   The full Visual Studio IDE is not required. Visual Studio Code alone does
   not include this compiler.
3. In Visual Studio Installer, select **Desktop development with C++**. Keep
   the MSVC x64/x86 build tools and a supported Windows SDK selected. Do not
   install unrelated workloads. An existing installation can be modified to
   add this workload instead of installing another copy.
4. Let installation finish. The installer may require administrator approval
   or IT assistance; obtain that through the user's permitted process. If it
   cannot be installed, report compilation as blocked. Do not claim setup is
   complete. Running the finished program does not require administrator rights.
5. Open **x64 Native Tools Command Prompt for Visual Studio** from Start. Its
   displayed name may include the installed Visual Studio version. Use x64,
   not x86. This is a configured **Command Prompt**, not ordinary PowerShell.
6. In that prompt, run:

```bat
where cl
cl
```

Expected: `where cl` locates MSVC and `cl` prints a Microsoft compiler banner
for **x64**. With no source argument, `cl` also reports that a source filename
is missing; that is expected for this availability check, not a failed build.
If `cl` is not found, reopen the correct developer prompt or repair the workload.
Do not copy `cl.exe` into the project or manually guess INCLUDE/LIB paths.

For an agent using separate shell tool calls: compiler environment setup does
not carry over automatically into a new shell. Keep setup and compilation in
the same configured command session. If you use `vcvars64.bat` instead of the
Start shortcut, discover it in the actual installed Visual Studio directory
and `call` it in the same `cmd.exe` process that runs `cl`. Do not reuse the
development machine's hardcoded Visual Studio path.

Reference: [Microsoft's command-line build setup](https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line).

### 1B. Compile and verify the output

In the **same x64 Native Tools Command Prompt**:

1. Change to the extracted source folder. Replace the entire example path
   below with the real path; preserve the quotes if it contains spaces:

```bat
cd /d "C:\Users\YOUR_USER\Documents\keep-awake"
dir keep-awake.c
```

2. Confirm the listing contains `keep-awake.c`. Compile **that file**, not
   `concept.cpp`, and do not run anything in `legacy`.
3. If an older `keep-awake.exe` exists, establish that it belongs to this
   project before replacing it. Have the user close a running copy before
   rebuilding it. Do not terminate unrelated processes.
4. Run this exact build command:

```bat
cl /nologo /W4 /WX /O2 /MT keep-awake.c /Fe:keep-awake.exe
```

5. Immediately check the build exit code, before running another command:

```bat
echo %ERRORLEVEL%
```

Expected: **0**. Any other value means the build failed. Read and resolve the
compiler/linker error; do not launch a stale EXE that survived a failed build.
Do not remove `/WX` to hide a warning.

6. Only after successful compilation, inspect the output:

```bat
dir /T:W keep-awake.exe
dumpbin /headers keep-awake.exe | findstr /i "machine"
dumpbin /dependents keep-awake.exe
```

Expected: a nonempty EXE with a fresh build timestamp, machine type **8664
(x64)**, and Windows system DLL dependencies. The development build depended
only on `KERNEL32.dll`; another compiler version can differ. `/MT` embeds the
C runtime, so the result should not require a separately installed MSVC runtime.

7. Record the successful build and full EXE path. Compilation alone is not
   completion: continue through Teams setup, testing, and the final launch.

Node, Python, npm, and CMake are not needed. The compiler and SDK are build-time
requirements; the finished EXE needs no installer or separate runtime.

If Windows or organizational policy blocks the executable, report the exact
message and use the organization's approved process. Do not disable security
controls or instruct the user to ignore an unknown security warning.

## 2. Prepare installed Teams

1. Open the installed Microsoft Teams desktop application.
2. Have the user sign in themselves if needed. Do not request their password,
   MFA code, token, or session files.
3. Open the Teams profile menu and confirm the intended account is active.
   If several accounts are present, ask which should be affected. Do not assume
   a personal account is the user's work account.
4. Check the desktop launcher's existence in PowerShell:

```powershell
Test-Path "$env:LOCALAPPDATA\Microsoft\WindowsApps\ms-teams.exe"
```

Expected result: `True`.

If `False`, confirm that current desktop Teams is installed for this Windows
user and check Windows Settings' **App execution aliases** for Teams. Enable
the relevant alias if available and authorized by the setup request, then
repeat the check. Do not hardcode a versioned `Program Files\WindowsApps` path.
If Teams is absent or restricted, resolve that with the user/IT before claiming
Teams setup is complete. Do not switch to Teams in a website.

## 3. Avoid duplicate instances

Check before starting another copy:

```powershell
Get-Process -Name keep-awake -ErrorAction SilentlyContinue |
    Select-Object Id, Path, StartTime
```

No output means no process with that name was found. If a matching copy is
already running from the intended folder, inspect and use it. Do not blindly
kill every process with that name or start a second copy.

## 4. Optional automated check: run before the final launch

Skip this step if `test.ps1` was not supplied; use the manual checks below.
Do not disturb an existing user-owned running instance just to run the test.
Running multiple copies can make the test's launcher-log count ambiguous.

Open PowerShell in the folder containing the executable and script, then run:

```powershell
./test.ps1
```

Allow up to 75 seconds. The expected success message is:

```text
PASS: Teams received the initial command and a periodic renewal; program remained running.
```

**The test stops the keep-awake process it creates. A passing test does not
leave the user set up and running. You must do step 5 afterward.**

This test reads launcher logs. It does not confirm a remote user's view of the
status or prove that the physical computer will stay awake. If script execution
is blocked, use manual verification; do not change the machine's execution policy
just for this optional check. If it fails, read the error and use step 7.

## 5. Start the program for the user

Double-click `keep-awake.exe` in the chosen folder. Alternatively, open
PowerShell in that folder and run:

```powershell
.\keep-awake.exe
```

Keep that console open. Do not launch it as an invisible background task for
normal use. The user needs to see its errors and have an obvious way to stop it.
Minimizing the console is fine. Do not add a scheduled task, startup entry, or
Windows service unless the user separately requests automatic startup.

Check for these messages:

```text
Windows and display keep-awake request active.
Requesting Teams Available every 60 seconds. Ctrl+C or close to stop.
Teams Available command launched.
```

The first message means the Windows power API accepted the request. The last
means Windows launched Teams' command; it is not proof of a server response.
Wait a little over 60 seconds and confirm another Teams launch message appears.

## 6. Verify the actual outcome

1. In installed Teams, open the intended account's profile menu. Confirm that
   its status reads **Available**. Reopen the menu if necessary to refresh it.
   Do not send a chat message or start a meeting as a test.
2. Confirm `keep-awake.exe` remains running. Leave the final instance running
   at handoff unless the user asks you to stop it.
3. Explain that a true idle check requires leaving the computer untouched
   past its usual idle timeout. With the user's agreement, perform that check
   without changing the power plan. Keep the lid open and use stable power.
   Mouse movement, keyboard input, and UI automation invalidate an idle check.
4. If that observation is impractical now, explicitly report physical idle
   behavior as unverified. Do not claim a timed idle test passed merely because
   the process stayed alive during interactive setup.

Optional diagnostic, only when an elevated terminal is available:

```powershell
powercfg /requests
```

Look for this program's request under both DISPLAY and SYSTEM. An elevation
error means this diagnostic was unavailable; it does not mean the program needs
administrator rights. Do not change request overrides or power policies.

The development machine verified the desktop command with Teams version
26213.1006.5014.9784 and a personal account. Treat the destination work account
as a fresh verification, not as already proven by that earlier result.

## 7. Troubleshoot the observed failure

| Observation | Your next action |
| --- | --- |
| `cl` is not recognized | Use the x64 Native Tools Command Prompt and confirm the C++ workload is installed. See step 1A. |
| `windows.h` or a Windows library cannot be found | Check the Windows SDK/C++ workload and reopen the configured developer prompt. Do not download individual headers or DLLs. |
| Compiler cannot find `keep-awake.c` | Check the current directory and extraction location. Compile the supplied source, not `concept.cpp`. |
| Linker cannot open `keep-awake.exe` for writing | Check whether this project's old EXE is running and ask the user to close it; also check folder write access. Then rebuild. |
| Build exit code is nonzero but an EXE exists | Treat the EXE as potentially stale. Resolve the build error and obtain a successful build before running it. |
| Console closes immediately | Run the EXE from an already-open PowerShell window so the error remains visible. Record the exact message. |
| `Windows rejected the keep-awake request.` | The program exits; do not report power protection as active. Capture the error and investigate the destination environment. |
| `Could not resolve the Teams launcher path.` | Check that this is the intended normal Windows user session and `LOCALAPPDATA` is set correctly. Do not substitute another user's path. |
| `Teams launch failed: ... (0x...)` | Read the Windows error description, preserve it and the hexadecimal code, repeat step 2's alias check, and verify access to the installed client. The program may still be keeping Windows awake. Do not report Teams as working. |
| Launch messages appear but Teams stays Away/Offline | Verify sign-in, intended account, connectivity, and that you are inspecting installed Teams. Try the direct command below once. If it still fails, report a client/account compatibility problem; do not add browser automation or extract tokens. |
| Test says to start desktop Teams | Start and sign into Teams. If its log layout differs from the script's expected location, use manual checks and report the automated check as unavailable. |
| Test observes fewer than two dispatches | Verify the launcher and console output. Inspect relevant recent launcher errors locally; do not increase the timeout indefinitely or count launch messages as confirmed delivery. |
| Computer still sleeps while program runs | Identify whether this was ordinary idle sleep or a lid/manual/battery/policy action. Check power requests if possible and report the actual limitation. Do not disable organizational policies. |

For the single direct-command check, use PowerShell:

```powershell
Start-Process -FilePath "$env:LOCALAPPDATA\Microsoft\WindowsApps\ms-teams.exe" `
    -ArgumentList '--set-presence-to-available' -WindowStyle Hidden
```

Then inspect the intended account's profile. Do not toggle Away or other
statuses merely to demonstrate a transition without the user's agreement.
Do not upload full Teams logs or copy private chats into your report. Record
only the relevant error and minimal diagnostic evidence.

## 8. Hand off clearly

Tell the user, in plain language:

- That compilation succeeded on their computer, the exact executable location,
  and whether it is currently running.
- Whether you observed Available on the intended Teams account and renewal.
- What power verification was performed and any unverified behavior.
- "Leave this console open or minimized. Close it or press Ctrl+C to stop."
- "Stopping ends renewals; use Teams' Reset status control if you want to
  clear its last manually requested status."

If something remains blocked, name it specifically and distinguish working
Windows keep-awake behavior from unverified or failing Teams behavior. Never
report full success when only the executable launched or only the test passed.
