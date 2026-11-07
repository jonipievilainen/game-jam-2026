$ErrorActionPreference = 'Stop'

$projectDir = $PSScriptRoot
$sdkVersion = '0.24'
$sdkHome = if ($env:PSN00BSDK_HOME) {
	$env:PSN00BSDK_HOME
} else {
	Join-Path $env:USERPROFILE ".local\opt\psn00bsdk-$sdkVersion"
}
$sdkCmake = Join-Path $sdkHome 'lib\libpsn00b\cmake\sdk.cmake'
$cmakePath = (Get-Command cmake -CommandType Application -ErrorAction SilentlyContinue).Source
if (-not $cmakePath) {
	$userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
	foreach ($entry in $userPath -split ';' | Where-Object { $_ }) {
		$candidate = Join-Path $entry 'cmake.exe'
		if (Test-Path $candidate) {
			$cmakePath = $candidate
			break
		}
	}
}

if (-not (Test-Path $sdkCmake)) {
	throw "PSn00bSDK not found. Run $projectDir\setup.ps1 first."
}
if (-not $cmakePath) {
	throw 'CMake not found. Run setup.ps1, then open a new terminal if it was just installed.'
}

$env:PSN00BSDK_LIBS = Join-Path $sdkHome 'lib\libpsn00b'
$env:PATH = "$(Split-Path -Parent $cmakePath);$(Join-Path $sdkHome 'bin');$env:PATH"

& $cmakePath --preset windows -S $projectDir
if ($LASTEXITCODE -ne 0) {
	throw "CMake configuration failed with exit code $LASTEXITCODE."
}

& $cmakePath --build --preset windows
if ($LASTEXITCODE -ne 0) {
	throw "Build failed with exit code $LASTEXITCODE."
}
