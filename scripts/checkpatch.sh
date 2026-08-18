#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
#
# Default:             Check origin/main..HEAD
# BASE..HEAD [REF...]: Check a range, excluding commits already reachable from
#                      REF(s)
# -f FILE...:          Check complete files

set -eu

# CI uses this reviewed Linux release. Set KERNEL_TAG=master locally to opt in
# to a newer, unverified checker.
KERNEL_TAG=${KERNEL_TAG:-v7.2}

case "$KERNEL_TAG" in
v7.2)
	CHECKPATCH_SHA256=f8f32e6b559004fcac021e2f6149b6b7ea486924a8bc48297c0f317eddf5e220
	SPELLING_SHA256=4095d4a8810f115bae1b7c0d8a1946beb3435f6e22d9a48ac009bb024bad1e68
	CONSTS_SHA256=ea064f6916a74763468037494aeb270aae34b7c97617e84d424ca5b8733539b2
	verify_downloads=true
	;;
*)
	verify_downloads=false
	echo "checkpatch: warning: $KERNEL_TAG is not checksum-verified" >&2
	;;
esac

top=$(git rev-parse --show-toplevel)
cpdir=$top/build/checkpatch/$KERNEL_TAG
url=https://raw.githubusercontent.com/torvalds/linux/$KERNEL_TAG/scripts

verify_file()
{
	target=$1
	expected=$2
	actual=$(sha256sum "$target" | awk '{print $1}')
	if [ "$actual" != "$expected" ]; then
		echo "checkpatch: checksum mismatch for ${target##*/}" >&2
		rm -f "$target"
		exit 1
	fi
}

fetch_checkpatch()
{
	for file in checkpatch.pl spelling.txt const_structs.checkpatch; do
		if [ "$verify_downloads" = true ]; then
			case "$file" in
			checkpatch.pl) expected=$CHECKPATCH_SHA256 ;;
			spelling.txt) expected=$SPELLING_SHA256 ;;
			const_structs.checkpatch) expected=$CONSTS_SHA256 ;;
			esac
		fi

		if [ -f "$cpdir/$file" ]; then
			if [ "$verify_downloads" = true ]; then
				verify_file "$cpdir/$file" "$expected"
			fi
			continue
		fi

		mkdir -p "$cpdir"
		tmpfile=$cpdir/$file.tmp
		trap 'rm -f "$tmpfile"' EXIT HUP INT TERM
		curl -fsSL "$url/$file" -o "$tmpfile"
		if [ "$verify_downloads" = true ]; then
			verify_file "$tmpfile" "$expected"
		fi
		mv "$tmpfile" "$cpdir/$file"
	done
	trap - EXIT HUP INT TERM
	chmod +x "$cpdir/checkpatch.pl"
}

echo "checkpatch: using Linux $KERNEL_TAG"
fetch_checkpatch
cd "$top"

if [ "${1:-}" = "-f" ]; then
	shift
	[ "$#" -gt 0 ] || {
		echo "Usage: $0 -f FILE..." >&2
		exit 2
	}
	exec "$cpdir/checkpatch.pl" --show-types -f "$@"
fi

range=${1:-origin/main..HEAD}
if [ "$#" -gt 0 ]; then
	shift
fi

case "$range" in
*..*)
	base=${range%%..*}
	head=${range##*..}
	;;
*)
	echo "Usage: $0 [BASE..HEAD [REF...]]" >&2
	exit 2
	;;
esac

exclude="^$base"
for ref; do
	exclude="$exclude ^$ref"
done

commits=$(git rev-list --no-merges "$head" $exclude)
if [ -z "$commits" ]; then
	echo "checkpatch: no new commits to check in $range"
	exit 0
fi

echo "checkpatch: checking $(printf '%s\n' "$commits" | wc -l) commit(s)"
exec "$cpdir/checkpatch.pl" --show-types -g $commits
