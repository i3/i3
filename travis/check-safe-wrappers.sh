#!/bin/sh
funcs='malloc|calloc|realloc|strdup|strndup|asprintf|write'
cstring='"([^"\\]|\\.)*"'
cchar="'[^\\\\]'|'\\\\.[^']*'"
regex="^([^'\"]|${cstring}|${cchar})*\<(${funcs})\>"
detected=0
while IFS= read -r file; do
    if { cpp -w -fpreprocessed "$file" || exit "$?"; } | grep -E -- "$regex"; then
        echo "^ $file calls a function that has a safe counterpart."
        detected=1
    fi
done << EOF
$(find -name '*.c' -not -name safewrappers.c -not -name strndup.c)
EOF
if [ "$detected" -ne 0 ]; then
    echo
    echo "Calls of functions that have safe counterparts were detected."
    exit 1
fi
