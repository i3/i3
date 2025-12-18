#!/bin/bash
# AI Optimize wrapper script
# Runs in background so it doesn't optimize its own terminal window

# Run the optimization in background and capture output
(
    output=$(ai3 layout "optimize and balance windows" 2>&1)
    exit_code=$?

    # Show notification with result
    if [ $exit_code -eq 0 ]; then
        notify-send -u normal -t 3000 "ai3 Layout" "Layout optimized" 2>/dev/null || true
    else
        notify-send -u critical -t 5000 "ai3 Layout" "Optimization failed - check /tmp/ai3-optimize.log" 2>/dev/null || true
        echo "$output" > /tmp/ai3-optimize.log
    fi
) &

# Exit immediately so the terminal closes
exit 0
