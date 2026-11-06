#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
sdk_home="${PSN00BSDK_HOME:-${HOME}/.local/opt/psn00bsdk-0.24}"
cmake_home="${PSN00BSDK_CMAKE_HOME:-${HOME}/.local/opt/psn00bsdk-cmake}"

if [[ -x "${cmake_home}/bin/cmake" ]]; then
	export PATH="${cmake_home}/bin:${PATH}"
fi
if ! command -v cmake >/dev/null 2>&1; then
	printf 'CMake 3.21 or newer is required. Run %s/setup.sh first.\n' "$project_dir" >&2
	exit 1
fi
if [[ ! -f "${sdk_home}/lib/libpsn00b/cmake/sdk.cmake" ]]; then
	printf 'PSn00bSDK not found. Run %s/setup.sh first.\n' "$project_dir" >&2
	exit 1
fi

export PSN00BSDK_LIBS="${sdk_home}/lib/libpsn00b"
export PATH="${sdk_home}/bin:${PATH}"

cmake --preset default -S "$project_dir"
cmake --build "${project_dir}/build"
