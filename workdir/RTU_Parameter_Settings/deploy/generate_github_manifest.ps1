param(
    [Parameter(Mandatory = $true)]
    [string]$ZipPath,

    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [string]$Repo,

    [string]$Notes = "",
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ZipPath)) {
    throw "未找到 zip 文件：$ZipPath"
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path (Split-Path -Parent $ZipPath) "update_manifest.json"
}

$zipName = Split-Path -Leaf $ZipPath
$hash = (Get-FileHash -Path $ZipPath -Algorithm SHA256).Hash.ToLower()
$downloadUrl = "https://github.com/$Repo/releases/download/v$Version/$zipName"

$manifest = [ordered]@{
    version = $Version
    url     = $downloadUrl
    sha256  = $hash
    notes   = $Notes
}

$json = $manifest | ConvertTo-Json -Depth 4
Set-Content -Path $OutputPath -Value $json -Encoding UTF8

Write-Host "已生成升级清单：" $OutputPath
Write-Host "下载地址：" $downloadUrl
Write-Host "SHA256：" $hash
