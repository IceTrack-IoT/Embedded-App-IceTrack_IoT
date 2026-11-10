#requires -Version 5.1
<#
.SYNOPSIS
    Compiles (or uploads) the IceTrack ESP32 firmware with arduino-cli.

.DESCRIPTION
    Arduino requires the main .ino file to have the same name as its containing
    folder. This repository is called "ice-track-embedded-app" but keeps
    "sketch.ino" so the same sources open directly in Wokwi. The script copies
    the sources into a temporary correctly-named sketch folder and builds it
    there, leaving the repository untouched.

.EXAMPLE
    .\tools\build.ps1

.EXAMPLE
    .\tools\build.ps1 -Upload -Port COM5
#>
param(
    [string]$Fqbn = "esp32:esp32:esp32",
    [string]$ArduinoCli = "",
    [string]$Port = "",
    [switch]$Upload
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if (-not $ArduinoCli) {
    $candidates = @(
        (Get-Command arduino-cli -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue),
        (Join-Path $env:LOCALAPPDATA "arduino-cli\arduino-cli.exe"),
        (Join-Path $env:USERPROFILE "AppData\Local\arduino-cli\arduino-cli.exe")
    ) | Where-Object { $_ -and (Test-Path $_) }

    if (-not $candidates) {
        throw "arduino-cli was not found. Install it or pass -ArduinoCli <path>."
    }
    $ArduinoCli = @($candidates)[0]
}

$stageRoot = Join-Path $env:TEMP "icetrack-build"
$stage = Join-Path $stageRoot "ice-track-embedded-app"
$out = Join-Path $stageRoot "out"

if (Test-Path $stage) {
    Remove-Item $stage -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stage | Out-Null
New-Item -ItemType Directory -Force -Path $out | Out-Null

Copy-Item -Path (Join-Path $root "*.h") -Destination $stage
Copy-Item -Path (Join-Path $root "*.cpp") -Destination $stage
Copy-Item -Path (Join-Path $root "sketch.ino") -Destination (Join-Path $stage "ice-track-embedded-app.ino")

Write-Host "Using arduino-cli: $ArduinoCli"
Write-Host "Staged sketch     : $stage"
Write-Host "Board             : $Fqbn"

if ($Upload) {
    if (-not $Port) {
        throw "Pass -Port <COMx> together with -Upload."
    }
    & $ArduinoCli compile --fqbn $Fqbn --warnings all --upload --port $Port --output-dir $out $stage
    exit $LASTEXITCODE
}

& $ArduinoCli compile --fqbn $Fqbn --warnings all --output-dir $out $stage
exit $LASTEXITCODE
