"""
Environment configuration loading for ai3-cli.
"""

from pathlib import Path
from typing import Optional


def load_env_file(env_path: Optional[Path] = None) -> dict[str, str]:
    """
    Load environment variables from .env file.

    Args:
        env_path: Path to .env file. If None, uses parent folder of script.

    Returns:
        Dictionary of environment variables.
    """
    if env_path is None:
        script_dir = Path(__file__).parent.parent.resolve()
        env_path = script_dir.parent / '.env'

    env_vars: dict[str, str] = {}

    if not env_path.exists():
        return env_vars

    try:
        with open(env_path, 'r') as f:
            for line in f:
                line = line.strip()
                if _should_skip_line(line):
                    continue

                key, value = _parse_env_line(line)
                if key:
                    env_vars[key] = value
    except (OSError, IOError):
        pass

    return env_vars


def _should_skip_line(line: str) -> bool:
    """Check if line should be skipped (empty or comment)."""
    return not line or line.startswith('#')


def _parse_env_line(line: str) -> tuple[Optional[str], str]:
    """
    Parse a single line from .env file.

    Returns:
        Tuple of (key, value) or (None, '') if invalid.
    """
    if '=' not in line:
        return None, ''

    key, value = line.split('=', 1)
    key = key.strip()
    value = value.strip()

    # Remove quotes if present
    if value and value[0] in ('"', "'") and value[-1] == value[0]:
        value = value[1:-1]

    return key, value


def get_ai_config_from_env(env_path: Optional[Path] = None) -> Optional[dict[str, str]]:
    """
    Get AI configuration from .env file if available.

    Args:
        env_path: Optional path to .env file.

    Returns:
        Dictionary with AI config or None if not configured.
    """
    env_vars = load_env_file(env_path)

    if not env_vars.get('OPENAI_KEY'):
        return None

    config = {
        'AI_ENABLED': 'yes',
        'OPENAI_KEY': env_vars['OPENAI_KEY'],
        'OPENAI_MODEL': env_vars.get('OPENAI_MODEL', 'gpt-4.1-mini'),
    }

    # Add optional base URL if present (for Azure OpenAI or other providers)
    if 'OPENAI_BASE_URL' in env_vars:
        config['OPENAI_BASE_URL'] = env_vars['OPENAI_BASE_URL']

    return config
