param(
    [string]$Executable = "$PSScriptRoot/build/app.exe",
    # Cold startup consumes the first cycle before launcher-to-server dispatches begin.
    [int]$TimeoutSeconds = 150
)
$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path $Executable).Path
if (Get-Process -Name app -ErrorAction SilentlyContinue | Where-Object Path -eq $Executable) {
    throw 'Close the existing controller before testing.'
}
$logs = "$env:LOCALAPPDATA/Packages/MSTeams_8wekyb3d8bbwe/LocalCache/Microsoft/MSTeams/Logs"
if (!(Test-Path $logs)) { throw 'Start and sign into desktop Teams before running this check.' }
$stdout = "$PSScriptRoot/build/integration.stdout.log"
$stderr = "$PSScriptRoot/build/integration.stderr.log"
$started = Get-Date
$process = Start-Process -FilePath $Executable -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
try {
    do {
        if ($process.WaitForExit(500)) { throw "Program exited early: $($process.ExitCode)" }
        $dispatches = @(Get-ChildItem $logs -Filter 'Launcher_*.log' |
            Where-Object CreationTime -ge $started |
            Where-Object {
                $text = Get-Content $_.FullName -Raw
                $text -match '"cmd_line":"--set-presence-to-available"' -and $text -match 'Message sent to server'
            } | Sort-Object CreationTime)
        $hooks = @(Select-String -Path $stdout -Pattern 'Idle hook confirmed: [1-9][0-9]* module')
        if ($dispatches.Count -ge 2 -and $hooks.Count -ge 2 -and
            ($dispatches[-1].CreationTime - $dispatches[0].CreationTime).TotalSeconds -ge 55) {
            Write-Output 'PASS: two Teams dispatches and two confirmed hook cycles; controller remained running.'
            return
        }
    } while ((Get-Date) -lt $started.AddSeconds($TimeoutSeconds))
    throw "Expected two dispatches and hook cycles; observed $($dispatches.Count) dispatches, $($hooks.Count) hooks. See $stdout and $stderr."
} finally {
    if (!$process.HasExited) { Stop-Process -Id $process.Id }
    $process.Dispose()
}
