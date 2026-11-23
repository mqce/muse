# Build the Muse Gadget SDK for Waveshare ESP32-S3-Touch-AMOLED-1.75 (profile s3n).
# Requires ESP-IDF v6.0.3 (default: C:\esp\esp-idf-v6.0.3).
#
# Usage:
#   .\tools\build_muse_sdk.ps1
#   .\tools\build_muse_sdk.ps1 -IdfPath D:\esp\esp-idf-v6.0.3
#   .\tools\build_muse_sdk.ps1 -Target app
#   .\tools\build_muse_sdk.ps1 -Target app-flash -Port COM5
#   .\tools\build_muse_sdk.ps1 -NoCcache

param(
    [string]$IdfPath = "C:\esp\esp-idf-v6.0.3",
    [ValidateSet("build", "app", "app-flash")]
    [string]$Target = "build",
    [string]$Port = "",
    [switch]$NoCcache
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$SdkRoot = Join-Path $RepoRoot "third_party\muse-gadget-sdk"
$Esp32Dir = Join-Path $SdkRoot "esp32"
$ExportBat = Join-Path $IdfPath "export.bat"
$BuildDir = "build-muse-waveshare-s3-175"
$VoicevoxDefaults = Join-Path $RepoRoot "sdk\voicevox\sdkconfig.defaults"
$RealtimeDefaults = Join-Path $RepoRoot "sdk\realtime\sdkconfig.defaults"
$VoicevoxDefaultsPosix = $VoicevoxDefaults -replace '\\', '/'
$RealtimeDefaultsPosix = $RealtimeDefaults -replace '\\', '/'
$Defaults = "sdkconfig.defaults;devices/sdkconfig.muse;devices/sdkconfig.muse-waveshare-s3-175;$VoicevoxDefaultsPosix;$RealtimeDefaultsPosix"
$Patch = Join-Path $RepoRoot "sdk\patches\muse-face.patch"
$FaceDir = ($RepoRoot + "\sdk") -replace '\\', '/'

if (-not (Test-Path $ExportBat)) {
    throw "ESP-IDF not found at $IdfPath. Install v6.0.3 or pass -IdfPath."
}
if (-not (Test-Path $Esp32Dir)) {
    throw "SDK missing at $Esp32Dir. Run: git submodule update --init --recursive"
}

if ($Target -eq "app-flash" -and [string]::IsNullOrWhiteSpace($Port)) {
    throw "app-flash requires -Port COMx"
}

Push-Location $SdkRoot
$prevEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"
git apply --reverse --check --whitespace=nowarn $Patch 2>$null | Out-Null
$patchAlready = ($LASTEXITCODE -eq 0)
if ($patchAlready) {
    Write-Host "SDK face patch already applied"
} else {
    git apply --check --whitespace=nowarn $Patch 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) {
        $ErrorActionPreference = $prevEap
        Pop-Location
        throw "SDK face patch does not apply. Submodule may have moved."
    }
    git apply --whitespace=nowarn $Patch
    if ($LASTEXITCODE -ne 0) {
        $ErrorActionPreference = $prevEap
        Pop-Location
        throw "Failed to apply SDK face patch"
    }
    Write-Host "Applied SDK face patch"
}
$ErrorActionPreference = $prevEap
Pop-Location

python (Join-Path $RepoRoot "tools\convert_faces.py")
if ($LASTEXITCODE -ne 0) {
    throw "convert_faces.py failed"
}

$Sdkconfig = Join-Path $Esp32Dir "$BuildDir\sdkconfig"
if (Test-Path $Sdkconfig) {
    if (-not (Test-Path $VoicevoxDefaults)) {
        throw "VOICEVOX settings missing at $VoicevoxDefaults"
    }
    if (-not (Test-Path $RealtimeDefaults)) {
        throw "Realtime voice settings missing at $RealtimeDefaults"
    }
    $settings = @()
    $settings += Get-Content -LiteralPath $VoicevoxDefaults | Where-Object { $_ -match '^\s*CONFIG_MUSE_VOICEVOX_' }
    $settings += Get-Content -LiteralPath $RealtimeDefaults | Where-Object { $_ -match '^\s*(?:# )?CONFIG_MUSE_REALTIME_' }
    if ($settings.Count -eq 0) {
        throw "No CONFIG_MUSE_VOICEVOX_* / CONFIG_MUSE_REALTIME_* lines in defaults"
    }
    $text = [System.IO.File]::ReadAllText($Sdkconfig)
    foreach ($line in $settings) {
        $trimmed = $line.Trim()
        if ($trimmed -match '^#\s*(CONFIG_[A-Z0-9_]+)\s+is not set$') {
            $key = $Matches[1]
        } else {
            $key = ($trimmed -split '=', 2)[0]
        }
        $pattern = "(?m)^(?:# )?$([regex]::Escape($key))(?:\s.*|=.*)$"
        if ([regex]::IsMatch($text, $pattern)) {
            $text = [regex]::Replace($text, $pattern, $trimmed)
        } else {
            $text = $text.TrimEnd() + "`n" + $trimmed + "`n"
        }
    }
    $utf8 = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($Sdkconfig, $text.Replace("`r`n", "`n"), $utf8)
    Write-Host "Applied VOICEVOX settings from $VoicevoxDefaults"
    Write-Host "Applied realtime voice settings from $RealtimeDefaults"
}

$ccacheArgs = ""
if (-not $NoCcache) {
    $ccacheCmd = Get-Command ccache -ErrorAction SilentlyContinue
    if ($ccacheCmd) {
        $ccacheArgs = "--ccache"
        Write-Host "ccache enabled"
    } else {
        Write-Host "ccache not found; building without cache (install ccache for faster rebuilds)"
    }
}

$portArgs = ""
if ($Target -eq "app-flash") {
    $portArgs = "-p $Port"
}

$idfTarget = if ($Target -eq "build") { "build" } else { $Target }

$cmd = @"
call "$ExportBat" && cd /d "$Esp32Dir" && idf.py $ccacheArgs -B $BuildDir -DIDF_TARGET=esp32s3 -DSDKCONFIG=$BuildDir/sdkconfig "-DSDKCONFIG_DEFAULTS=$Defaults" "-DEXTRA_COMPONENT_DIRS=$FaceDir" $portArgs $idfTarget
"@

Write-Host "Building Muse Gadget SDK (waveshare-s3-175) target=$idfTarget ..."
cmd /c $cmd
if ($LASTEXITCODE -ne 0) {
    throw "idf.py $idfTarget failed with exit code $LASTEXITCODE"
}

$bin = Join-Path $Esp32Dir "$BuildDir\muse-gadget.bin"
Write-Host "OK: $bin"
Get-Item $bin | Format-List FullName, Length, LastWriteTime
