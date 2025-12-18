"""
AI-related commands: suggest, layout, chat, ask.

These commands interact with the AI agents to provide intelligent
assistance for i3 window management.
"""

from typing import Optional

import typer
from rich.box import ASCII
from rich.panel import Panel
from rich.text import Text

from ai3_server.cli.utils import (
    check_server,
    console,
    format_suggestion,
    print_response_panel,
    print_tool_summary,
)


def suggest_command(
    accept: bool = typer.Option(
        False, "--accept", "-a", help="Accept the suggestion"
    ),
) -> None:
    """Get AI-powered next action suggestion."""
    client = check_server()

    with console.status("Analyzing window state..."):
        suggestion = client.suggest()

    if not suggestion:
        console.print("[yellow]No suggestion available.[/yellow]")
        return

    panel_content = format_suggestion(suggestion)
    console.print(
        Panel(panel_content,
              title="[bold blue]AI Suggestion[/bold blue]", box=ASCII)
    )

    if accept:
        _execute_suggestion(client, suggestion)
    else:
        console.print("\n[dim]Use --accept to execute this suggestion.[/dim]")


def _execute_suggestion(client, suggestion) -> None:
    """Execute a suggestion and show result."""
    success = client.accept_suggestion(suggestion)
    if success:
        console.print("[green]OK - Suggestion executed.[/green]")
    else:
        console.print("[red]FAILED - Could not execute suggestion.[/red]")


def layout_command(
    goal: Optional[str] = typer.Argument(None, help="Optimization goal"),
) -> None:
    """Optimize window layout with AI."""
    client = check_server()

    goal_text = goal or "general optimization"
    with console.status(f"Optimizing layout: {goal_text}..."):
        result = client.optimize_layout(goal=goal)

    if result.success:
        _display_layout_result(result)
    else:
        console.print(
            f"[red]Layout optimization failed: {result.description}[/red]")


def _display_layout_result(result) -> None:
    """Display layout optimization result."""
    panel_content = Text()
    panel_content.append("Changes made: ", style="bold")
    panel_content.append(f"{result.changes_made}\n", style="cyan")
    panel_content.append("Duration: ", style="bold")
    panel_content.append(f"{result.duration_ms}ms\n\n", style="yellow")
    panel_content.append(result.description, style="white")

    if result.commands_executed:
        _append_commands_list(panel_content, result.commands_executed)

    console.print(
        Panel(panel_content,
              title="[bold green]Layout Optimized[/bold green]", box=ASCII)
    )


def _append_commands_list(content: Text, commands: list[str], max_show: int = 10) -> None:
    """Append commands list to panel content."""
    content.append("\n\nCommands executed:\n", style="bold")
    for cmd in commands[:max_show]:
        content.append(f"  * {cmd}\n", style="dim")
    if len(commands) > max_show:
        content.append(
            f"  ... and {len(commands) - max_show} more\n", style="dim")


def chat_command(
    message: Optional[str] = typer.Argument(None, help="Message to send"),
    interactive: bool = typer.Option(
        False, "--interactive", "-i", help="Interactive mode"
    ),
    conversation: Optional[str] = typer.Option(
        None, "--conversation", "-c", help="Conversation ID"
    ),
) -> None:
    """Chat with AI assistant about your desktop."""
    client = check_server()

    if interactive or message is None:
        _interactive_chat(client, conversation)
    else:
        _single_chat(client, message, conversation)


def _interactive_chat(client, conversation: Optional[str]) -> None:
    """Run interactive chat mode."""
    console.print(
        Panel(
            "Interactive chat mode. Type 'exit' or 'quit' to exit.\n"
            "Type 'clear' to start a new conversation.",
            title="[bold blue]ai3 Chat[/bold blue]",
            box=ASCII,
        )
    )

    conv_id = conversation

    while True:
        user_input = _get_user_input()
        if user_input is None:
            break

        action = _handle_special_commands(user_input, client, conv_id)
        if action == "break":
            break
        if action == "continue":
            conv_id = None if user_input.lower() == "clear" else conv_id
            continue

        response, conv_id = _process_chat_message(client, user_input, conv_id)
        if response:
            _display_chat_response(response)


def _get_user_input() -> Optional[str]:
    """Get user input, return None on EOF/interrupt."""
    try:
        return console.input("[bold cyan]You:[/bold cyan] ")
    except (EOFError, KeyboardInterrupt):
        console.print("\n[dim]Goodbye![/dim]")
        return None


def _handle_special_commands(user_input: str, client, conv_id: Optional[str]) -> Optional[str]:
    """Handle special chat commands. Returns action: 'break', 'continue', or None."""
    lower_input = user_input.lower()

    if lower_input in ("exit", "quit"):
        return "break"

    if lower_input == "clear":
        if conv_id:
            client.clear_conversation(conv_id)
        console.print("[dim]Conversation cleared.[/dim]")
        return "continue"

    if not user_input.strip():
        return "continue"

    return None


def _process_chat_message(client, message: str, conv_id: Optional[str]):
    """Process a chat message and return response."""
    console.print("[dim]Thinking...[/dim]", end="")
    try:
        response, new_conv_id = client.chat(message, conversation_id=conv_id)
        console.print("\r" + " " * 20 + "\r", end="")
        return response, new_conv_id
    except Exception as e:
        console.print("\r" + " " * 20 + "\r", end="")
        console.print(f"[red]Error: {e}[/red]")
        return None, conv_id


def _display_chat_response(response) -> None:
    """Display a chat response with tool info."""
    console.print(f"[bold green]AI:[/bold green] {response.message}")

    if response.tool_calls:
        tool_summary = ", ".join(
            f"{tc['tool']}({'✓' if tc['success'] else '✗'})"
            for tc in response.tool_calls
        )
        console.print(f"[dim]Tools used: {tool_summary}[/dim]")

    console.print()


def _single_chat(client, message: str, conversation: Optional[str]) -> None:
    """Send single chat message."""
    console.print("[dim]Thinking...[/dim]")
    response, conv_id = client.chat(message, conversation_id=conversation)

    print_response_panel(response.message, response.tool_calls)
    console.print(f"[dim]Conversation ID: {conv_id}[/dim]")


def ask_command(
    question: Optional[str] = typer.Argument(
        None, help="Question to ask the AI"),
) -> None:
    """Ask AI a single question (prompts for input if no question provided)."""
    client = check_server()

    if question is None:
        question = _prompt_for_question()
        if question is None:
            return

    console.print("[dim]Thinking...[/dim]")
    response, _ = client.chat(question)
    print_response_panel(response.message, response.tool_calls)


def _prompt_for_question() -> Optional[str]:
    """Prompt user for a question. Returns None if cancelled or empty."""
    console.print(
        Panel(
            "Ask the AI anything about your desktop, windows, or i3wm.",
            title="[bold blue]Ask AI[/bold blue]",
            box=ASCII,
        )
    )
    try:
        question = console.input("[bold cyan]Your question:[/bold cyan] ")
    except (EOFError, KeyboardInterrupt):
        console.print("\n[dim]Cancelled.[/dim]")
        return None

    if not question.strip():
        console.print("[yellow]No question provided.[/yellow]")
        return None

    return question
