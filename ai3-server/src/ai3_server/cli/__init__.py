"""
CLI module for ai3.

Split into separate command modules for maintainability.
"""

from ai3_server.cli.app import app, main

__all__ = ["app", "main"]
