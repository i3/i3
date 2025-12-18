"""
Server management commands.
"""

import subprocess
import sys
import time

import typer
from rich.table import Table
from rich.box import ASCII

from ai3_server.cli.utils import console, get_client

server_app = typer.Typer(help="Server management commands")


def _start_background_server(client) -> bool:
    """Start server in background and wait for it."""
    console.print("Starting ai3-server in background...")
    subprocess.Popen(
        [sys.executable, "-m", "ai3_server.server"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
    )
    for _ in range(10):
        time.sleep(0.5)
        if client.is_running():
            return True
    return False


@server_app.callback(invoke_without_command=True)
def server_command(
    ctx: typer.Context,
    action: str = typer.Argument(...,
                                 help="Action: start, stop, status, restart"),
    background: bool = typer.Option(
        True, "--background/--foreground", "-b/-f", help="Run in background"
    ),
):
    """Manage the ai3 server."""
    if action == "start":
        client = get_client()
        if client.is_running():
            console.print("[green]Server is already running.[/green]")
            return

        if background:
            if _start_background_server(client):
                console.print("[green]Server started successfully.[/green]")
            else:
                console.print("[red]Server failed to start.[/red]")
        else:
            from ai3_server.server import main
            main()

    elif action == "stop":
        console.print(
            "[yellow]To stop the server, kill the process manually.[/yellow]")
        subprocess.run(["pkill", "-f", "ai3_server.server"],
                       capture_output=True)

    elif action == "status":
        _show_status()

    elif action == "restart":
        subprocess.run(["pkill", "-f", "ai3_server.server"],
                       capture_output=True)
        time.sleep(1)
        client = get_client()
        if _start_background_server(client):
            console.print("[green]Server restarted.[/green]")
        else:
            console.print("[red]Server failed to restart.[/red]")

    else:
        console.print(f"[red]Unknown action: {action}[/red]")
        raise typer.Exit(1)


def _show_status():
    """Show server status."""
    client = get_client()
    try:
        status = client.status()
        table = Table(title="ai3-server Status", box=ASCII)
        table.add_column("Property", style="cyan")
        table.add_column("Value", style="green")

        table.add_row("Running", "Yes" if status.running else "No")
        table.add_row("Version", status.version)
        table.add_row("Uptime", f"{status.uptime_seconds:.1f}s")
        table.add_row("i3 Connected", "Yes" if status.i3_connected else "No")
        table.add_row("OpenAI Configured",
                      "Yes" if status.openai_configured else "No")
        table.add_row("Total Requests", str(status.total_requests))
        table.add_row("Total Tokens", str(status.total_tokens))

        console.print(table)

        if status.agents:
            agent_table = Table(title="Agents", box=ASCII)
            agent_table.add_column("Name")
            agent_table.add_column("Runs")
            agent_table.add_column("Tokens")
            agent_table.add_column("Errors")

            for agent in status.agents:
                agent_table.add_row(
                    agent["name"],
                    str(agent["total_runs"]),
                    str(agent["total_tokens"]),
                    str(agent["errors"]),
                )
            console.print(agent_table)

    except ConnectionError:
        console.print("[red]Server is not running.[/red]")
