#!/bin/sh
# Returns true if Debian/Ubuntu packages should be skipped because this CI run
# was triggered by a pull request.

# Verify BALTO_TOKEN is present (only set on github.com/i3/i3),
# otherwise the CI run was triggered by a pull request.
# Verify CC=gcc so that we only build packages once for each commit,
# not twice (with gcc and clang).
if [ ! -z "$BALTO_TOKEN" ] && [ "$CC" = "gcc" ]
then
	exit 1
fi

exit 0
