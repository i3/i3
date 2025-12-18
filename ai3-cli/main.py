#!/usr/bin/env python3
"""
ai3 - CLI tool for spinning up AI-powered i3 desktop environments

A beautiful, animated CLI for running containerized ai3 (i3 + AI features)
with browser-based remote access via noVNC.
"""

from commands import (
    cmd_start,
    cmd_stop,
    cmd_logs,
    cmd_status,
    cmd_shell,
    cmd_ai,
)
from config.constants import Colors, DEFAULT_CONTAINER_NAME, DEFAULT_PORT, DEFAULT_RESOLUTION, DEFAULT_LOG_LINES
import argparse
import sys
from pathlib import Path

# Add parent directory to path for imports
_script_dir = Path(__file__).parent
if str(_script_dir) not in sys.path:
    sys.path.insert(0, str(_script_dir))


def create_parser() -> argparse.ArgumentParser:
    """Create and configure the argument parser."""
    parser = argparse.ArgumentParser(
        description='ai3 - AI-Powered i3 Desktop Environment',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=_get_help_epilog()
    )

    subparsers = parser.add_subparsers(dest='command', help='Commands')

    _add_start_command(subparsers)
    _add_stop_command(subparsers)
    _add_logs_command(subparsers)
    _add_status_command(subparsers)
    _add_shell_command(subparsers)
    _add_ai_command(subparsers)

    return parser


def _add_start_command(subparsers) -> None:
    """Add the start command to subparsers."""
    parser = subparsers.add_parser('start', help='Start ai3 desktop')
    parser.add_argument(
        '-n', '--name',
        default=DEFAULT_CONTAINER_NAME,
        help=f'Container name (default: {DEFAULT_CONTAINER_NAME})'
    )
    parser.add_argument(
        '-p', '--port',
        type=int,
        default=DEFAULT_PORT,
        help=f'noVNC port (default: {DEFAULT_PORT})'
    )
    parser.add_argument(
        '-r', '--resolution',
        default=DEFAULT_RESOLUTION,
        help=f'Screen resolution (default: {DEFAULT_RESOLUTION})'
    )
    parser.add_argument(
        '-d', '--detach',
        action='store_true',
        help='Run in background'
    )
    parser.add_argument(
        '--no-browser',
        action='store_true',
        help="Don't open browser automatically"
    )
    parser.add_argument(
        '--skip-ai',
        action='store_true',
        help='Skip AI configuration'
    )


def _add_stop_command(subparsers) -> None:
    """Add the stop command to subparsers."""
    parser = subparsers.add_parser('stop', help='Stop ai3 desktop')
    parser.add_argument(
        'name',
        nargs='?',
        default=DEFAULT_CONTAINER_NAME,
        help=f'Container name (default: {DEFAULT_CONTAINER_NAME})'
    )
    parser.add_argument(
        '-a', '--all',
        action='store_true',
        help='Stop all ai3 containers'
    )


def _add_logs_command(subparsers) -> None:
    """Add the logs command to subparsers."""
    parser = subparsers.add_parser('logs', help='View container logs')
    parser.add_argument(
        'name',
        nargs='?',
        default=DEFAULT_CONTAINER_NAME,
        help='Container name'
    )
    parser.add_argument(
        '-f', '--follow',
        action='store_true',
        help='Follow log output'
    )
    parser.add_argument(
        '-n', '--lines',
        type=int,
        default=DEFAULT_LOG_LINES,
        help='Number of lines to show'
    )


def _add_status_command(subparsers) -> None:
    """Add the status command to subparsers."""
    subparsers.add_parser('status', help='Show running containers')


def _add_shell_command(subparsers) -> None:
    """Add the shell command to subparsers."""
    parser = subparsers.add_parser('shell', help='Open shell in container')
    parser.add_argument(
        'name',
        nargs='?',
        default=DEFAULT_CONTAINER_NAME,
        help='Container name'
    )


def _add_ai_command(subparsers) -> None:
    """Add the ai command to subparsers."""
    parser = subparsers.add_parser('ai', help='Run AI commands in container')
    parser.add_argument(
        'name',
        nargs='?',
        default=DEFAULT_CONTAINER_NAME,
        help='Container name'
    )
    parser.add_argument(
        'ai_args',
        nargs='*',
        default=[],
        help='Arguments to pass to ai3'
    )


def _get_help_epilog() -> str:
    """Get the help epilog with examples."""
    return f"""
{Colors.PURPLE}Examples:{Colors.RESET}
  {Colors.CYAN}ai3 start{Colors.RESET}                    Start with default settings
  {Colors.CYAN}ai3 start -p 8080{Colors.RESET}            Start on port 8080
  {Colors.CYAN}ai3 start -r 2560x1440{Colors.RESET}       Start with custom resolution
  {Colors.CYAN}ai3 start --skip-ai{Colors.RESET}          Start without AI features
  {Colors.CYAN}ai3 stop{Colors.RESET}                     Stop the running container
  {Colors.CYAN}ai3 logs -f{Colors.RESET}                  Follow container logs
  {Colors.CYAN}ai3 shell{Colors.RESET}                    Open shell in container
  {Colors.CYAN}ai3 ai suggest{Colors.RESET}               Run AI suggest command

{Colors.COMMENT}Environment variables (or .env file):{Colors.RESET}
  {Colors.YELLOW}OPENAI_KEY{Colors.RESET}         Your OpenAI API key
  {Colors.YELLOW}OPENAI_MODEL{Colors.RESET}       Model to use (default: gpt-4.1-mini)
  {Colors.YELLOW}OPENAI_BASE_URL{Colors.RESET}    Custom API endpoint (optional)
"""


def main() -> int:
    """Main entry point."""
    parser = create_parser()
    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        return 0

    command_handlers = {
        'start': cmd_start,
        'stop': cmd_stop,
        'logs': cmd_logs,
        'status': cmd_status,
        'shell': cmd_shell,
        'ai': cmd_ai,
    }

    handler = command_handlers.get(args.command)
    if handler:
        return handler(args)

    return 1


if __name__ == '__main__':
    sys.exit(main())
