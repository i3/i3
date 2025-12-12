#!/bin/sh

set -euxo pipefail

cd build

# Try running the tests in parallel so that the common case (tests pass) is
# quick, but fall back to running them in sequence to make debugging easier.
if ! ninja test
then
	./complete-run.pl --parallel=1 || (cat latest/complete-run.log; false)
fi
