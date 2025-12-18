# ai3 Desktop Environment Container
# A beautiful, riced i3 environment with noVNC access and AI assistant
# Uses Python-based ai3-server for AI features

FROM ubuntu:22.04

# Avoid prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Display settings (can be overridden)
ENV SCREEN_WIDTH=1920
ENV SCREEN_HEIGHT=1080
ENV SCREEN_DEPTH=24
ENV DISPLAY=:99

# Install base packages and i3 dependencies
RUN apt-get update && apt-get install -y \
    # X server and VNC
    xvfb \
    x11vnc \
    novnc \
    websockify \
    # i3 and related tools
    i3 \
    i3status \
    i3lock \
    # Desktop utilities
    feh \
    picom \
    dmenu \
    xterm \
    # Text editors
    vim \
    nano \
    gedit \
    # File manager
    thunar \
    thunar-archive-plugin \
    # Media players
    vlc \
    mpv \
    # PDF viewer
    evince \
    # Image viewer
    eog \
    # Archive manager
    file-roller \
    # Calculator
    galculator \
    # Screenshot tool (additional)
    flameshot \
    # System monitor
    gnome-system-monitor \
    # Appearance settings
    lxappearance \
    # Audio control
    pavucontrol \
    pulseaudio \
    # Fonts
    fonts-dejavu \
    fonts-noto \
    fonts-font-awesome \
    # Utilities
    dbus-x11 \
    curl \
    wget \
    unzip \
    git \
    procps \
    net-tools \
    htop \
    neofetch \
    x11-xserver-utils \
    scrot \
    jq \
    # Keyboard/mouse automation for AI suggestions
    xdotool \
    # Dialog for Auto Mode goal input
    zenity \
    # Notifications for AI feedback
    libnotify-bin \
    # Python 3.10+
    python3 \
    python3-pip \
    python3-venv \
    # Clean up
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install Firefox from Mozilla PPA (better than snap in Docker)
RUN apt-get update && \
    apt-get install -y software-properties-common && \
    add-apt-repository -y ppa:mozillateam/ppa && \
    echo 'Package: *' > /etc/apt/preferences.d/mozilla-firefox && \
    echo 'Pin: release o=LP-PPA-mozillateam' >> /etc/apt/preferences.d/mozilla-firefox && \
    echo 'Pin-Priority: 1001' >> /etc/apt/preferences.d/mozilla-firefox && \
    apt-get update && \
    apt-get install -y firefox && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Install JetBrains Mono font
RUN mkdir -p /usr/share/fonts/jetbrains-mono && \
    cd /usr/share/fonts/jetbrains-mono && \
    wget -q https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip && \
    unzip -q JetBrainsMono-2.304.zip && \
    rm JetBrainsMono-2.304.zip && \
    fc-cache -f -v

# Create workspace directory
WORKDIR /workspace

# Copy ai3-server Python package
COPY ai3-server/ /workspace/ai3-server/

# Install ai3-server
RUN pip3 install --no-cache-dir --upgrade pip && \
    cd /workspace/ai3-server && \
    pip3 install --no-cache-dir -e .

# Create config directories
RUN mkdir -p /root/.config/i3 \
    /root/.config/picom \
    /root/.config/neofetch \
    /root/.config/i3status \
    /root/.local/bin \
    /opt/ai3/scripts

# Add local bin to PATH
ENV PATH="/root/.local/bin:${PATH}"

# Copy ai3-server configs
COPY ai3-server/config/i3config /root/.config/i3/config
COPY ai3-server/config/picom.conf /root/.config/picom/picom.conf
COPY ai3-server/config/Xresources /root/.Xresources
COPY ai3-server/config/ai3.ascii /root/.config/neofetch/ai3.ascii
COPY ai3-server/config/i3status.conf /root/.config/i3status/config

# Copy AI status script
COPY ai3-server/scripts/ai3-status /root/.local/bin/ai3-status
RUN chmod +x /root/.local/bin/ai3-status

# Copy AI wrapper scripts
COPY ai3-server/scripts/ai3-ask.sh /root/.local/bin/ai3-ask.sh
COPY ai3-server/scripts/ai3-suggest.sh /root/.local/bin/ai3-suggest.sh
COPY ai3-server/scripts/ai3-optimize.sh /root/.local/bin/ai3-optimize.sh
COPY ai3-server/scripts/ai3-chat.sh /root/.local/bin/ai3-chat.sh
COPY ai3-server/scripts/ai3-accept.sh /root/.local/bin/ai3-accept.sh
COPY ai3-server/scripts/ai3-discard.sh /root/.local/bin/ai3-discard.sh
COPY ai3-server/scripts/ai3-auto-start.sh /root/.local/bin/ai3-auto-start.sh
COPY ai3-server/scripts/ai3-auto-stop.sh /root/.local/bin/ai3-auto-stop.sh
RUN chmod +x /root/.local/bin/ai3-*.sh

# Create wallpaper directory and generate wallpaper
RUN mkdir -p /root/Pictures
COPY ai3-server/config/wallpaper.svg /root/Pictures/wallpaper.svg

# Convert SVG to PNG using librsvg
RUN apt-get update && apt-get install -y imagemagick librsvg2-bin && \
    rsvg-convert -w 1920 -h 1080 /root/Pictures/wallpaper.svg > /root/Pictures/wallpaper.png && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Copy and apply noVNC ai3 skin
COPY ai3-server/config/novnc/ /opt/ai3-novnc/
RUN cp /opt/ai3-novnc/index.html /usr/share/novnc/index.html && \
    cp /opt/ai3-novnc/ai3-style.css /usr/share/novnc/app/styles/ai3-style.css && \
    cp /opt/ai3-novnc/ai3-ui.css /usr/share/novnc/app/styles/ai3-ui.css && \
    cp /opt/ai3-novnc/ai3-inject.js /usr/share/novnc/app/ai3-inject.js && \
    cp /opt/ai3-novnc/favicon.svg /usr/share/novnc/app/images/icons/ai3-favicon.svg && \
    # Inject ai3 styles and scripts into vnc.html
    sed -i '/<\/head>/i \    <link rel="stylesheet" href="app/styles/ai3-style.css">' /usr/share/novnc/vnc.html && \
    sed -i '/<\/head>/i \    <link rel="stylesheet" href="app/styles/ai3-ui.css">' /usr/share/novnc/vnc.html && \
    sed -i '/<\/body>/i \    <script src="app/ai3-inject.js" defer></script>' /usr/share/novnc/vnc.html && \
    # Update favicon in vnc.html
    sed -i 's|<link rel="icon"[^>]*>|<link rel="icon" type="image/svg+xml" href="app/images/icons/ai3-favicon.svg">|g' /usr/share/novnc/vnc.html && \
    # Update title in vnc.html
    sed -i 's|<title>noVNC</title>|<title>ai3 Desktop</title>|g' /usr/share/novnc/vnc.html

# Set custom distro name for neofetch
RUN echo 'PRETTY_NAME="ai3 (based on Ubuntu 22.04)"' > /etc/os-release && \
    echo 'NAME="ai3"' >> /etc/os-release && \
    echo 'VERSION_ID="2.0"' >> /etc/os-release && \
    echo 'VERSION="2.0 (AI-Powered i3 - Python Edition)"' >> /etc/os-release && \
    echo 'ID=ai3' >> /etc/os-release && \
    echo 'ID_LIKE=ubuntu' >> /etc/os-release && \
    echo 'HOME_URL="https://github.com/i3/i3"' >> /etc/os-release

# Copy entrypoint script
COPY ai3-server/scripts/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Expose ports
# 6080 - noVNC web interface
# 7878 - ai3-server API
EXPOSE 6080 7878

# Environment variables for OpenAI (set at runtime)
# OPENAI_KEY - Required
# OPENAI_MODEL - Optional, defaults to gpt-4.1-mini
# OPENAI_BASE_URL - Optional, for custom endpoints

# Set entrypoint
ENTRYPOINT ["/entrypoint.sh"]
