#!/bin/sh
clang-format-3.5 -i $(find . -name "*.[ch]" | tr '\n' ' ') && git diff --exit-code || (echo 'Code was not formatted using clang-format!'; false)
