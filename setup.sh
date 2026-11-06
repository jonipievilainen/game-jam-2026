#!/usr/bin/env bash
set -euo pipefail

sdk_version=0.24
sdk_home="${PSN00BSDK_HOME:-${HOME}/.local/opt/psn00bsdk-${sdk_version}}"
cmake_home="${PSN00BSDK_CMAKE_HOME:-${HOME}/.local/opt/psn00bsdk-cmake}"
sdk_url="https://github.com/Lameguy64/PSn00bSDK/releases/download/v${sdk_version}/PSn00bSDK-${sdk_version}-Linux.zip"

cmake_is_supported() {
	local version major minor
	version="$("$1" --version | awk 'NR == 1 {print $3}')"
	[[ "$version" =~ ^([0-9]+)\.([0-9]+) ]] || return 1
	major="${BASH_REMATCH[1]}"
	minor="${BASH_REMATCH[2]}"
	((major > 3 || (major == 3 && minor >= 21)))
}

if ! command -v cmake >/dev/null 2>&1 || ! cmake_is_supported "$(command -v cmake)"; then
	if [[ ! -x "${cmake_home}/bin/cmake" ]] || ! cmake_is_supported "${cmake_home}/bin/cmake"; then
		if ! command -v python3 >/dev/null 2>&1; then
			printf 'CMake 3.21+ is required; install CMake or Python 3 with venv support.\n' >&2
			exit 1
		fi
		mkdir -p "$(dirname "$cmake_home")"
		if [[ ! -x "${cmake_home}/bin/python" ]]; then
			python3 -m venv "$cmake_home"
		fi
		"${cmake_home}/bin/python" -m pip install --upgrade 'cmake>=3.21'
	fi
	export PATH="${cmake_home}/bin:${PATH}"
fi

if [[ -f "${sdk_home}/lib/libpsn00b/cmake/sdk.cmake" ]]; then
	printf 'PSn00bSDK %s is already installed at %s\n' "$sdk_version" "$sdk_home"
	exit 0
fi

if [[ -e "$sdk_home" ]]; then
	printf 'Refusing to overwrite an incomplete SDK directory: %s\n' "$sdk_home" >&2
	exit 1
fi

for tool in curl unzip; do
	if ! command -v "$tool" >/dev/null 2>&1; then
		printf 'Required command not found: %s\n' "$tool" >&2
		exit 1
	fi
done

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

curl --fail --location --silent --show-error "$sdk_url" -o "${tmp_dir}/psn00bsdk.zip"
mkdir "${tmp_dir}/unpacked"
unzip -q "${tmp_dir}/psn00bsdk.zip" -d "${tmp_dir}/unpacked"

sdk_cmake="$(find "${tmp_dir}/unpacked" -path '*/lib/libpsn00b/cmake/sdk.cmake' -print -quit)"
if [[ -z "$sdk_cmake" ]]; then
	printf 'Downloaded archive does not contain a valid PSn00bSDK installation.\n' >&2
	exit 1
fi
sdk_root="${sdk_cmake%/lib/libpsn00b/cmake/sdk.cmake}"

mkdir -p "$(dirname "$sdk_home")"
mv "$sdk_root" "$sdk_home"
printf 'Installed PSn00bSDK %s at %s\n' "$sdk_version" "$sdk_home"
