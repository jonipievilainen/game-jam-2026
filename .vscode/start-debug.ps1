$ErrorActionPreference = 'Stop'

$projectDir = Split-Path -Parent $PSScriptRoot
$duckstation = if ($env:DUCKSTATION) {
	$env:DUCKSTATION
} else {
	'E:\duckstation\duckstation-qt-x64-ReleaseLTCG.exe'
}
$discImage = Join-Path $projectDir 'build\windows\hello.cue'
$gdb = $env:PSN00BSDK_GDB

if (-not (Test-Path $duckstation)) {
	throw "DuckStation was not found at '$duckstation'. Set the DUCKSTATION environment variable."
}
if (-not (Test-Path $discImage)) {
	throw "Built disc image not found at '$discImage'. Build the Windows target first."
}
if (-not $gdb -or -not (Test-Path $gdb)) {
	throw 'A MIPS-capable Windows GDB is required. Set PSN00BSDK_GDB to its executable path before starting VS Code.'
}

function Test-DuckStationGdbServer {
	$client = [System.Net.Sockets.TcpClient]::new()
	try {
		$connection = $client.ConnectAsync('127.0.0.1', 2345)
		return $connection.Wait(500) -and $client.Connected
	} catch [System.AggregateException] {
		return $false
	} finally {
		$client.Dispose()
	}
}

if (-not (Test-DuckStationGdbServer)) {
	Start-Process -FilePath $duckstation -ArgumentList "`"$discImage`""
}

for ($attempt = 0; $attempt -lt 60; $attempt++) {
	if (Test-DuckStationGdbServer) {
		Write-Host 'DuckStation GDB server is ready on 127.0.0.1:2345.'
		exit 0
	}
	Start-Sleep -Seconds 1
}

throw 'DuckStation GDB server did not start. Enable Settings > Debugging > Enable GDB Server (port 2345), then retry F5.'
