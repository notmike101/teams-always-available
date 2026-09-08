param([string]$Executable = "$PSScriptRoot/keep-awake.exe")
$ErrorActionPreference = 'Stop'
# Force a real missing-path error without touching the user's Teams installation.
$info = [System.Diagnostics.ProcessStartInfo]::new($Executable)
$info.UseShellExecute = $false
$info.CreateNoWindow = $true
$info.RedirectStandardError = $true
$info.RedirectStandardOutput = $true
$info.EnvironmentVariables['LOCALAPPDATA'] = Join-Path ([IO.Path]::GetTempPath()) ([guid]::NewGuid().ToString())
$process = [System.Diagnostics.Process]::Start($info)
try {
    $read = $process.StandardError.ReadLineAsync()
    if (!$read.Wait(5000)) { throw 'No launch error was reported within five seconds.' }
    $message = $read.Result
    $description = [System.ComponentModel.Win32Exception]::new(3).Message.Trim()
    if ($message -notlike "*$description*" -or $message -notmatch '0x00000003') {
        throw "Expected the Windows path-not-found description and hexadecimal code; received: $message"
    }
    if ($process.HasExited) { throw 'A Teams error must not stop keep-awake.' }
    Write-Output "PASS: $message"
} finally {
    if (!$process.HasExited) { $process.Kill(); $process.WaitForExit() }
    $process.Dispose()
}
