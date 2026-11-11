param([Parameter(Mandatory=$true)][string]$RendererPath)
$ErrorActionPreference = 'Stop'
$targetPath = (Resolve-Path -LiteralPath $RendererPath).Path
$patchPath = Join-Path $PSScriptRoot 'upstream.patch'
& git -C $targetPath apply --check --ignore-space-change $patchPath
if ($LASTEXITCODE -ne 0) { throw 'Integration patch does not apply. No files were changed.' }
& git -C $targetPath apply --ignore-space-change $patchPath
if ($LASTEXITCODE -ne 0) { throw 'Integration patch failed.' }
Write-Host 'Integration hooks applied. No commit was created.'
Write-Host "Configure with -DPRS_VOLUMETRICS_DIR=<absolute path to volumetrics>"
