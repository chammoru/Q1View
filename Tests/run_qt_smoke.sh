#!/bin/sh
# Minimal headless smoke check for the Qt viewer: open each committed raw-format
# fixture through the built executable's --selftest mode (offscreen QPA) and
# assert a clean exit. CI runs the same checks via ctest (see CMakeLists.txt);
# this script is the standalone, no-CMake equivalent for local use.
#
# Usage:
#   Tests/run_qt_smoke.sh [path-to-q1view_viewer_qt]
#
# With no argument it searches under ./build for the executable.
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
fixtures="${repo_root}/Tests/fixtures/raw"

viewer="${1:-}"
if [ -z "${viewer}" ]; then
	viewer=$(find "${repo_root}/build" \
		-type f \( -name q1view_viewer_qt -o -name q1view_viewer_qt.exe \) \
		-perm -111 2>/dev/null | head -n 1 || true)
	# macOS .app bundle layout
	if [ -z "${viewer}" ]; then
		viewer=$(find "${repo_root}/build" -type f \
			-path '*q1view_viewer_qt.app/Contents/MacOS/q1view_viewer_qt' \
			2>/dev/null | head -n 1 || true)
	fi
fi

if [ -z "${viewer}" ] || [ ! -x "${viewer}" ]; then
	echo "q1view_viewer_qt executable not found; build it first or pass its path" >&2
	exit 1
fi

# "format" only; all fixtures are 16x16 and the extension matches the format.
formats="yuv420 nv12 nv21 yuv420p10le p010 grayscale rgb888 bgr888 rgba8888 bgr565"

export QT_QPA_PLATFORM=offscreen

failures=0
for fmt in ${formats}; do
	fixture="${fixtures}/gradient_16x16.${fmt}"
	if [ ! -f "${fixture}" ]; then
		echo "MISSING ${fixture}" >&2
		failures=$((failures + 1))
		continue
	fi
	if "${viewer}" --selftest --raw --width 16 --height 16 \
		--format "${fmt}" "${fixture}"; then
		echo "PASS ${fmt}"
	else
		echo "FAIL ${fmt}" >&2
		failures=$((failures + 1))
	fi
done

if [ "${failures}" -ne 0 ]; then
	echo "${failures} Qt smoke check(s) failed" >&2
	exit 1
fi
echo "all Qt smoke checks passed"
