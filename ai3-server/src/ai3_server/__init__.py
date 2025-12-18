"""
ai3-server: AI-powered i3wm assistant with tools and OpenAI agents.
"""

__version__ = "1.0.0"

from ai3_server.config import AI3Config
from ai3_server.client import AI3Client

__all__ = ["AI3Config", "AI3Client", "__version__"]
