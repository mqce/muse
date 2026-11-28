# Select muse SDK build/flash path (see .cursor/skills/muse-build/SKILL.md).
#
# Usage:
#   .\tools\build.ps1 -Mode sdk
#   .\tools\build.ps1 -Mode sdk-app
#   .\tools\build.ps1 -Mode flash -Port COM5
#   .\tools\build.ps1 -Mode sdk-flash -Port COM5   # alias of flash (no rebuild)

param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("sdk", "sdk-app", "flash", "sdk-flash")]
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
    "sdk" {
        Write-Host "Mode=sdk (Muse Gadget SDK full incremental build)"
        & $sdkScript @sdkParams -Target build
    }
    "sdk-app" {
        Write-Host "Mode=sdk-app (Muse Gadget SDK app-only build)"
        & $sdkScript @sdkParams -Target app
    }
    { $_ -in @("flash", "sdk-flash") } {
        if ([string]::IsNullOrWhiteSpace($Port)) {
            throw "$Mode requires -Port COMx"
        }
        Write-Host "Mode=$Mode (flash existing app, no rebuild) Port=$Port"
        & $sdkScript @sdkParams -Target flash -Port $Port
    }
}
