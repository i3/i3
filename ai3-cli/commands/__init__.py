"""
Command handlers for ai3-cli.
"""

from ui.prompts import prompt_ai_config
from ui import (
    Spinner,
    print_banner,
    print_box,
    print_success,
    print_error,
    print_warning,
    print_info,
)
from docker import (
    check_docker_available,
    build_container,
    start_container,
    stop_container,
    get_container_logs,
    list_running_containers,
    wait_for_novnc,
    wait_for_i3,
    run_command,
)
from config.env import get_ai_config_from_env
from config.constants import (
    Colors,
    SMALL_BANNER,
    KEYBOARD_SHORTCUTS,
    DEFAULT_LOG_LINES,
)
import signal
import subprocess
import sys
import time
import webbrowser
from pathlib import Path
from typing import Any

# Add parent directory to path for imports
_parent = Path(__file__).parent.parent
if str(_parent) not in sys.path:
    sys.path.insert(0, str(_parent))


def cmd_start(args: Any) -> int:
    """Handle the start command."""
    print_banner()

    if not _check_prerequisites():
        return 1

    script_dir = get_script_dir()

    if not build_container(script_dir):
        return 1

    ai_config, ai_enabled = _get_ai_configuration(args)

    if not start_container(args.name, args.port, args.resolution, script_dir, ai_config):
        return 1

    if not wait_for_i3(args.name):
        print_warning("i3 may not have started properly")

    if not wait_for_novnc(args.port):
        print_error("noVNC failed to start")
        return 1

    url = _show_ready_message(args.port, ai_enabled)

    if not args.no_browser:
        _open_browser(url)

    if not args.detach:
        _run_foreground_mode(args.name)
    else:
        print_info(f"Container running in background.")
        print_info(
            f"Use '{Colors.CYAN}ai3 stop {args.name}{Colors.COMMENT}' to stop.")

    return 0


def cmd_stop(args: Any) -> int:
    """Handle the stop command."""
    _print_small_banner()

    if args.all:
        return _stop_all_containers()

    if stop_container(args.name):
        print_success(f"Container '{args.name}' stopped")
    else:
        print_error(f"Failed to stop container '{args.name}'")

    return 0


def cmd_logs(args: Any) -> int:
    """Handle the logs command."""
    _print_small_banner()

    if args.follow:
        try:
            subprocess.run(['docker', 'logs', '-f', args.name])
        except KeyboardInterrupt:
            print()
    else:
        lines = getattr(args, 'lines', DEFAULT_LOG_LINES)
        output = get_container_logs(args.name, lines)
        print(output)

    return 0


def cmd_status(args: Any) -> int:
    """Handle the status command."""
    _print_small_banner()

    print(f"{Colors.BOLD}Running ai3 Containers:{Colors.RESET}")
    print(list_running_containers())
    print()

    return 0


def cmd_shell(args: Any) -> int:
    """Handle the shell command."""
    print_info(f"Opening shell in {args.name}...")
    subprocess.run(['docker', 'exec', '-it', args.name, 'bash'])
    return 0


def cmd_ai(args: Any) -> int:
    """Handle the ai command (run ai3 CLI inside container)."""
    cmd = ['docker', 'exec', '-it', args.name, 'ai3'] + args.ai_args
    subprocess.run(cmd)
    return 0


# Private helper functions

def get_script_dir() -> Path:
    """Get the directory where this script is located."""
    return Path(__file__).parent.parent.resolve()


def _check_prerequisites() -> bool:
    """Check all prerequisites for starting."""
    spinner = Spinner("Checking Docker...")
    spinner.start()

    if not check_docker_available():
        spinner.stop(False)
        print_error(
            "Docker is not available. Please install Docker and try again.")
        return False

    spinner.stop(True)
    return True


def _get_ai_configuration(args: Any) -> tuple[dict[str, str] | None, bool]:
    """Get AI configuration from environment or user prompt."""
    if args.skip_ai:
        print_info("AI configuration skipped (--skip-ai)")
        return None, False

    # Try to load from .env file first
    ai_config = get_ai_config_from_env()
    if ai_config:
        print_success(
            f"AI configuration loaded from {Colors.CYAN}.env{Colors.RESET} file")
        print_info(f"Model: {ai_config.get('OPENAI_MODEL', 'gpt-4.1-mini')}")
        return ai_config, True

    # No .env file or missing credentials, prompt user
    ai_config = prompt_ai_config()
    ai_enabled = ai_config.get('AI_ENABLED') == 'yes'
    return ai_config, ai_enabled


def _show_ready_message(port: int, ai_enabled: bool) -> str:
    """Display the ready message with URL."""
    url = f"http://localhost:{port}/vnc.html?autoconnect=true&resize=remote"
    ai_status = f"{Colors.GREEN}✓ Enabled{Colors.RESET}" if ai_enabled else f"{Colors.COMMENT}Disabled{Colors.RESET}"

    content = [
        "",
        f"{Colors.BOLD}🌐 Open in browser:{Colors.RESET}",
        f"   {Colors.CYAN}{Colors.BOLD}{url}{Colors.RESET}",
        "",
        f"{Colors.BOLD}🤖 AI Features:{Colors.RESET} {ai_status}",
        "",
        f"{Colors.COMMENT}Keyboard Shortcuts:{Colors.RESET}",
    ]

    for key, desc in KEYBOARD_SHORTCUTS:
        content.append(f"   {key} {desc}")

    content.append("")

    print()
    print_box(
        f"🖥️  {Colors.GREEN}ai3 Desktop Ready!{Colors.RESET}", content, Colors.GREEN)

    return url


def _open_browser(url: str) -> None:
    """Open URL in browser."""
    time.sleep(1)
    spinner = Spinner("Opening browser...")
    spinner.start()

    try:
        webbrowser.open(url)
        spinner.stop(True)
    except (webbrowser.Error, OSError):
        spinner.stop(False)


def _run_foreground_mode(name: str) -> None:
    """Run in foreground mode with signal handling."""
    print_info("Press Ctrl+C to stop the environment")

    def signal_handler(sig, frame):
        print(f"\n\n{Colors.YELLOW}Shutting down...{Colors.RESET}")
        stop_container(name)
        print(f"{Colors.GREEN}Goodbye! 👋{Colors.RESET}")
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        signal_handler(None, None)


def _stop_all_containers() -> int:
    """Stop all ai3 containers."""
    success, output = run_command([
        'docker', 'ps', '-q',
        '--filter', 'ancestor=ai3-desktop'
    ])

    if success and output.strip():
        containers = output.strip().split('\n')
        for container in containers:
            run_command(['docker', 'stop', container])
            run_command(['docker', 'rm', container])
        print_success(f"Stopped {len(containers)} container(s)")
    else:
        print_info("No ai3 containers running")

    return 0


def _print_small_banner() -> None:
    """Print small banner."""
    print(SMALL_BANNER)
    print()
