#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
duckstation="${DUCKSTATION:-}"

if ! command -v gdb-multiarch >/dev/null 2>&1; then
	printf 'Install the MIPS-capable debugger first: sudo apt install gdb-multiarch\n' >&2
	exit 1
fi

if [[ -z "$duckstation" ]]; then
	if [[ -x "${HOME}/.local/opt/duckstation/DuckStation-x64.AppImage" ]]; then
		duckstation="${HOME}/.local/opt/duckstation/DuckStation-x64.AppImage"
	elif command -v duckstation-qt >/dev/null 2>&1; then
		duckstation="$(command -v duckstation-qt)"
	elif command -v duckstation >/dev/null 2>&1; then
		duckstation="$(command -v duckstation)"
	else
		printf 'DuckStation not found. Set DUCKSTATION to its executable path.\n' >&2
		exit 1
	fi
fi

if [[ ! -x "$duckstation" ]]; then
	printf 'DuckStation is not executable: %s\n' "$duckstation" >&2
	exit 1
fi

port_open() {
	(exec 3<>/dev/tcp/127.0.0.1/2345) 2>/dev/null
}

if ! port_open; then
	"$duckstation" "${project_dir}/build/hello.cue" >/dev/null 2>&1 &
fi

for ((attempt = 0; attempt < 60; attempt++)); do
	if port_open; then
		printf 'DuckStation GDB server is ready on 127.0.0.1:2345.\n'
		exit 0
	fi
	sleep 1
done

printf 'DuckStation GDB server did not start. In DuckStation enable Settings > Debugging > Enable GDB Server (port 2345), then retry F5.\n' >&2
exit 1
