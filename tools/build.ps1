# Select muse SDK build/flash path (see .cursor/skills/muse-build/SKILL.md).
#
# Usage:
#   .\tools\build.ps1 -Mode sdk
#   .\tools\build.ps1 -Mode sdk-app
#   .\tools\build.ps1 -Mode sdk-flash -Port COM5

param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("sdk", "sdk-app", "sdk-flash")]
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
    "sdk-flash" {
        if ([string]::IsNullOrWhiteSpace($Port)) {
            throw "sdk-flash requires -Port COMx"
        }
        Write-Host "Mode=sdk-flash (Muse Gadget SDK app-flash) Port=$Port"
        & $sdkScript @sdkParams -Target app-flash -Port $Port
    }
}
