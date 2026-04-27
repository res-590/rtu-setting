param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$PublishRoot = "",
    [string]$QMakePath = "qmake",
    [string]$MakeTool = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Resolve-MakeTool {
    param([string]$QMakeExe, [string]$Preferred)

    if ($Preferred -and (Test-Path $Preferred)) {
        return $Preferred
    }

    $candidates = @(
        "mingw32-make",
        "jom",
        "nmake"
    )

    foreach ($candidate in $candidates) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($command) {
            return $command.Source
        }
    }

    try {
        $qtBins = & $QMakeExe -query QT_HOST_BINS
        if ($LASTEXITCODE -eq 0 -and $qtBins) {
            $qtRoot = Split-Path -Parent $qtBins
            $possible = Get-ChildItem -Path $qtRoot -Filter mingw32-make.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($possible) {
                return $possible.FullName
            }
        }
    } catch {
    }

    throw "未找到 make 工具，请通过 -MakeTool 显式指定。"
}

function Ensure-Dir([string]$Path) {
    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Copy-IfExists([string]$Source, [string]$TargetDir) {
    if (Test-Path $Source) {
        Copy-Item -Path $Source -Destination $TargetDir -Force
    }
}

Set-Location $ProjectRoot

$mainPro = Join-Path $ProjectRoot "RTU_Parameter_Settings.pro"
$updaterPro = Join-Path $ProjectRoot "updater\RTU_Updater.pro"
$releaseDir = Join-Path $ProjectRoot "release"
$updaterBuildDir = Join-Path $ProjectRoot "updater\release"
$publishRootResolved = if ($PublishRoot) { $PublishRoot } else { Join-Path $ProjectRoot "publish" }
$publishDir = Join-Path $publishRootResolved "RTU_Parameter_Config_Tool"
$configDir = Join-Path $publishDir "config"

Ensure-Dir $publishRootResolved
Ensure-Dir $publishDir
Ensure-Dir $configDir

if (-not $SkipBuild) {
    $resolvedMake = Resolve-MakeTool -QMakeExe $QMakePath -Preferred $MakeTool

    & $QMakePath $mainPro
    & $resolvedMake

    Push-Location (Join-Path $ProjectRoot "updater")
    & $QMakePath $updaterPro
    & $resolvedMake
    Pop-Location
}

$mainExe = Join-Path $releaseDir "RTU_Parameter_Config_Tool.exe"
$updaterExe = Join-Path $updaterBuildDir "updater.exe"

if (-not (Test-Path $mainExe)) {
    throw "未找到主程序：$mainExe"
}
if (-not (Test-Path $updaterExe)) {
    throw "未找到升级器：$updaterExe"
}

Get-ChildItem -Path $publishDir -Force | Remove-Item -Recurse -Force
Ensure-Dir $publishDir
Ensure-Dir $configDir

Copy-Item -Path $mainExe -Destination $publishDir -Force
Copy-Item -Path $updaterExe -Destination (Join-Path $publishDir "updater.exe") -Force
Copy-IfExists (Join-Path $ProjectRoot "dashboard_dark.qss") $publishDir
if (Test-Path (Join-Path $ProjectRoot "deploy\update_config.ini.example")) {
    Copy-Item -Path (Join-Path $ProjectRoot "deploy\update_config.ini.example") -Destination (Join-Path $configDir "update_config.ini") -Force
}
Copy-IfExists (Join-Path $ProjectRoot "deploy\README.md") $publishDir
Copy-IfExists (Join-Path $ProjectRoot "deploy\update_manifest.sample.json") $publishDir

$externalConfigRoot = Join-Path (Split-Path -Parent $ProjectRoot) "RTU_Parameter_Settings"
foreach ($name in @("baseparaminfo.ini", "instancemap.ini", "sensorinstance.ini", "sensortypeitemconfig.ini")) {
    Copy-IfExists (Join-Path $externalConfigRoot $name) $publishDir
}

$version = Select-String -Path (Join-Path $ProjectRoot "app_metadata.h") -Pattern 'APP_VERSION_LITERAL QStringLiteral\("([^"]+)"\)' | ForEach-Object { $_.Matches[0].Groups[1].Value } | Select-Object -First 1
if (-not $version) {
    $version = "1.0.0"
}
Set-Content -Path (Join-Path $publishDir "version.txt") -Value $version -Encoding UTF8

$windeployqt = Join-Path ((& $QMakePath -query QT_HOST_BINS).Trim()) "windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    throw "未找到 windeployqt：$windeployqt"
}

& $windeployqt --release --compiler-runtime --no-translations $mainExe
Copy-Item -Path (Join-Path $releaseDir "*") -Destination $publishDir -Recurse -Force

$zipPath = Join-Path $publishRootResolved ("RTU_Parameter_Config_Tool-{0}.zip" -f $version)
if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}
Compress-Archive -Path (Join-Path $publishDir "*") -DestinationPath $zipPath

Write-Host "发布目录已生成：" $publishDir
Write-Host "发布压缩包已生成：" $zipPath
