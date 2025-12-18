"""
UI utilities for ai3-cli: spinners, banners, and output formatting.
"""

from config.constants import (
    Colors,
    BANNER,
    SPINNER_FRAMES,
    SPINNER_DELAY_SECONDS,
)
import os
import sys
import time
import threading
from typing import Optional

import sys
from pathlib import Path

# Add parent directory to path for imports
_parent = Path(__file__).parent.parent
if str(_parent) not in sys.path:
    sys.path.insert(0, str(_parent))


class Spinner:
    """Animated spinner for long-running operations."""

    def __init__(self, message: str, frames: Optional[list[str]] = None):
        """
        Initialize spinner.

        Args:
            message: Message to display next to spinner.
            frames: Animation frames. Defaults to SPINNER_FRAMES.
        """
        self.message = message
        self.frames = frames or SPINNER_FRAMES
        self._current = 0
        self._running = False
        self._thread: Optional[threading.Thread] = None

    def _spin(self) -> None:
        """Spin animation loop."""
        while self._running:
            frame = self.frames[self._current % len(self.frames)]
            sys.stdout.write(
                f'\r{Colors.PURPLE}{frame}{Colors.RESET} {self.message}')
            sys.stdout.flush()
            self._current += 1
            time.sleep(SPINNER_DELAY_SECONDS)

    def start(self) -> None:
        """Start the spinner animation."""
        self._running = True
        self._thread = threading.Thread(target=self._spin)
        self._thread.start()

    def stop(self, success: bool = True) -> None:
        """
        Stop the spinner animation.

        Args:
            success: Whether operation succeeded (affects displayed symbol).
        """
        self._running = False
        if self._thread:
            self._thread.join()

        symbol = f'{Colors.GREEN}✓' if success else f'{Colors.RED}✗'
        sys.stdout.write(f'\r{symbol}{Colors.RESET} {self.message}\n')
        sys.stdout.flush()

    def __enter__(self) -> 'Spinner':
        """Context manager entry."""
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """Context manager exit."""
        self.stop(success=exc_type is None)


def clear_screen() -> None:
    """Clear the terminal screen."""
    os.system('clear' if os.name == 'posix' else 'cls')


def print_banner(animate: bool = True) -> None:
    """
    Print the ai3 ASCII banner.

    Args:
        animate: Whether to animate the banner lines.
    """
    clear_screen()

    lines = BANNER.split('\n')
    for line in lines:
        print(line)
        if animate:
            time.sleep(0.03)
    print()


def print_box(title: str, content: list[str], color: str = Colors.PURPLE) -> None:
    """
    Print a styled box with title and content.

    Args:
        title: Box title.
        content: List of content lines.
        color: Box border color.
    """
    width = 52
    print(f"{color}{'━' * width}{Colors.RESET}")
    print(f"{Colors.BOLD}{title}{Colors.RESET}")
    print(f"{color}{'━' * width}{Colors.RESET}")
    for line in content:
        print(f"   {line}")
    print(f"{color}{'━' * width}{Colors.RESET}")


def print_success(message: str) -> None:
    """Print a success message with green checkmark."""
    print(f"{Colors.GREEN}✓{Colors.RESET} {message}")


def print_error(message: str) -> None:
    """Print an error message with red X."""
    print(f"{Colors.RED}✗{Colors.RESET} {message}")


def print_warning(message: str) -> None:
    """Print a warning message in yellow."""
    print(f"{Colors.YELLOW}⚠ {message}{Colors.RESET}")


def print_info(message: str) -> None:
    """Print an info message in dim style."""
    print(f"{Colors.COMMENT}{message}{Colors.RESET}")
