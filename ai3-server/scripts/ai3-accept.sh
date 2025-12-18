#!/bin/bash
# Accept pending AI suggestion

SUGGESTION_FILE="/tmp/ai3-suggestion.json"

if [ ! -f "$SUGGESTION_FILE" ]; then
    notify-send -u normal -t 2000 "ai3" "No pending suggestion" 2>/dev/null || true
    exit 1
fi

# IMPORTANT: Wait for modifier keys to be released before typing
# This prevents Ctrl/Shift from being held during xdotool type
sleep 0.3

# Read and execute suggestion
python3 << 'PYEOF'
import json
import subprocess
import os
import time

with open('/tmp/ai3-suggestion.json', 'r') as f:
    data = json.load(f)

full = data.get('full', {})
action_type = full.get('action_type', 'command')
keys = full.get('keys', '')
command = full.get('command', '')
description = full.get('description', 'action')

def notify(msg):
    subprocess.run(['notify-send', '-u', 'normal', '-t', '2000', 'ai3', msg],
                   capture_output=True)

# Handle based on action type
if action_type == 'keys' and keys:
    # Check if it's text to type (no modifier keys) or a key combo
    modifiers = ['ctrl', 'alt', 'super', 'shift+', 'return', 'enter', 'escape', 'tab']
    is_key_combo = any(mod in keys.lower() for mod in modifiers)

    if is_key_combo:
        # It's a keyboard shortcut - use xdotool key
        subprocess.run(['xdotool', 'key', keys], check=False)
        notify(f'Pressed: {keys}')
    else:
        # It's text to type - use xdotool type
        # Clear any held modifiers first
        subprocess.run(['xdotool', 'keyup', 'ctrl', 'shift', 'alt', 'super'], check=False)
        time.sleep(0.1)
        result = subprocess.run(['xdotool', 'type', '--clearmodifiers', '--delay', '30', keys],
                               capture_output=True, text=True)
        if result.returncode == 0:
            notify(f'Typed: {keys[:30]}...' if len(keys) > 30 else f'Typed: {keys}')
        else:
            notify(f'Type failed: {result.stderr}')

elif action_type == 'workspace' and command:
    # Workspace switch via i3-msg
    subprocess.run(['i3-msg', command], check=False)
    notify(f'Switched workspace')

elif command:
    # General i3 command
    subprocess.run(['i3-msg', command], check=False)
    notify(f'Executed: {description[:30]}')

else:
    notify('No action to execute')
PYEOF

# Remove suggestion file
rm -f "$SUGGESTION_FILE"
