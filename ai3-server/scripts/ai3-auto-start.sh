#!/bin/bash
# ai3-auto-start.sh - Start Auto Mode with goal input

SERVER_URL="${AI3_SERVER_URL:-http://localhost:7878}"
ERROR_FILE="/tmp/ai3-auto-error.txt"

# Show error ONLY in xterm window - no stderr output
show_error() {
    cat > "$ERROR_FILE" << ERREOF
========================================
        AI3 AUTO MODE ERROR
========================================

$1

$2

----------------------------------------
Auto Mode requires OpenAI Computer Use
Agent which is NOT available with the
standard OpenAI API.

Use 'ai3 chat' instead for interactive
AI assistance with full capabilities.
----------------------------------------

Press Enter to close...
ERREOF
    xterm -title "ai3 Error" -geometry 50x22 -e "cat '$ERROR_FILE'; read"
    exit 1
}

# Check server status
status=$(curl -s "$SERVER_URL/auto/status" 2>/dev/null)
if [ $? -eq 0 ]; then
    cs=$(echo "$status" | jq -r '.state.status' 2>/dev/null)
    if [ "$cs" = "running" ]; then
        show_error "Already Running" "Auto mode is already running."
    fi
fi

# Get goal from user
if command -v zenity &>/dev/null; then
    goal=$(zenity --entry --title="ai3 Auto Mode" \
        --text="Enter your goal:" --width=400 2>/dev/null)
else
    show_error "Missing Tool" "zenity not found"
fi

[ -z "$goal" ] && exit 0

# Start auto mode - capture response silently
resp=$(curl -s -w "\n%{http_code}" -X POST "$SERVER_URL/auto/start" \
    -H "Content-Type: application/json" \
    -d "{\"goal\": \"$goal\"}" 2>/dev/null)

code=$(echo "$resp" | tail -n1)
body=$(echo "$resp" | sed '$d')

if [ "$code" != "200" ]; then
    msg=$(echo "$body" | jq -r '.state.error // .detail // .message // empty' 2>/dev/null)
    [ -z "$msg" ] && msg="$body"
    show_error "Error $code" "$msg"
fi

# Success - just show brief notification
notify-send -t 2000 "ai3" "Auto Mode started" 2>/dev/null
