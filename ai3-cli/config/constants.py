"""
Constants and styling for ai3-cli.
"""


class Colors:
    """ANSI color codes using Dracula theme."""

    RESET = '\033[0m'
    BOLD = '\033[1m'
    DIM = '\033[2m'

    # Dracula theme colors
    BG = '\033[48;2;40;42;54m'
    FG = '\033[38;2;248;248;242m'
    CYAN = '\033[38;2;139;233;253m'
    GREEN = '\033[38;2;80;250;123m'
    ORANGE = '\033[38;2;255;184;108m'
    PINK = '\033[38;2;255;121;198m'
    PURPLE = '\033[38;2;189;147;249m'
    RED = '\033[38;2;255;85;85m'
    YELLOW = '\033[38;2;241;250;140m'
    COMMENT = '\033[38;2;98;114;164m'


# Spinner frames for loading animation
SPINNER_FRAMES = ['⠋', '⠙', '⠹', '⠸', '⠼', '⠴', '⠦', '⠧', '⠇', '⠏']
SPINNER_FRAMES_DOTS = ['⣾', '⣽', '⣻', '⢿', '⡿', '⣟', '⣯', '⣷']

# Default configuration values
DEFAULT_CONTAINER_NAME = "ai3-desktop"
DEFAULT_PORT = 6080
DEFAULT_RESOLUTION = "1920x1080"
DEFAULT_SCREEN_DEPTH = 24
DEFAULT_LOG_LINES = 50
NOVNC_TIMEOUT_SECONDS = 60
I3_STARTUP_TIMEOUT_SECONDS = 60
SPINNER_DELAY_SECONDS = 0.08

# ASCII art banner
BANNER = f"""{Colors.PURPLE}
    █████╗ ██╗██████╗
   ██╔══██╗██║╚════██╗
   ███████║██║ █████╔╝
   ██╔══██║██║ ╚═══██╗
   ██║  ██║██║██████╔╝
   ╚═╝  ╚═╝╚═╝╚═════╝ {Colors.RESET}

   {Colors.CYAN}AI-Powered i3 Window Manager{Colors.RESET}
   {Colors.COMMENT}Fast • Beautiful • Intelligent{Colors.RESET}
"""

SMALL_BANNER = f"""{Colors.PURPLE}█▀█ █ █▀█{Colors.RESET} {Colors.CYAN}ai3{Colors.RESET}"""


# Keyboard shortcuts help text
KEYBOARD_SHORTCUTS = [
    (f"{Colors.YELLOW}Ctrl+G{Colors.RESET}", "Enter AI mode"),
    (f"{Colors.YELLOW}Ctrl+Shift+S{Colors.RESET}", "AI Suggestion"),
    (f"{Colors.YELLOW}Ctrl+Shift+O{Colors.RESET}", "Optimize Layout"),
    (f"{Colors.YELLOW}Ctrl+Shift+T{Colors.RESET}", "AI Chat"),
]
