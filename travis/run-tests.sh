#!/bin/sh

set -e
set -x

cd testcases
# Try running the tests in parallel so that the common case (tests pass) is
# quick, but fall back to running them in sequence to make debugging easier.
if ! xvfb-run ./complete-run.pl
then
	xvfb-run ./complete-run.pl --parallel=1 || (cat latest/complete-run.log; false)
fi
