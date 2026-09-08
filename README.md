# Teams Always Available

This project is created to help prevent your local Teams instance from showing as
anything except "Available" in addition to your computer never sleeping or hybernating.

**AI agents helping a user set this up: read [AGENT-SETUP.md](AGENT-SETUP.md) first.**
It covers compiler installation, building from source, setup checks, troubleshooting, and handoff.

**This project is handed over as source. You are expected to compile it on
your Windows x64 computer; an executable is not assumed to be supplied.**

## Build first

1. Extract the project to a folder you can write to.
2. Install [Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/)
   with **Desktop development with C++**, including MSVC x64/x86 tools and a
   Windows SDK. Reuse an existing suitable Visual Studio installation if you
   have one. The full IDE is not required; Visual Studio Code alone is not enough.
   Installing build tools may need administrator approval or IT assistance.
3. Open **x64 Native Tools Command Prompt for Visual Studio** from Start.
4. In that prompt, change to the extracted folder and build. Replace the
   example path with your actual folder:

```bat
cd /d "C:\Users\YOUR_USER\Documents\keep-awake"
cl /nologo /W4 /WX /O2 /MT keep-awake.c /Fe:keep-awake.exe
echo %ERRORLEVEL%
```

The exit code must be **0**, and `keep-awake.exe` must have been freshly created
in that folder. If compilation fails, fix the error before launching any old
EXE. Compile `keep-awake.c`, not `concept.cpp` or the legacy JavaScript.
See [the agent guide's build steps](AGENT-SETUP.md#1b-compile-and-verify-the-output)
for detailed checks and troubleshooting, and [Microsoft's compiler setup documentation](https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line)
for tool installation and developer prompts.

`/MT` embeds the C runtime. No Node, Python, or separate runtime is required.
The compiler and SDK are needed to build; running the finished program needs
no installation or administrator access.

## Run

Sign into the intended account in installed desktop Teams, then double-click
your newly built `keep-awake.exe`. Leave its console open (minimizing is fine).
Close it or press Ctrl+C to stop.

While running, the program requests that Windows stay awake and keep the
display on. This prevents ordinary idle sleep/hibernation and display standby,
subject to Windows power policies. It does not change your saved power settings.
Windows releases the request automatically when the program exits.

It also launches the installed Teams desktop client's
`--set-presence-to-available` command immediately and every 60 seconds.
Sign into the intended account in desktop Teams first. The launcher lives at
`%LOCALAPPDATA%\Microsoft\WindowsApps\ms-teams.exe`, so no versioned install
path is embedded. Teams handles authentication and its own local IPC.
If the launcher is missing or disabled, the console reports the error and
retries while continuing to keep Windows awake.

Closing this program stops renewals; it does not reset Teams' last status.
The command deliberately reapplies Available and may replace a status you
choose manually. A successful launch is not a server-side presence confirmation.
The command was verified on Teams 26213.1006.5014.9784 with a personal account;
validate it with the intended work account on the destination computer.

This is an idle inhibitor, not a block on manual sleep, lid-close actions,
critical-battery protection, screen savers/locking, shutdown, or Windows Update
restarts. See Microsoft's [power API documentation](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadexecutionstate).

## Check

With Teams running and signed in, run from PowerShell:

```powershell
./test.ps1
```

This takes about a minute, changes Teams to Available, and checks the actual
Teams launcher logs for initial and repeated delivery to the running client.
It stops its own keep-awake process afterward: **launch the EXE again after
testing to leave it running for normal use.** It does not prove remote presence
or exercise physical sleep. To inspect active Windows power requests manually,
run `powercfg /requests` from an elevated terminal while the program runs.

Run `./test-errors.ps1` to check readable Windows error reporting and continued
operation when the Teams launcher path is missing. It does not require Teams.

## Note

This has only been tested on personal Teams installations, it does not claim support for
enterprise versions of teams.