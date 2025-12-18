#!/bin/bash
# ai3 Desktop Entrypoint Script
# Starts Xvfb, i3, ai3-server (Python), and noVNC for remote access

set -e

# Dracula theme colors
PURPLE='\033[38;2;189;147;249m'
CYAN='\033[38;2;139;233;253m'
GREEN='\033[38;2;80;250;123m'
YELLOW='\033[38;2;241;250;140m'
RED='\033[38;2;255;85;85m'
COMMENT='\033[38;2;98;114;164m'
RESET='\033[0m'

echo -e "${PURPLE}"
echo "    █████╗ ██╗██████╗ "
echo "   ██╔══██╗██║╚════██╗"
echo "   ███████║██║ █████╔╝"
echo "   ██╔══██║██║ ╚═══██╗"
echo "   ██║  ██║██║██████╔╝"
echo "   ╚═╝  ╚═╝╚═╝╚═════╝ ${RESET}"
echo ""
echo -e "   ${CYAN}AI-Powered i3 Window Manager${RESET}"
echo -e "   ${COMMENT}Fast • Beautiful • Intelligent${RESET}"
echo -e "   ${COMMENT}Python Edition v2.0${RESET}"
echo ""

# ============================================================
# SECURE SECRETS HANDLING
# Load secrets from file if present, then securely delete
# ============================================================
SECRETS_FILE="/tmp/.ai3_secrets"
if [ -f "$SECRETS_FILE" ]; then
    echo -e "[ai3] ${GREEN}Loading AI configuration from secure secrets file...${RESET}"
    while IFS='=' read -r key value; do
        [[ -z "$key" || "$key" =~ ^[[:space:]]*# ]] && continue
        key=$(echo "$key" | xargs)
        key=${key#export }
        value=$(echo "$value" | sed 's/^["'"'"']//;s/["'"'"']$//')
        export "$key=$value"
    done < "$SECRETS_FILE"
    shred -u "$SECRETS_FILE" 2>/dev/null || rm -f "$SECRETS_FILE" 2>/dev/null || true
    echo -e "[ai3] ${GREEN}Secrets loaded and file securely deleted${RESET}"
fi

# Configuration
DISPLAY_NUM=99
export DISPLAY=:${DISPLAY_NUM}
SCREEN_GEOMETRY="${SCREEN_WIDTH}x${SCREEN_HEIGHT}x${SCREEN_DEPTH}"

echo "[ai3] Starting with resolution: ${SCREEN_WIDTH}x${SCREEN_HEIGHT}"

# Start D-Bus (needed for some desktop apps)
echo "[ai3] Starting D-Bus..."
mkdir -p /run/dbus
dbus-daemon --system --fork 2>/dev/null || true
export $(dbus-launch)

# Start Xvfb (virtual framebuffer)
echo "[ai3] Starting Xvfb on display :${DISPLAY_NUM}..."
Xvfb :${DISPLAY_NUM} -screen 0 ${SCREEN_GEOMETRY} -ac +extension GLX +render -noreset &
XVFB_PID=$!
sleep 2

# Verify Xvfb is running
if ! kill -0 $XVFB_PID 2>/dev/null; then
    echo "[ai3] ERROR: Xvfb failed to start"
    exit 1
fi
echo "[ai3] Xvfb started successfully (PID: $XVFB_PID)"

# Load Xresources (terminal colors, etc.)
echo "[ai3] Loading Xresources..."
if [ -f /root/.Xresources ]; then
    xrdb -merge /root/.Xresources
fi

# Set wallpaper with feh
echo "[ai3] Setting wallpaper..."
if [ -f /root/Pictures/wallpaper.png ]; then
    feh --bg-fill /root/Pictures/wallpaper.png &
fi

# Start picom compositor (for transparency and effects)
echo "[ai3] Starting picom compositor..."
picom --config /root/.config/picom/picom.conf --backend xrender -b 2>/dev/null || \
picom --backend xrender -b 2>/dev/null || \
echo "[ai3] Warning: picom not available, continuing without compositor"

# Start i3 window manager
echo "[ai3] Starting i3 window manager..."
i3 &
I3_PID=$!
sleep 2

# Verify i3 is running
if ! kill -0 $I3_PID 2>/dev/null; then
    echo "[ai3] ERROR: i3 failed to start"
    exit 1
fi
echo "[ai3] i3 started successfully (PID: $I3_PID)"

# ============================================================
# START AI3-SERVER (Python)
# ============================================================
echo ""
echo -e "[ai3] ${CYAN}Starting ai3-server (Python)...${RESET}"

# Check if OpenAI is configured
if [ -n "$OPENAI_KEY" ]; then
    echo -e "[ai3] ${GREEN}✓ OPENAI_KEY is set${RESET}"
    echo -e "[ai3]   Model: ${CYAN}${OPENAI_MODEL:-gpt-4.1-mini}${RESET}"

    # Start ai3-server in background
    python3 -m ai3_server.server &
    AI3_SERVER_PID=$!
    sleep 2

    if kill -0 $AI3_SERVER_PID 2>/dev/null; then
        echo -e "[ai3] ${GREEN}✓ ai3-server started (PID: $AI3_SERVER_PID)${RESET}"
        echo -e "[ai3]   API: ${CYAN}http://localhost:7878${RESET}"
    else
        echo -e "[ai3] ${RED}✗ ai3-server failed to start${RESET}"
    fi
else
    echo -e "[ai3] ${YELLOW}⚠ AI features DISABLED (OPENAI_KEY not set)${RESET}"
    echo -e "[ai3]   ${COMMENT}To enable AI features, set:${RESET}"
    echo -e "[ai3]   ${COMMENT}  OPENAI_KEY=your-key${RESET}"
    echo -e "[ai3]   ${COMMENT}  OPENAI_MODEL=gpt-4.1-mini (optional)${RESET}"
    AI3_SERVER_PID=""
fi
echo ""

# Start x11vnc (VNC server)
echo "[ai3] Starting x11vnc..."
x11vnc -display :${DISPLAY_NUM} \
    -forever \
    -shared \
    -rfbport 5900 \
    -nopw \
    -xkb \
    -clip ${SCREEN_WIDTH}x${SCREEN_HEIGHT}+0+0 \
    -bg \
    -o /var/log/x11vnc.log

sleep 1
echo "[ai3] x11vnc started"

# Start noVNC via websockify
echo "[ai3] Starting noVNC on port 6080..."
websockify --web=/usr/share/novnc/ 6080 localhost:5900 &
NOVNC_PID=$!
sleep 2

if ! kill -0 $NOVNC_PID 2>/dev/null; then
    echo "[ai3] ERROR: noVNC failed to start"
    exit 1
fi

echo ""
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║                    ai3 Desktop Ready!                      ║"
echo "╠═══════════════════════════════════════════════════════════╣"
echo "║  Desktop: http://localhost:6080/vnc.html                  ║"
echo "║  AI API:  http://localhost:7878                           ║"
echo "║                                                            ║"
echo "║  AI Keyboard Shortcuts:                                   ║"
echo "║    Ctrl+Shift+S → Get AI suggestion (shown in bar)        ║"
echo "║    Ctrl+Shift+G → Accept suggestion                       ║"
echo "║    Ctrl+Shift+X → Discard suggestion                      ║"
echo "║    Ctrl+Shift+O → AI optimize layout                      ║"
echo "║    Ctrl+Shift+C → AI chat (terminal)                      ║"
echo "║    Ctrl+Shift+A → AI AUTO MODE (autonomous control!)      ║"
echo "║    Ctrl+Shift+Esc → STOP Auto Mode                        ║"
echo "║                                                            ║"
echo "║  Other Shortcuts:                                         ║"
echo "║    Ctrl+Shift+Enter → Open terminal                       ║"
echo "║    Ctrl+Shift+D     → Application launcher (dmenu)        ║"
echo "║    Ctrl+Shift+Q     → Close window                        ║"
echo "║    Ctrl+Alt+1-4     → Switch workspace                    ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

# Function to handle shutdown
cleanup() {
    echo "[ai3] Shutting down..."
    [ -n "$AI3_SERVER_PID" ] && kill $AI3_SERVER_PID 2>/dev/null || true
    kill $NOVNC_PID 2>/dev/null || true
    kill $I3_PID 2>/dev/null || true
    kill $XVFB_PID 2>/dev/null || true
    echo "[ai3] Goodbye!"
    exit 0
}

trap cleanup SIGTERM SIGINT

# Keep the container running
echo "[ai3] Container running. Press Ctrl+C to stop."
wait $NOVNC_PID
