param([string]$Executable = "$PSScriptRoot/build/app.exe")
$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path $Executable).Path
if (Get-Process -Name app -ErrorAction SilentlyContinue | Where-Object Path -eq $Executable) {
    throw 'Close the existing controller before testing.'
}
# Missing DLL must be a visible startup error, not a false success.
$folder = Join-Path "$PSScriptRoot/build" ([guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $folder | Out-Null
$copy = Join-Path $folder 'app.exe'
Copy-Item $Executable $copy
$info = [System.Diagnostics.ProcessStartInfo]::new($copy)
$info.UseShellExecute = $false
$info.CreateNoWindow = $true
$info.RedirectStandardError = $true
$process = [System.Diagnostics.Process]::Start($info)
try {
    if (!$process.WaitForExit(5000)) { throw 'Missing DLL did not cause prompt startup failure.' }
    $message = $process.StandardError.ReadToEnd()
    if ($process.ExitCode -ne 1 -or $message -notmatch 'hook2.dll') { throw "Unexpected missing-DLL result: $message" }
    Write-Output 'PASS: missing DLL fails startup clearly.'
} finally {
    if (!$process.HasExited) { $process.Kill(); $process.WaitForExit() }
    $process.Dispose()
    Remove-Item -LiteralPath $copy
    Remove-Item -LiteralPath $folder
}
# Force a real launch failure without changing the user's Teams installation.
$info.FileName = $Executable
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
    Write-Output 'PASS: launch failure is reported while controller remains running.'
} finally {
    if (!$process.HasExited) { $process.Kill(); $process.WaitForExit() }
    $process.Dispose()
}
