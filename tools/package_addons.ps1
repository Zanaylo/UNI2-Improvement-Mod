[CmdletBinding()]
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$addonsSrc = Join-Path $root "addons"
$resourceSrc = Join-Path $root "resource"
$export = Join-Path $root "export"

function Get-ModVersion {
    $info = Join-Path $root "src\Core\info.h"
    $line = Select-String -Path $info -Pattern '#define\s+UNI2_IM_VERSION\s+"([^"]+)"'
    if (-not $line) { throw "UNI2_IM_VERSION not found in $info" }
    return $line.Matches[0].Groups[1].Value
}

function Copy-FolderFiles {
    param(
        [string]$SourceDir,
        [string]$DestDir,
        [string[]]$ExcludeNames = @()
    )
    if (-not (Test-Path $SourceDir)) { throw "Missing asset source folder: $SourceDir" }
    New-Item -ItemType Directory -Force $DestDir | Out-Null
    Get-ChildItem -Path $SourceDir -Recurse -File | ForEach-Object {
        if ($ExcludeNames -contains $_.Name.ToLower()) { return }
        $rel = $_.FullName.Substring($SourceDir.Length + 1)
        $target = Join-Path $DestDir $rel
        New-Item -ItemType Directory -Force (Split-Path $target -Parent) | Out-Null
        Copy-Item -Path $_.FullName -Destination $target -Force
    }
}

$version = Get-ModVersion

$assetsStage = Join-Path $export "UNI2-IM-Assets-$version"
$assetsZip = "$assetsStage.zip"
$manifestPath = Join-Path $export "addons-manifest.json"

if (Test-Path $assetsStage) { Remove-Item $assetsStage -Recurse -Force }
if (Test-Path $assetsZip) { Remove-Item $assetsZip -Force }
New-Item -ItemType Directory -Force $assetsStage | Out-Null

Copy-FolderFiles -SourceDir (Join-Path $resourceSrc "basepals") -DestDir (Join-Path $assetsStage "BasePals")
Copy-FolderFiles -SourceDir (Join-Path $resourceSrc "patches") -DestDir (Join-Path $assetsStage "Patches")
Copy-FolderFiles -SourceDir (Join-Path $resourceSrc "stageicons") -DestDir (Join-Path $assetsStage "StageIcons")
Copy-FolderFiles -SourceDir (Join-Path $resourceSrc "shaders\examples") -DestDir (Join-Path $assetsStage "Shaders") -ExcludeNames @("examples.zip")

Compress-Archive -Path "$assetsStage\*" -DestinationPath $assetsZip -Force
$assetsHash = (Get-FileHash -Algorithm SHA256 -Path $assetsZip).Hash.ToLower()
$assetsSize = (Get-Item $assetsZip).Length

$addonEntries = @()
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
            url         = $ext.repo
        }
    }
}

$manifestObj = [ordered]@{
    schemaVersion = 2
    modVersion    = $version
    assetsName    = Split-Path $assetsZip -Leaf
    assetsBytes   = $assetsSize
    assetsSha256  = $assetsHash
    addonCount    = $addonEntries.Count
    addons        = $addonEntries
}
$manifestJson = $manifestObj | ConvertTo-Json -Depth 5
[IO.File]::WriteAllText($manifestPath, $manifestJson, (New-Object System.Text.UTF8Encoding $false))

Write-Host ""
Write-Host ("Packaged IM Assets ({0}):" -f $version)
foreach ($sub in @("BasePals", "Patches", "StageIcons", "Shaders")) {
    $files = Get-ChildItem (Join-Path $assetsStage $sub) -Recurse -File
    $bytes = ($files | Measure-Object -Property Length -Sum).Sum
    Write-Host ("  {0,-14} {1,4} file(s)  {2,12:N0} bytes" -f $sub, $files.Count, $bytes)
}
Write-Host ""
Write-Host ("Catalog add-ons ({0}):" -f $addonEntries.Count)
foreach ($a in $addonEntries) {
    Write-Host ("  {0,-18} [github]  {1}" -f $a.folder, $a.repo)
}
Write-Host ""
Write-Host ("  -> {0} ({1:N0} bytes)" -f $assetsZip, $assetsSize)
Write-Host ("  -> {0}" -f $manifestPath)
Write-Host ("  sha256 {0}" -f $assetsHash)
