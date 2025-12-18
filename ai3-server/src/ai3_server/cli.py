"""
CLI interface for ai3.

This module re-exports the CLI from the cli package for backwards compatibility.
The actual implementation is in the cli/ subpackage.
"""

from ai3_server.cli.app import app, main

__all__ = ["app", "main"]
