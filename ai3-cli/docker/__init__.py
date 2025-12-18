"""
Docker operations for ai3-cli.
"""

from ui import Spinner
from config.constants import (
    DEFAULT_RESOLUTION,
    DEFAULT_SCREEN_DEPTH,
    NOVNC_TIMEOUT_SECONDS,
    I3_STARTUP_TIMEOUT_SECONDS,
)
import os
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Optional

import sys
from pathlib import Path

# Add parent directory to path for imports
_parent = Path(__file__).parent.parent
if str(_parent) not in sys.path:
    sys.path.insert(0, str(_parent))


def check_docker_available() -> bool:
    """Check if Docker is available and running."""
    success, _ = run_command(['docker', 'info'])
    return success


def run_command(cmd: list[str], capture: bool = True) -> tuple[bool, str]:
    """
    Run a shell command and return result.

    Args:
        cmd: Command and arguments as list.
        capture: Whether to capture output.

    Returns:
        Tuple of (success, output).
    """
    try:
        if capture:
            result = subprocess.run(cmd, capture_output=True, text=True)
            return result.returncode == 0, result.stdout + result.stderr
        else:
            result = subprocess.run(cmd)
            return result.returncode == 0, ""
    except Exception as e:
        return False, str(e)


def build_container(script_dir: Path) -> bool:
    """
    Build the ai3 Docker container.

    Args:
        script_dir: Directory containing the script.

    Returns:
        True if build succeeded.
    """
    spinner = Spinner("Building ai3 container image...")
    spinner.start()

    workspace_dir = script_dir.parent
    ai3_server_dir = workspace_dir / "ai3-server"
    dockerfile_path = ai3_server_dir / "Dockerfile"

    success, output = run_command([
        'docker', 'build',
        '-t', 'ai3-desktop',
        '-f', str(dockerfile_path),
        str(workspace_dir)
    ])

    spinner.stop(success)

    if not success:
        from ui import print_error
        print_error(f"Build failed:\n{output}")

    return success


def start_container(
    name: str,
    port: int,
    resolution: str,
    script_dir: Path,
    ai_config: Optional[dict[str, str]] = None
) -> bool:
    """
    Start the ai3 container.

    Args:
        name: Container name.
        port: noVNC port.
        resolution: Screen resolution (WIDTHxHEIGHT).
        script_dir: Script directory for context.
        ai_config: Optional AI configuration.

    Returns:
        True if container started successfully.
    """
    width, height = _parse_resolution(resolution)

    # Remove existing container if exists
    run_command(['docker', 'rm', '-f', name])

    spinner = Spinner("Starting ai3 container...")
    spinner.start()

    secrets_file = _create_secrets_file(ai_config)
    cmd = _build_docker_run_command(name, port, width, height, secrets_file)

    success, output = run_command(cmd)
    spinner.stop(success)

    _cleanup_secrets_file(secrets_file)

    if not success:
        from ui import print_error
        print_error(f"Failed to start container:\n{output}")

    return success


def stop_container(name: str) -> bool:
    """
    Stop and remove a container.

    Args:
        name: Container name.

    Returns:
        True if stopped successfully.
    """
    spinner = Spinner(f"Stopping {name}...")
    spinner.start()

    run_command(['docker', 'stop', name])
    success, _ = run_command(['docker', 'rm', name])

    spinner.stop(success)
    return success


def get_container_logs(name: str, lines: int = 50) -> str:
    """
    Get logs from a container.

    Args:
        name: Container name.
        lines: Number of log lines to retrieve.

    Returns:
        Log output string.
    """
    success, output = run_command(
        ['docker', 'logs', '--tail', str(lines), name])
    return output if success else f"Failed to get logs: {output}"


def list_running_containers() -> str:
    """List running ai3 containers."""
    success, output = run_command([
        'docker', 'ps',
        '--filter', 'ancestor=ai3-desktop',
        '--format', 'table {{.Names}}\t{{.Status}}\t{{.Ports}}'
    ])
    return output if success else "No containers found"


def wait_for_novnc(port: int, timeout: int = NOVNC_TIMEOUT_SECONDS) -> bool:
    """
    Wait for noVNC to be ready.

    Args:
        port: noVNC port.
        timeout: Maximum wait time in seconds.

    Returns:
        True if noVNC is ready.
    """
    import socket

    spinner = Spinner("Waiting for noVNC to be ready...")
    spinner.start()

    start_time = time.time()
    while time.time() - start_time < timeout:
        if _check_port_open('localhost', port):
            spinner.stop(True)
            return True
        time.sleep(0.5)

    spinner.stop(False)
    return False


def wait_for_i3(name: str, timeout: int = I3_STARTUP_TIMEOUT_SECONDS) -> bool:
    """
    Wait for i3 to be running inside the container.

    Args:
        name: Container name.
        timeout: Maximum wait time in seconds.

    Returns:
        True if i3 is running.
    """
    spinner = Spinner("Waiting for ai3 window manager...")
    spinner.start()

    start_time = time.time()
    while time.time() - start_time < timeout:
        success, _ = run_command(['docker', 'exec', name, 'pgrep', '-x', 'i3'])
        if success:
            spinner.stop(True)
            return True
        time.sleep(1)

    spinner.stop(False)
    return False


# Private helper functions

def _parse_resolution(resolution: str) -> tuple[str, str]:
    """Parse resolution string into width and height."""
    try:
        width, height = resolution.split('x')
        return width, height
    except ValueError:
        default_width, default_height = DEFAULT_RESOLUTION.split('x')
        return default_width, default_height


def _create_secrets_file(ai_config: Optional[dict[str, str]]) -> Optional[tempfile.NamedTemporaryFile]:
    """Create temporary secrets file for AI config."""
    if not ai_config:
        return None

    secrets_file = tempfile.NamedTemporaryFile(
        mode='w',
        prefix='ai3_secrets_',
        suffix='.env',
        delete=False
    )

    for key, value in ai_config.items():
        secrets_file.write(f'export {key}="{value}"\n')
    secrets_file.close()

    os.chmod(secrets_file.name, 0o600)
    return secrets_file


def _cleanup_secrets_file(secrets_file: Optional[tempfile.NamedTemporaryFile]) -> None:
    """Clean up the temporary secrets file."""
    if not secrets_file:
        return

    try:
        time.sleep(1)  # Give container time to read the file
        os.unlink(secrets_file.name)
    except OSError:
        pass


def _build_docker_run_command(
    name: str,
    port: int,
    width: str,
    height: str,
    secrets_file: Optional[tempfile.NamedTemporaryFile]
) -> list[str]:
    """Build the docker run command."""
    cmd = [
        'docker', 'run', '-d',
        '--name', name,
        '-p', f'{port}:6080',
        '-e', f'SCREEN_WIDTH={width}',
        '-e', f'SCREEN_HEIGHT={height}',
        '-e', f'SCREEN_DEPTH={DEFAULT_SCREEN_DEPTH}',
    ]

    if secrets_file:
        cmd.extend(['-v', f'{secrets_file.name}:/tmp/.ai3_secrets:ro'])

    cmd.append('ai3-desktop')
    return cmd


def _check_port_open(host: str, port: int) -> bool:
    """Check if a port is open and accepting connections."""
    import socket

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(1)
        result = sock.connect_ex((host, port))
        sock.close()
        return result == 0
    except (socket.error, OSError):
        return False
