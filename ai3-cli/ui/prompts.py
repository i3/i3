"""
User prompts for ai3-cli.
"""

from config.constants import Colors
import getpass
from typing import Optional

import sys
from pathlib import Path

# Add parent directory to path for imports
_parent = Path(__file__).parent.parent
if str(_parent) not in sys.path:
    sys.path.insert(0, str(_parent))


def prompt_ai_config() -> dict[str, str]:
    """
    Prompt user for AI configuration (OpenAI credentials).

    Returns:
        Dictionary with AI configuration values.
    """
    _print_config_header()

    config: dict[str, str] = {}

    if not _prompt_enable_ai():
        config['AI_ENABLED'] = 'no'
        return config

    config['AI_ENABLED'] = 'yes'
    config['OPENAI_KEY'] = _prompt_api_key()
    config['OPENAI_MODEL'] = _prompt_model()

    base_url = _prompt_base_url()
    if base_url:
        config['OPENAI_BASE_URL'] = base_url

    _print_validation_result(config)

    return config


def _print_config_header() -> None:
    """Print the configuration section header."""
    print(f"\n{Colors.PURPLE}{'━' * 52}{Colors.RESET}")
    print(f"{Colors.BOLD}🤖 AI Configuration{Colors.RESET}")
    print(f"{Colors.COMMENT}Configure OpenAI for AI features (optional){Colors.RESET}")
    print(f"{Colors.PURPLE}{'━' * 52}{Colors.RESET}\n")


def _prompt_enable_ai() -> bool:
    """Prompt user whether to enable AI features."""
    print(f"{Colors.YELLOW}?{Colors.RESET} Enable AI features? (requires OpenAI API key)")
    response = input(
        f"  {Colors.COMMENT}[y/N]:{Colors.RESET} ").strip().lower()

    if response != 'y':
        print(
            f"\n{Colors.COMMENT}AI features disabled. You can configure later.{Colors.RESET}\n")
        return False

    print()
    return True


def _prompt_api_key() -> str:
    """Prompt for OpenAI API key (hidden input)."""
    print(f"{Colors.CYAN}OpenAI API Key{Colors.RESET}")
    print(f"  {Colors.COMMENT}Your API key (input hidden for security){Colors.RESET}")
    print(
        f"  {Colors.COMMENT}Get one at: https://platform.openai.com/api-keys{Colors.RESET}")
    return getpass.getpass(f"  {Colors.PURPLE}→{Colors.RESET} ")


def _prompt_model() -> str:
    """Prompt for OpenAI model selection."""
    default_model = 'gpt-4.1-mini'
    print(f"\n{Colors.CYAN}Model{Colors.RESET}")
    print(
        f"  {Colors.COMMENT}Examples: gpt-4.1-mini, gpt-4o, gpt-4-turbo{Colors.RESET}")
    print(f"  {Colors.COMMENT}Press Enter for default: {default_model}{Colors.RESET}")

    model = input(f"  {Colors.PURPLE}→{Colors.RESET} ").strip()
    return model if model else default_model


def _prompt_base_url() -> Optional[str]:
    """Prompt for optional custom API base URL."""
    print(f"\n{Colors.CYAN}Custom API Base URL (optional){Colors.RESET}")
    print(f"  {Colors.COMMENT}Leave empty for standard OpenAI{Colors.RESET}")
    print(f"  {Colors.COMMENT}Or enter Azure/other endpoint URL{Colors.RESET}")

    base_url = input(f"  {Colors.PURPLE}→{Colors.RESET} ").strip()
    return base_url if base_url else None


def _print_validation_result(config: dict[str, str]) -> None:
    """Print validation result based on config."""
    if not config.get('OPENAI_KEY'):
        print(f"\n{Colors.YELLOW}⚠ Warning:{Colors.RESET} Missing API key")
        print(f"{Colors.COMMENT}AI features may not work properly.{Colors.RESET}\n")
    else:
        print(f"\n{Colors.GREEN}✓{Colors.RESET} AI configuration complete!\n")
