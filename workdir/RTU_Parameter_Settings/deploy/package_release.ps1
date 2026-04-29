param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$PublishRoot = "",
    [string]$QMakePath = "qmake",
    [string]$MakeTool = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Resolve-MakeTool {
    param(
        [string]$QMakeExe,
        [string]$Preferred
    )

    if ($Preferred -and (Test-Path $Preferred)) {
        return $Preferred
    }

    foreach ($candidate in @("mingw32-make", "jom", "nmake")) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($command) {
            return $command.Source
        }
    }

    try {
        $qtBins = (& $QMakeExe -query QT_HOST_BINS).Trim()
        if ($LASTEXITCODE -eq 0 -and $qtBins) {
            $qtRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $qtBins))
            $toolsDir = Join-Path $qtRoot "Tools"
            if (Test-Path $toolsDir) {
                $possible = Get-ChildItem -Path $toolsDir -Filter "mingw32-make.exe" -Recurse -ErrorAction SilentlyContinue |
                    Sort-Object FullName -Descending |
                    Select-Object -First 1
                if ($possible) {
                    return $possible.FullName
                }
            }
        }
    } catch {
    }

    throw "make tool not found; use -MakeTool to specify it explicitly."
}

function Ensure-Dir {
    param([string]$Path)

    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Copy-IfExists {
    param(
        [string]$Source,
        [string]$TargetDir
    )

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
    $makeDir = Split-Path -Parent $resolvedMake
    $env:PATH = "$makeDir;$env:PATH"

    & $QMakePath $mainPro "CONFIG+=release"
    if ($LASTEXITCODE -ne 0) {
        throw "qmake failed for main project."
    }
    & $resolvedMake
    if ($LASTEXITCODE -ne 0) {
        throw "make failed for main project."
    }

    Push-Location (Join-Path $ProjectRoot "updater")
    & $QMakePath $updaterPro "CONFIG+=release"
    if ($LASTEXITCODE -ne 0) {
        Pop-Location
        throw "qmake failed for updater project."
    }
    & $resolvedMake
    if ($LASTEXITCODE -ne 0) {
        Pop-Location
        throw "make failed for updater project."
    }
    Pop-Location
}

$mainExe = Join-Path $releaseDir "RTU_Parameter_Config_Tool.exe"
$updaterExe = Join-Path $updaterBuildDir "updater.exe"

if (-not (Test-Path $mainExe)) {
    throw "main executable not found: $mainExe"
}

if (-not (Test-Path $updaterExe)) {
    throw "updater executable not found: $updaterExe"
}

if (Test-Path $publishDir) {
    Get-ChildItem -Path $publishDir -Force | Remove-Item -Recurse -Force
}

Ensure-Dir $publishDir
Ensure-Dir $configDir

Copy-Item -Path $mainExe -Destination $publishDir -Force
Copy-Item -Path $updaterExe -Destination (Join-Path $publishDir "updater.exe") -Force
Copy-IfExists -Source (Join-Path $ProjectRoot "dashboard_dark.qss") -TargetDir $publishDir

$updateConfigTemplate = Join-Path $ProjectRoot "deploy\update_config.ini.example"
if (Test-Path $updateConfigTemplate) {
    Copy-Item -Path $updateConfigTemplate -Destination (Join-Path $configDir "update_config.ini") -Force
}

Copy-IfExists -Source (Join-Path $ProjectRoot "deploy\README.md") -TargetDir $publishDir
Copy-IfExists -Source (Join-Path $ProjectRoot "deploy\update_manifest.sample.json") -TargetDir $publishDir

$externalConfigRoot = Join-Path (Split-Path -Parent $ProjectRoot) "RTU_Parameter_Settings"
foreach ($name in @("baseparaminfo.ini", "instancemap.ini", "sensorinstance.ini", "sensortypeitemconfig.ini")) {
    Copy-IfExists -Source (Join-Path $externalConfigRoot $name) -TargetDir $publishDir
}

$versionMatch = Select-String -Path (Join-Path $ProjectRoot "app_metadata.h") -Pattern 'APP_VERSION_LITERAL QStringLiteral\("([^"]+)"\)' |
    Select-Object -First 1
$version = if ($versionMatch) { $versionMatch.Matches[0].Groups[1].Value } else { "1.0.0" }
Set-Content -Path (Join-Path $publishDir "version.txt") -Value $version -Encoding UTF8

$qtHostBins = (& $QMakePath -query QT_HOST_BINS).Trim()
$windeployqt = Join-Path $qtHostBins "windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    throw "windeployqt not found: $windeployqt"
}

& $windeployqt --release --compiler-runtime --no-translations --dir $publishDir $mainExe
& $windeployqt --release --compiler-runtime --no-translations --dir $publishDir $updaterExe

$zipPath = Join-Path $publishRootResolved ("RTU_Parameter_Config_Tool-{0}.zip" -f $version)
if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

Compress-Archive -Path (Join-Path $publishDir "*") -DestinationPath $zipPath

Write-Host "Publish dir created: $publishDir"
Write-Host "Zip created: $zipPath"
