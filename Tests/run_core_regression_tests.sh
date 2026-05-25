#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_bin="${TMPDIR:-/tmp}/q1view-core-regression-tests-$$"

cleanup()
{
	rm -f "$test_bin"
}
trap cleanup EXIT INT TERM

"${CC:-cc}" -std=c11 -Wall -Wextra -Wno-unused-parameter \
	-I "$repo_root/QCommon/inc" \
	-I "$repo_root/QVisionCore" \
	"$repo_root/Tests/CoreRegressionTests.c" \
	"$repo_root/QVisionCore/qimage_cs.c" \
	"$repo_root/QVisionCore/qimage_metrics.c" \
	-lm -o "$test_bin"

"$test_bin"
