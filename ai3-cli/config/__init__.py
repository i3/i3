"""
Configuration module for ai3-cli.
"""

from config.constants import (
    Colors,
    BANNER,
    SMALL_BANNER,
    SPINNER_FRAMES,
    SPINNER_FRAMES_DOTS,
    DEFAULT_CONTAINER_NAME,
    DEFAULT_PORT,
    DEFAULT_RESOLUTION,
)
from config.env import load_env_file, get_ai_config_from_env

__all__ = [
    "Colors",
    "BANNER",
    "SMALL_BANNER",
    "SPINNER_FRAMES",
    "SPINNER_FRAMES_DOTS",
    "DEFAULT_CONTAINER_NAME",
    "DEFAULT_PORT",
    "DEFAULT_RESOLUTION",
    "load_env_file",
    "get_ai_config_from_env",
]
