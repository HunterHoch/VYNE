param([switch]$EnableModeProbe)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$sourceBinary = Join-Path $projectRoot 'build\VYNE.asi'
if (!(Test-Path -LiteralPath $sourceBinary)) { throw 'Build VYNE first with scripts\build.cmd.' }
if (Get-Process -Name 'FiveM*GTAProcess*' -ErrorAction SilentlyContinue) {
    throw 'Close FiveM before installing VYNE; this does not replace a running plugin.'
}
$pluginDirectory = Join-Path $env:LOCALAPPDATA 'FiveM\FiveM.app\plugins'
if (!(Test-Path -LiteralPath $pluginDirectory)) { throw 'Existing FiveM plugins folder not found.' }
$destination = Join-Path $pluginDirectory 'VYNE.asi'
$configDirectory = Join-Path $pluginDirectory 'VYNE'
if (Test-Path -LiteralPath $destination) {
    $backupDirectory = Join-Path $projectRoot ('build\install-backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null
    Copy-Item -LiteralPath $destination -Destination $backupDirectory
}
New-Item -ItemType Directory -Path $configDirectory -Force | Out-Null
$configuration = Join-Path $configDirectory 'VYNE.ini'
if (Test-Path -LiteralPath $configuration) {
    $configBackup = Join-Path $projectRoot ('build\VYNE-ini-backup-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.ini')
    Copy-Item -LiteralPath $configuration -Destination $configBackup
}
$enabled = if ($EnableModeProbe) { 1 } else { 0 }
Set-Content -LiteralPath $configuration -Value "[Diagnostics]`r`nEnableExportMenu=1`r`nEnableModeProbe=$enabled" -Encoding ascii
Copy-Item -LiteralPath (Join-Path $projectRoot 'vendor\minhook\LICENSE.txt') -Destination (Join-Path $configDirectory 'MinHook-LICENSE.txt') -Force
Copy-Item -LiteralPath $sourceBinary -Destination $destination -Force
Get-FileHash -LiteralPath $sourceBinary, $destination
Write-Output 'Installed VYNE only. Existing unrelated plugins and game settings were not changed.'
