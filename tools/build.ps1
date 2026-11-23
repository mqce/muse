# Select muse build/flash path by change type (see .cursor/skills/muse-build/SKILL.md).
#
# Usage:
#   .\tools\build.ps1 -Mode face
#   .\tools\build.ps1 -Mode face-upload
#   .\tools\build.ps1 -Mode sdk
#   .\tools\build.ps1 -Mode sdk-app
#   .\tools\build.ps1 -Mode sdk-flash -Port COM5

param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("face", "face-upload", "sdk", "sdk-app", "sdk-flash")]
    [string]$Mode,

    [string]$IdfPath = "C:\esp\esp-idf-v6.0.3",
    [string]$Port = "",
    [switch]$NoCcache
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

$sdkScript = Join-Path $PSScriptRoot "build_muse_sdk.ps1"
$sdkParams = @{ IdfPath = $IdfPath }
if ($NoCcache) { $sdkParams.NoCcache = $true }

switch ($Mode) {
    "face" {
        Write-Host "Mode=face (PlatformIO build)"
        pio run
        if ($LASTEXITCODE -ne 0) { throw "pio run failed with exit code $LASTEXITCODE" }
    }
    "face-upload" {
        Write-Host "Mode=face-upload (PlatformIO upload)"
        pio run -t upload
        if ($LASTEXITCODE -ne 0) { throw "pio run -t upload failed with exit code $LASTEXITCODE" }
    }
    "sdk" {
        Write-Host "Mode=sdk (Muse Gadget SDK full incremental build)"
        & $sdkScript @sdkParams -Target build
    }
    "sdk-app" {
        Write-Host "Mode=sdk-app (Muse Gadget SDK app-only build)"
        & $sdkScript @sdkParams -Target app
    }
    "sdk-flash" {
        if ([string]::IsNullOrWhiteSpace($Port)) {
            throw "sdk-flash requires -Port COMx"
        }
        Write-Host "Mode=sdk-flash (Muse Gadget SDK app-flash) Port=$Port"
        & $sdkScript @sdkParams -Target app-flash -Port $Port
    }
}
