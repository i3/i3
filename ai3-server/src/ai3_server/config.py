"""
Configuration management for ai3-server.

Provides centralized configuration with environment variable support.
"""

import os
from dataclasses import dataclass, field
from typing import Optional


# Default values
DEFAULT_MODEL = "gpt-4.1-mini"
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 7878
DEFAULT_MAX_TOKENS = 2048
DEFAULT_TEMPERATURE = 0.7


@dataclass
class AI3Config:
    """
    Configuration for ai3-server.

    All settings can be configured via environment variables:
    - OPENAI_KEY: OpenAI API key (required for AI features)
    - OPENAI_MODEL: Model to use (default: gpt-4.1-mini)
    - OPENAI_BASE_URL: Custom API endpoint (optional)
    - AI3_SERVER_HOST: Server host (default: 127.0.0.1)
    - AI3_SERVER_PORT: Server port (default: 7878)
    - AI3_MAX_TOKENS: Max tokens per request (default: 2048)
    - AI3_TEMPERATURE: Model temperature (default: 0.7)
    - I3SOCK: i3 socket path (optional, auto-detected)
    """

    # OpenAI settings
    openai_api_key: str = field(
        default_factory=lambda: os.getenv("OPENAI_KEY", "")
    )
    openai_model: str = field(
        default_factory=lambda: os.getenv("OPENAI_MODEL", DEFAULT_MODEL)
    )
    openai_base_url: Optional[str] = field(
        default_factory=lambda: os.getenv("OPENAI_BASE_URL")
    )

    # Server settings
    server_host: str = field(
        default_factory=lambda: os.getenv("AI3_SERVER_HOST", DEFAULT_HOST)
    )
    server_port: int = field(
        default_factory=lambda: int(
            os.getenv("AI3_SERVER_PORT", str(DEFAULT_PORT)))
    )

    # Feature settings
    max_tokens: int = field(
        default_factory=lambda: int(
            os.getenv("AI3_MAX_TOKENS", str(DEFAULT_MAX_TOKENS)))
    )
    temperature: float = field(
        default_factory=lambda: float(
            os.getenv("AI3_TEMPERATURE", str(DEFAULT_TEMPERATURE)))
    )

    # i3 settings
    i3_socket_path: Optional[str] = field(
        default_factory=lambda: os.getenv("I3SOCK")
    )

    def validate(self) -> bool:
        """
        Validate configuration.

        Raises:
            ValueError: If required configuration is missing.

        Returns:
            True if configuration is valid.
        """
        if not self.openai_api_key:
            raise ValueError("OPENAI_KEY environment variable is required")
        return True

    @property
    def server_url(self) -> str:
        """Get server URL."""
        return f"http://{self.server_host}:{self.server_port}"

    @property
    def is_openai_configured(self) -> bool:
        """Check if OpenAI is configured."""
        return bool(self.openai_api_key)


# Global configuration instance
_config: Optional[AI3Config] = None


def get_config() -> AI3Config:
    """Get or create global configuration."""
    global _config
    if _config is None:
        _config = AI3Config()
    return _config


def set_config(config: AI3Config) -> None:
    """Set global configuration."""
    global _config
    _config = config


def reset_config() -> None:
    """Reset global configuration to default."""
    global _config
    _config = None
