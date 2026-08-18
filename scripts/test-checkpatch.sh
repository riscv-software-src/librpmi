#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause

set -eu

top=$(git rev-parse --show-toplevel)
tag=unverified-test
cpdir=$top/build/checkpatch/$tag

cleanup()
{
	rm -rf "$cpdir"
}
trap cleanup EXIT HUP INT TERM

mkdir -p "$cpdir"
printf '%s\n' '#!/bin/sh' 'exit 0' > "$cpdir/checkpatch.pl"
printf '%s\n' fixture > "$cpdir/spelling.txt"
printf '%s\n' fixture > "$cpdir/const_structs.checkpatch"
fixture=$cpdir/fixture.c
printf '%s\n' 'int fixture;' > "$fixture"
chmod +x "$cpdir/checkpatch.pl"

KERNEL_TAG=$tag "$top/scripts/checkpatch.sh" -f "$fixture" >/dev/null 2>&1
output=$(KERNEL_TAG=$tag "$top/scripts/checkpatch.sh" HEAD..HEAD 2>&1)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -F \
	"checkpatch: warning: $tag is not checksum-verified" >/dev/null
printf '%s\n' "$output" | grep -F \
	'checkpatch: no new commits to check in HEAD..HEAD' >/dev/null
