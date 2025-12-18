"""
Shared CLI utilities and helpers.
"""

from typing import TYPE_CHECKING, Optional

import typer
from rich.box import ASCII
from rich.console import Console
from rich.panel import Panel
from rich.table import Table
from rich.text import Text

if TYPE_CHECKING:
    from ai3_server.client import AI3Client
    from ai3_server.models import Suggestion

# Global console instance
console = Console()


def get_client() -> "AI3Client":
    """Get ai3 client instance."""
    from ai3_server.client import AI3Client
    return AI3Client()


def check_server() -> "AI3Client":
    """
    Check if server is running and return client.

    Raises:
        typer.Exit: If server is not running.

    Returns:
        Connected AI3Client instance.
    """
    client = get_client()
    if not client.is_running():
        print_server_not_running()
        raise typer.Exit(1)
    return client


def print_server_not_running() -> None:
    """Print server not running message."""
    console.print(
        "[yellow]ai3-server is not running.[/yellow]\n"
        "Start it with: [bold]ai3 server start[/bold]"
    )


def create_table(title: str, columns: list[tuple[str, str]]) -> Table:
    """
    Create a styled table with columns.

    Args:
        title: Table title.
        columns: List of (column_name, style) tuples.

    Returns:
        Configured Rich Table.
    """
    table = Table(title=title, box=ASCII)
    for name, style in columns:
        table.add_column(name, style=style)
    return table


def print_panel(content: str, title: str, style: str = "bold blue") -> None:
    """
    Print content in a styled panel.

    Args:
        content: Panel content.
        title: Panel title.
        style: Rich style for the title.
    """
    console.print(
        Panel(content, title=f"[{style}]{title}[/{style}]", box=ASCII)
    )


def print_response_panel(
    message: str,
    tool_calls: Optional[list[dict]] = None,
    title: str = "AI Response"
) -> None:
    """
    Print AI response with optional tool summary.

    Args:
        message: Response message to display.
        tool_calls: Optional list of tool call results.
        title: Panel title.
    """
    console.print(
        Panel(message, title=f"[bold green]{title}[/bold green]", box=ASCII)
    )
    if tool_calls:
        print_tool_summary(tool_calls)


def print_tool_summary(tool_calls: list[dict]) -> None:
    """
    Print a summary of tool calls.

    Args:
        tool_calls: List of tool call results with 'tool' and 'success' keys.
    """
    tool_summary = ", ".join(
        f"{tc['tool']}({'OK' if tc['success'] else 'FAIL'})"
        for tc in tool_calls
    )
    console.print(f"[dim]Tools used: {tool_summary}[/dim]")


def format_suggestion(suggestion: "Suggestion") -> Text:
    """
    Format a suggestion for Rich display.

    Args:
        suggestion: Suggestion object to format.

    Returns:
        Rich Text object with formatted suggestion.
    """
    content = Text()

    _append_field(content, "Action", suggestion.action_type.value, "cyan")
    _append_field(content, "Description", suggestion.description, "white")

    if suggestion.keys:
        _append_field(content, "Keys", suggestion.keys, "green")

    if suggestion.command:
        _append_field(content, "Command", suggestion.command, "green")

    _append_field(content, "Confidence",
                  f"{suggestion.confidence:.0%}", "yellow")

    if suggestion.reason:
        _append_field(content, "Reason", suggestion.reason,
                      "dim", newline=False)

    return content


def _append_field(
    text: Text,
    label: str,
    value: str,
    value_style: str,
    newline: bool = True
) -> None:
    """
    Append a labeled field to Rich Text.

    Args:
        text: Rich Text object to append to.
        label: Field label.
        value: Field value.
        value_style: Rich style for the value.
        newline: Whether to add newline after value.
    """
    text.append(f"{label}: ", style="bold")
    suffix = "\n" if newline else ""
    text.append(f"{value}{suffix}", style=value_style)
