[CmdletBinding()]
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$addonsSrc = Join-Path $root "addons"
$export = Join-Path $root "export"
$addonsExport = Join-Path $export "addons"
$officialRepo = "https://github.com/Zanaylo/UNI2-Improvement-Mod"

function Get-ModVersion {
    $info = Join-Path $root "src\Core\info.h"
    $line = Select-String -Path $info -Pattern '#define\s+UNI2_IM_VERSION\s+"([^"]+)"'
    if (-not $line) { throw "UNI2_IM_VERSION not found in $info" }
    return $line.Matches[0].Groups[1].Value
}

function Parse-Manifest {
    param([string]$IniPath, [string]$FallbackId)
    $meta = [ordered]@{
        id          = $FallbackId
        name        = $FallbackId
        version     = ""
        author      = ""
        description = ""
        entry       = ""
        nativeDll   = ""
        modules     = ""
        assets      = ""
        url         = ""
    }
    foreach ($raw in (Get-Content -Path $IniPath -Encoding UTF8)) {
        $line = $raw.Trim()
        if (-not $line -or $line.StartsWith(";") -or $line.StartsWith("#") -or $line.StartsWith("[")) { continue }
        $eq = $line.IndexOf("=")
        if ($eq -lt 1) { continue }
        $k = $line.Substring(0, $eq).Trim().ToLower()
        $v = $line.Substring($eq + 1).Trim()
        switch ($k) {
            "id"          { $meta.id = $v }
            "name"        { $meta.name = $v }
            "version"     { $meta.version = $v }
            "author"      { $meta.author = $v }
            "description" { $meta.description = $v }
            "entry"       { $meta.entry = $v }
            "native"      { $meta.nativeDll = $v }
            "modules"     { $meta.modules = $v }
            "assets"      { $meta.assets = $v }
            "url"         { $meta.url = $v }
        }
    }
    return $meta
}

function Copy-CleanAddon {
    param([string]$SourceDir, [string]$DestDir)
    New-Item -ItemType Directory -Force $DestDir | Out-Null
    $excludeExt = @(".pdb", ".lib", ".exp", ".iobj", ".ipdb", ".obj", ".tlog", ".log")
    Get-ChildItem -Path $SourceDir -Recurse -File | ForEach-Object {
        $rel = $_.FullName.Substring($SourceDir.Length + 1)
        if ($rel -match '(^|\\)obj(\\|$)' -or $rel -match '(^|\\)\.vs(\\|$)') { return }
        if ($excludeExt -contains $_.Extension.ToLower()) { return }
        $target = Join-Path $DestDir $rel
        New-Item -ItemType Directory -Force (Split-Path $target -Parent) | Out-Null
        Copy-Item -Path $_.FullName -Destination $target -Force
    }
}

$version = Get-ModVersion

$bundleStage = Join-Path $export "UNI2-IM-Addons-Bundle-$version"
$bundleZip = "$bundleStage.zip"
$manifestPath = Join-Path $export "addons-manifest.json"

if (Test-Path $bundleStage) { Remove-Item $bundleStage -Recurse -Force }
if (Test-Path $bundleZip) { Remove-Item $bundleZip -Force }
if (Test-Path $addonsExport) { Remove-Item $addonsExport -Recurse -Force }
New-Item -ItemType Directory -Force $bundleStage | Out-Null
New-Item -ItemType Directory -Force $addonsExport | Out-Null

$addonEntries = @()

Get-ChildItem -Path $addonsSrc -Directory | Sort-Object Name | ForEach-Object {
    $ini = Join-Path $_.FullName "addon.ini"
    if (-not (Test-Path $ini)) { return }

    $folderName = $_.Name
    $meta = Parse-Manifest -IniPath $ini -FallbackId $folderName
    $destInBundle = Join-Path $bundleStage $folderName
    Copy-CleanAddon -SourceDir $_.FullName -DestDir $destInBundle

    $singleZip = Join-Path $addonsExport "$folderName-$version.zip"
    if (Test-Path $singleZip) { Remove-Item $singleZip -Force }
    Compress-Archive -Path $destInBundle -DestinationPath $singleZip -Force

    $singleHash = (Get-FileHash -Algorithm SHA256 -Path $singleZip).Hash.ToLower()
    $singleSize = (Get-Item $singleZip).Length
    $repoUrl = if ($meta.url) { $meta.url } else { $officialRepo }

    $addonEntries += [ordered]@{
        folder      = $folderName
        id          = $meta.id
        name        = $meta.name
        version     = $meta.version
        author      = $meta.author
        description = $meta.description
        source      = "release"
        repo        = $repoUrl
        iconUrl     = ""
        entry       = $meta.entry
        nativeDll   = $meta.nativeDll
        modules     = $meta.modules
        assets      = $meta.assets
        url         = $repoUrl
        zipName     = Split-Path $singleZip -Leaf
        zipBytes    = $singleSize
        sha256      = $singleHash
    }
}

$catalogPath = Join-Path $addonsSrc "catalog.json"
if (Test-Path $catalogPath) {
    $catalog = Get-Content -Path $catalogPath -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($ext in @($catalog.externalAddons)) {
        if (-not $ext -or -not $ext.id -or -not $ext.repo) { continue }
        $addonEntries += [ordered]@{
            folder      = if ($ext.folder) { $ext.folder } else { $ext.id }
            id          = $ext.id
            name        = if ($ext.name) { $ext.name } else { $ext.id }
            version     = ""
            author      = if ($ext.author) { $ext.author } else { "" }
            description = if ($ext.description) { $ext.description } else { "" }
            source      = "github"
            repo        = $ext.repo
            iconUrl     = if ($ext.iconUrl) { $ext.iconUrl } else { "" }
            entry       = ""
            nativeDll   = ""
            modules     = ""
            assets      = ""
            url         = $ext.repo
            zipName     = ""
            zipBytes    = 0
            sha256      = ""
        }
    }
}

Compress-Archive -Path "$bundleStage\*" -DestinationPath $bundleZip -Force
$bundleHash = (Get-FileHash -Algorithm SHA256 -Path $bundleZip).Hash.ToLower()
$bundleSize = (Get-Item $bundleZip).Length

$manifestObj = [ordered]@{
    schemaVersion = 2
    modVersion    = $version
    bundleName    = Split-Path $bundleZip -Leaf
    bundleBytes   = $bundleSize
    bundleSha256  = $bundleHash
    addonCount    = $addonEntries.Count
    addons        = $addonEntries
}
$manifestJson = $manifestObj | ConvertTo-Json -Depth 5
[IO.File]::WriteAllText($manifestPath, $manifestJson, (New-Object System.Text.UTF8Encoding $false))

Write-Host ""
Write-Host "Packaged $($addonEntries.Count) catalog add-on entries:"
foreach ($a in $addonEntries) {
    if ($a.source -eq "github") {
        Write-Host ("  {0,-18} [github]  {1}" -f $a.folder, $a.repo)
    } else {
        Write-Host ("  {0,-18} {1,10:N0} bytes  ({2})" -f $a.folder, $a.zipBytes, $a.id)
    }
}
Write-Host ""
Write-Host ("  -> {0} ({1:N0} bytes)" -f $bundleZip, $bundleSize)
Write-Host ("  -> {0}" -f $manifestPath)
Write-Host ("  sha256 {0}" -f $bundleHash)
