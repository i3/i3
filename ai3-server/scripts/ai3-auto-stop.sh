#!/bin/bash
# ai3-auto-stop.sh - Stop Auto Mode
# Immediately stops the CUA agent

SERVER_URL="${AI3_SERVER_URL:-http://localhost:7878}"
STATUS_FILE="/tmp/ai3-auto-status.json"

# Function to show notification
notify() {
    notify-send -u normal -t 3000 "ai3 Auto Mode" "$1" 2>/dev/null || true
}

# Function to show error
error() {
    notify-send -u critical -t 5000 "ai3 Auto Mode Error" "$1" 2>/dev/null || true
    echo "Error: $1" >&2
}

# Check current status
status=$(curl -s "$SERVER_URL/auto/status" 2>/dev/null)
if [ $? -ne 0 ]; then
    error "Failed to connect to ai3 server"
    exit 1
fi

current_status=$(echo "$status" | jq -r '.state.status' 2>/dev/null)
if [ "$current_status" != "running" ]; then
    notify "Auto mode is not running"
    exit 0
fi

# Stop auto mode
response=$(curl -s -X POST "$SERVER_URL/auto/stop" 2>/dev/null)

if [ $? -ne 0 ]; then
    error "Failed to connect to ai3 server"
    exit 1
fi

# Check response
success=$(echo "$response" | jq -r '.success' 2>/dev/null)
message=$(echo "$response" | jq -r '.message' 2>/dev/null)
actions=$(echo "$response" | jq -r '.state.actions_taken' 2>/dev/null)

if [ "$success" = "true" ]; then
    notify "🛑 Auto Mode Stopped\n\nActions taken: $actions"
    rm -f "$STATUS_FILE" 2>/dev/null
else
    error "$message"
    exit 1
fi
