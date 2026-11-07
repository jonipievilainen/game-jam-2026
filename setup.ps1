$ErrorActionPreference = 'Stop'

$sdkVersion = '0.24'
$sdkHome = if ($env:PSN00BSDK_HOME) {
	$env:PSN00BSDK_HOME
} else {
	Join-Path $env:USERPROFILE ".local\opt\psn00bsdk-$sdkVersion"
}
$sdkCmake = Join-Path $sdkHome 'lib\libpsn00b\cmake\sdk.cmake'

function Get-CMakePath {
	$command = Get-Command cmake -CommandType Application -ErrorAction SilentlyContinue
	if ($command) {
		return $command.Source
	}

	$searchPaths = @(
		(Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe'),
		(Join-Path $env:LOCALAPPDATA 'Programs\CMake\bin\cmake.exe')
	)
	$searchPaths += $env:PATH -split ';'
	$searchPaths += [Environment]::GetEnvironmentVariable('Path', 'User') -split ';'
	foreach ($searchPath in $searchPaths | Where-Object { $_ }) {
		$candidate = if ($searchPath.EndsWith('cmake.exe', [StringComparison]::OrdinalIgnoreCase)) {
			$searchPath
		} else {
			Join-Path $searchPath 'cmake.exe'
		}
		if (Test-Path $candidate) {
			return $candidate
		}
	}
	return $null
}

$cmakePath = Get-CMakePath
$cmakeVersion = $null
if ($cmakePath) {
	$versionOutput = & $cmakePath --version | Select-Object -First 1
	if ($versionOutput -match 'cmake version (\d+\.\d+(?:\.\d+)?)') {
		$cmakeVersion = [version]$Matches[1]
	}
}

if (-not $cmakeVersion -or $cmakeVersion -lt [version]'3.21') {
	$winget = Get-Command winget -CommandType Application -ErrorAction SilentlyContinue
	if (-not $winget) {
		throw 'CMake 3.21 or newer is required. Install it from https://cmake.org/download/ and rerun setup.ps1.'
	}

	if ($cmakePath) {
		& $winget.Source upgrade --id Kitware.CMake --exact --silent --accept-source-agreements --accept-package-agreements
	} else {
		& $winget.Source install --id Kitware.CMake --exact --silent --accept-source-agreements --accept-package-agreements
	}
	if ($LASTEXITCODE -ne 0) {
		throw "CMake installation failed with exit code $LASTEXITCODE."
	}

	$cmakePath = Get-CMakePath
	if (-not $cmakePath) {
		throw 'CMake was installed, but cmake.exe was not found. Open a new terminal and rerun setup.ps1.'
	}
	$versionOutput = & $cmakePath --version | Select-Object -First 1
	if ($versionOutput -notmatch 'cmake version (\d+\.\d+(?:\.\d+)?)' -or [version]$Matches[1] -lt [version]'3.21') {
		throw "CMake 3.21 or newer is required; found: $versionOutput"
	}
}

if (-not (Test-Path $sdkCmake)) {
	if (Test-Path $sdkHome) {
		throw "Refusing to overwrite an incomplete PSn00bSDK directory: $sdkHome"
	}

	$archiveUrl = "https://github.com/Lameguy64/PSn00bSDK/releases/download/v$sdkVersion/PSn00bSDK-$sdkVersion-win32.zip"
	$tempRoot = Join-Path ([IO.Path]::GetTempPath()) "psn00bsdk-$([guid]::NewGuid())"
	$archive = "$tempRoot.zip"
	try {
		[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
		Invoke-WebRequest -Uri $archiveUrl -OutFile $archive
		New-Item -ItemType Directory -Path $tempRoot | Out-Null
		Expand-Archive -LiteralPath $archive -DestinationPath $tempRoot
		$sdkFile = Get-ChildItem -LiteralPath $tempRoot -Filter sdk.cmake -File -Recurse |
			Where-Object { $_.FullName -match '[\\/]lib[\\/]libpsn00b[\\/]cmake[\\/]sdk\.cmake$' } |
			Select-Object -First 1
		if (-not $sdkFile) {
			throw 'Downloaded PSn00bSDK archive does not contain a valid SDK installation.'
		}

		$sdkRoot = $sdkFile.Directory.Parent.Parent.Parent.FullName
		New-Item -ItemType Directory -Path (Split-Path -Parent $sdkHome) -Force | Out-Null
		Move-Item -LiteralPath $sdkRoot -Destination $sdkHome
	} finally {
		if (Test-Path $archive) {
			Remove-Item -LiteralPath $archive -Force
		}
		if (Test-Path $tempRoot) {
			Remove-Item -LiteralPath $tempRoot -Recurse -Force
		}
	}
}

Write-Host "PSn00bSDK $sdkVersion is ready at $sdkHome"
