#!/bin/sh

set -e
set -x

clang-format-9 -i $(find . -name "*.[ch]" | tr '\n' ' ') && git diff --exit-code || (echo 'Code was not formatted using clang-format!'; false)
