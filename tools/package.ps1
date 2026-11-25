[CmdletBinding()]
param(
    [string]$Configuration = "Release",
    [string]$Toolset = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$export = Join-Path $root "export"

function Get-ModVersion {
    $info = Join-Path $root "src\Core\info.h"
    $line = Select-String -Path $info -Pattern '#define\s+UNI2_IM_VERSION\s+"([^"]+)"'
    if (-not $line) { throw "UNI2_IM_VERSION not found in $info" }
    return $line.Matches[0].Groups[1].Value
}

function Get-MSBuild {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $path = & $vswhere -latest -prerelease -products * -requires Microsoft.Component.MSBuild `
            -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
        if ($path) { return $path }
    }
    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\18\*\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\*\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\MSBuild\Current\Bin\MSBuild.exe"
    )
    foreach ($pattern in $candidates) {
        $found = Get-ChildItem $pattern -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
        if ($found) { return $found }
    }
    throw "MSBuild not found"
}

function Build-Project {
    param(
        [string]$MSBuild,
        [string]$Project,
        [string[]]$ExtraArgs = @(),
        [string]$Label = ""
    )

    $arguments = @(
        (Join-Path $root $Project),
        "/p:Configuration=$Configuration",
        "/p:Platform=Win32",
        "/p:DeployToGame=false",
        "/t:Rebuild",
        "/v:minimal",
        "/nologo"
    )
    if ($Toolset) { $arguments += "/p:PlatformToolset=$Toolset" }
    if ($ExtraArgs.Count -gt 0) { $arguments += $ExtraArgs }

    $name = if ($Label) { "$Project ($Label)" } else { $Project }
    Write-Host "Building $name"
    & $MSBuild @arguments
    if ($LASTEXITCODE -ne 0) { throw "build failed: $name" }
}

$version = Get-ModVersion
$stage = Join-Path $export "UNI2-Improvement-Mod-$version"
$zip = "$stage.zip"
$manifest = Join-Path $export "manifest.json"

if (-not $SkipBuild) {
    $msbuild = Get-MSBuild
    Build-Project -MSBuild $msbuild -Project "UNI2_IM.vcxproj" -Label "core"
    Build-Project -MSBuild $msbuild -Project "UNI2ModLoader.vcxproj" -Label "mod-loader"
    Build-Project -MSBuild $msbuild -Project "UNI2IMUpdater.vcxproj" -Label "updater"
}

$dll = Join-Path $root "bin\$Configuration\dinput8.dll"
$loader = Join-Path $root "bin\$Configuration\UNI2ModLoader.exe"
$updater = Join-Path $root "bin\$Configuration\UNI2IMUpdater.exe"

foreach ($file in @($dll, $loader, $updater)) {
    if (-not (Test-Path $file)) { throw "missing $file" }
}

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null
if (Test-Path $zip) { Remove-Item $zip -Force }

Copy-Item $dll $stage
Copy-Item $loader $stage
Copy-Item $updater $stage

Get-ChildItem $stage -Filter "steam_appid.txt" -Recurse | Remove-Item -Force
Compress-Archive -Path "$stage\*" -DestinationPath $zip -Force

$hash = (Get-FileHash -Algorithm SHA256 -Path $zip).Hash.ToLower()

$manifestJson = [ordered]@{
    schemaVersion = 1
    version       = $version
    tag           = "v$version"
    channel       = "stable"
    os            = "win"
    arch          = "x86"
    assetName     = Split-Path $zip -Leaf
    sha256        = $hash
    entryDll      = "dinput8.dll"
    modLoaderExe  = "UNI2ModLoader.exe"
    updaterExe    = "UNI2IMUpdater.exe"
} | ConvertTo-Json -Depth 4
[IO.File]::WriteAllText($manifest, $manifestJson, (New-Object System.Text.UTF8Encoding $false))

Write-Host ""
Write-Host "packaged $version"
Get-ChildItem $stage | ForEach-Object { "  {0,-24} {1,10:N0} bytes" -f $_.Name, $_.Length }
"  -> {0}" -f $zip
"  -> {0}" -f $manifest
"  sha256 {0}" -f $hash
