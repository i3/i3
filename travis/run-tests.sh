#!/bin/sh

set -e
set -x

cd build

# TODO: remove this workaround once https://bugs.debian.org/836723 is fixed
# Found at https://llvm.org/bugs/show_bug.cgi?id=27310#c8:
if [ "$CC" = "clang" ]
then
	cat >fixasan.c <<EOT
void __isoc99_printf() {}
void __isoc99_sprintf() {}
void __isoc99_snprintf() {}
void __isoc99_fprintf() {}
void __isoc99_vprintf() {}
void __isoc99_vsprintf() {}
void __isoc99_vsnprintf() {}
void __isoc99_vfprintf() {}
void __cxa_throw() {} // NEW
EOT
	gcc fixasan.c -o fixasan.so -fPIC -shared -nostdlib
	export LD_PRELOAD=$PWD/fixasan.so
fi

# Try running the tests in parallel so that the common case (tests pass) is
# quick, but fall back to running them in sequence to make debugging easier.
if ! make check
then
	./testcases/complete-run.pl --parallel=1 || (cat latest/complete-run.log; false)
fi
