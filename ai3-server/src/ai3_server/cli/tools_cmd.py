"""
Tool and status commands.
"""

import json as json_lib
from typing import Optional

import typer
from rich.box import ASCII
from rich.table import Table

from ai3_server.cli.utils import check_server, console, create_table


def tools_command(
    execute: Optional[str] = typer.Option(
        None, "--execute", "-e", help="Execute a tool"),
    args: Optional[str] = typer.Option(
        None, "--args", "-a", help="Tool arguments as JSON"),
) -> None:
    """List or execute tools."""
    client = check_server()

    if execute:
        _execute_tool(client, execute, args)
    else:
        _list_tools(client)


def _execute_tool(client, tool_name: str, args: Optional[str]) -> None:
    """Execute a single tool."""
    arguments = json_lib.loads(args) if args else {}
    with console.status(f"Executing {tool_name}..."):
        result = client.execute_tool(tool_name, arguments)

    if result.get("success"):
        console.print("[green]✓ Tool executed successfully[/green]")
        if result.get("data"):
            console.print(json_lib.dumps(result["data"], indent=2))
    else:
        console.print(f"[red]FAIL - Tool failed: {result.get('error')}[/red]")


def _list_tools(client) -> None:
    """List all available tools."""
    tool_list = client.get_tools()

    table = create_table("Available Tools", [
                         ("Name", "cyan"), ("Description", "white")])
    for tool in tool_list:
        func = tool.get("function", {})
        desc = func.get("description", "")[:60]
        table.add_row(func.get("name", ""), desc +
                      "..." if len(desc) == 60 else desc)

    console.print(table)
    console.print(f"\n[dim]Total: {len(tool_list)} tools[/dim]")
    console.print("[dim]Use --execute <name> to execute a tool[/dim]")


def status_command() -> None:
    """Show current i3 status."""
    client = check_server()

    windows = client.get_windows()
    workspaces = client.get_workspaces()
    focused = client.get_focused()

    # Workspaces table
    ws_table = Table(title="Workspaces", box=ASCII)
    ws_table.add_column("Name", style="cyan")
    ws_table.add_column("Output")
    ws_table.add_column("Focused", justify="center")
    ws_table.add_column("Visible", justify="center")

    for ws in workspaces:
        ws_table.add_row(
            ws["name"],
            ws.get("output", ""),
            "*" if ws.get("focused") else "",
            "*" if ws.get("visible") else "",
        )
    console.print(ws_table)

    # Windows table
    win_table = Table(title="Windows", box=ASCII)
    win_table.add_column("ID", style="dim")
    win_table.add_column("Class", style="cyan")
    win_table.add_column("Title")
    win_table.add_column("Workspace")
    win_table.add_column("Focused", justify="center")

    for win in windows[:15]:
        title = (win.get("name") or "")[:40]
        if len(win.get("name") or "") > 40:
            title += "..."
        win_table.add_row(
            str(win.get("id", "")),
            win.get("window_class", ""),
            title,
            win.get("workspace", ""),
            "*" if win.get("focused") else "",
        )
    console.print(win_table)

    if len(windows) > 15:
        console.print(f"[dim]... and {len(windows) - 15} more windows[/dim]")

    if focused:
        console.print(
            f"\n[bold]Focused:[/bold] {focused.get('window_class', '')} - {focused.get('name', '')}"
        )
