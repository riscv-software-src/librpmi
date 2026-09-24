#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
#
#   scripts/checkpatch.sh			check origin/main..HEAD
#   scripts/checkpatch.sh BASE..HEAD [REF...]	check a range, skipping any
#						commit already in a REF
#   scripts/checkpatch.sh -f FILE...		check whole files

set -e

KERNEL_TAG=v7.2
 
fetch_checkpatch()
{
	url=https://raw.githubusercontent.com/torvalds/linux/$KERNEL_TAG/scripts
 
	if [ -x "$cpdir/checkpatch.pl" ]; then
		return
	fi
 
	mkdir -p "$cpdir"
	for f in checkpatch.pl spelling.txt const_structs.checkpatch; do
		curl -sSfL "$url/$f" -o "$cpdir/$f"
	done
	chmod +x "$cpdir/checkpatch.pl"
}
 
echo "checkpatch: using Linux $KERNEL_TAG"

top=$(git rev-parse --show-toplevel)
cpdir=$top/build/checkpatch/$KERNEL_TAG
fetch_checkpatch
cd "$top"

if [ "$1" = -f ]; then
	shift
	exec "$cpdir/checkpatch.pl" --show-types -f "$@"
fi

range=${1:-origin/main..HEAD}
if [ $# -gt 0 ]; then
	shift
fi

exclude="^${range%%..*}"
for ref; do
	exclude="$exclude ^$ref"
done

commits=$(git rev-list --no-merges "${range##*..}" $exclude)
if [ -z "$commits" ]; then
	echo "checkpatch: no new commits to check in $range"
	exit 0
fi

echo "checkpatch: checking $(echo "$commits" | wc -l) commit(s)"
exec "$cpdir/checkpatch.pl" --show-types -g $commits
