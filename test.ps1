param(
    [string]$Executable = "$PSScriptRoot/keep-awake.exe",
    [int]$TimeoutSeconds = 75
)
$ErrorActionPreference = 'Stop'
# Integration check: catches missing or broken periodic Teams dispatch.
# Requires installed, signed-in desktop Teams. Sets its status to Available.
$names = @('keep-awake', [IO.Path]::GetFileNameWithoutExtension($Executable)) | Select-Object -Unique
if (Get-Process -Name $names -ErrorAction SilentlyContinue) {
    throw 'Close existing keep-awake instances before testing.'
}
$logs = "$env:LOCALAPPDATA/Packages/MSTeams_8wekyb3d8bbwe/LocalCache/Microsoft/MSTeams/Logs"
if (!(Test-Path $logs)) { throw 'Start desktop Teams before running this check.' }
$started = Get-Date
$process = Start-Process -FilePath $Executable -WindowStyle Hidden -PassThru
try {
    do {
        if ($process.WaitForExit(500)) { throw "Program exited early: $($process.ExitCode)" }
        $dispatches = @(Get-ChildItem $logs -Filter 'Launcher_*.log' |
            Where-Object CreationTime -ge $started |
            Where-Object {
                $text = Get-Content $_.FullName -Raw
                $text -match '"cmd_line":"--set-presence-to-available"' -and
                $text -match 'Message sent to server'
            } | Sort-Object CreationTime)
        if ($dispatches.Count -ge 2 -and
            # Allow launcher startup jitter around the program's 60-second interval.
            ($dispatches[-1].CreationTime - $dispatches[0].CreationTime).TotalSeconds -ge 55) {
            Write-Output 'PASS: Teams received the initial command and a periodic renewal; program remained running.'
            return
        }
    } while ((Get-Date) -lt $started.AddSeconds($TimeoutSeconds))
    throw "Expected two Teams dispatches; observed $($dispatches.Count)."
} finally {
    if (!$process.HasExited) { Stop-Process -Id $process.Id }
    $process.Dispose()
}
