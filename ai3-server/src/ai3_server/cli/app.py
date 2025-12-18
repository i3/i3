"""
Main CLI application.
"""

import typer

from ai3_server.cli.ai_cmd import (
    ask_command,
    chat_command,
    layout_command,
    suggest_command,
)
from ai3_server.cli.server_cmd import server_command
from ai3_server.cli.tools_cmd import status_command, tools_command

app = typer.Typer(
    name="ai3",
    help="AI-powered i3wm assistant",
    no_args_is_help=True,
)

# Register commands directly
app.command("server")(server_command)
app.command("suggest")(suggest_command)
app.command("layout")(layout_command)
app.command("chat")(chat_command)
app.command("ask")(ask_command)
app.command("tools")(tools_command)
app.command("status")(status_command)


def main():
    """Main entry point."""
    app()


if __name__ == "__main__":
    main()
