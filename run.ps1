param(
	[string]$DuckStationExe = 'E:\duckstation\duckstation-qt-x64-ReleaseLTCG.exe'
)

$ErrorActionPreference = 'Stop'
$discImage = Join-Path $PSScriptRoot 'build\windows\hello.cue'

if (-not (Test-Path $DuckStationExe)) {
	throw "DuckStation was not found at '$DuckStationExe'. Pass its executable path with -DuckStationExe."
}
if (-not (Test-Path $discImage)) {
	throw "Built disc image not found at '$discImage'. Run .\build.ps1 first."
}

Start-Process -FilePath $DuckStationExe -ArgumentList "`"$discImage`""
