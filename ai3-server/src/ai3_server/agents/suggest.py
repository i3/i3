"""
Suggestion agent - predicts user's next command or action.

This agent focuses on COMMAND suggestions:
- Switching workspaces
- Opening new terminals and running commands
- Typing commands in existing terminals
- Launching applications

It does NOT suggest window manipulation (focus, move, resize, fullscreen).
"""

import json
import logging
from typing import Optional

from ai3_server.agents.base import BaseAgent
from ai3_server.config import AI3Config
from ai3_server.i3.tools import I3Tools
from ai3_server.models import ActionType, Suggestion

logger = logging.getLogger(__name__)


class SuggestAgent(BaseAgent):
    """Agent that predicts the user's next command or action."""

    # Read-only tools for suggestion analysis
    ALLOWED_TOOLS = [
        "get_workspaces",
        "get_windows",
        "get_focused_window",
        "get_tree",
        "get_outputs",
        "get_system_info",
    ]

    def __init__(
        self,
        config: Optional[AI3Config] = None,
        tools: Optional[I3Tools] = None,
    ):
        """Initialize suggestion agent."""
        super().__init__("suggest", config, tools)

    @property
    def system_prompt(self) -> str:
        """System prompt for suggestion agent."""
        return """You are an intelligent assistant that predicts what COMMAND or ACTION the user wants to execute next.

Your task is to analyze the current screen (screenshot) and suggest what the user likely wants to DO next.

ALLOWED SUGGESTIONS:
1. **Open a new terminal** and run a command (e.g., "Open terminal and run 'git status'")
2. **Type a command** in an existing terminal (suggest what to type)
3. **Switch workspace** (e.g., "Switch to workspace 2")
4. **Launch an application** via dmenu or command (e.g., "Open Firefox")
5. **Execute a shell command** that helps the user's workflow

NOT ALLOWED (do NOT suggest these):
- Moving windows
- Resizing windows
- Focusing different windows
- Fullscreen toggles
- Window layout changes (split, tabbed, etc.)
- Floating toggles

Focus on PRODUCTIVITY - what command or program would help the user RIGHT NOW based on what you see on screen?

Look at:
- What applications are open
- What's visible in terminals (errors, prompts, output)
- The overall context of what the user is working on

After analyzing, respond with a JSON object:
{
    "action_type": "command|workspace|keys",
    "description": "Brief description (e.g., 'Run git pull in terminal')",
    "command": "The actual command to execute (shell command or i3 command)",
    "keys": "Key sequence if typing is needed (e.g., 'git pull' to type, or 'ctrl+shift+Return' to open terminal)",
    "confidence": 0.0-1.0,
    "reason": "Why this action is suggested based on what you see"
}

Examples:
- {"action_type": "command", "description": "Open new terminal", "command": "exec xterm", "confidence": 0.9, "reason": "User has no terminal open"}
- {"action_type": "keys", "description": "Type git status command", "keys": "git status", "confidence": 0.85, "reason": "Terminal is at prompt in a git repo"}
- {"action_type": "workspace", "description": "Switch to workspace 2", "command": "workspace 2", "confidence": 0.8, "reason": "User has work on workspace 2"}

Only respond with the JSON object, no other text."""

    @property
    def tool_definitions(self) -> list[dict]:
        """Limited tools for suggestion - read-only operations."""
        all_tools = I3Tools.get_tool_definitions()
        return [t for t in all_tools if t["function"]["name"] in self.ALLOWED_TOOLS]

    def run(self, include_screenshot: bool = True) -> Optional[Suggestion]:
        """
        Run suggestion agent to predict next command/action.

        Args:
            include_screenshot: Whether to include a screenshot (default True)

        Returns:
            Suggestion object or None if failed
        """
        self.total_runs += 1

        try:
            # Build user message with optional screenshot
            user_content = []

            # Always try to include screenshot for better context
            if include_screenshot:
                screenshot_result = self.tools.screenshot()
                if screenshot_result.success:
                    user_content.append({
                        "type": "image_url",
                        "image_url": {
                            "url": f"data:image/png;base64,{screenshot_result.data['base64']}",
                            "detail": "high"
                        }
                    })
                    logger.info("Screenshot included in suggestion request")

            user_content.append({
                "type": "text",
                "text": "Look at the screenshot and suggest what COMMAND or ACTION the user most likely wants to execute next. Focus on productivity - what would help them right now? Do NOT suggest window movements, resizing, or focusing."
            })

            messages = [
                {"role": "system", "content": self.system_prompt},
                {"role": "user", "content": user_content},
            ]

            # Run completion with tools
            response_text, tool_results = self.chat_completion(
                messages, max_iterations=3, include_tools=True
            )

            # Parse the JSON response
            try:
                # Try to find JSON in the response
                response_text = response_text.strip()
                if response_text.startswith("```"):
                    # Remove markdown code block
                    lines = response_text.split("\n")
                    response_text = "\n".join(lines[1:-1])

                # Find JSON in response
                start = response_text.find("{")
                end = response_text.rfind("}") + 1
                if start != -1 and end > start:
                    response_text = response_text[start:end]

                data = json.loads(response_text)

                # Only allow safe action types
                action_type_str = data.get("action_type", "").lower()

                # Map allowed action types only
                allowed_actions = {
                    "command": ActionType.COMMAND,
                    "workspace": ActionType.WORKSPACE,
                    "keys": ActionType.KEYS,
                }

                # Reject disallowed actions
                disallowed = ["focus", "move", "resize", "layout",
                              "fullscreen", "floating", "split", "close"]
                if action_type_str in disallowed:
                    logger.warning(
                        f"Rejected disallowed action type: {action_type_str}")
                    # Convert to a safe command instead
                    action_type = ActionType.COMMAND
                else:
                    action_type = allowed_actions.get(
                        action_type_str, ActionType.COMMAND)

                return Suggestion(
                    action_type=action_type,
                    description=data.get("description", ""),
                    keys=data.get("keys"),
                    command=data.get("command"),
                    target_id=None,  # Don't target specific windows
                    confidence=float(data.get("confidence", 0.8)),
                    reason=data.get("reason"),
                )

            except json.JSONDecodeError as e:
                logger.error(f"Failed to parse suggestion JSON: {e}")
                logger.error(f"Response was: {response_text}")
                self.errors += 1
                return None

        except Exception as e:
            logger.error(f"Suggestion agent failed: {e}")
            self.errors += 1
            return None

    def accept_suggestion(self, suggestion: Suggestion) -> bool:
        """
        Execute a suggestion.

        Args:
            suggestion: The suggestion to execute

        Returns:
            True if executed successfully
        """
        try:
            # For workspace switching
            if suggestion.action_type == ActionType.WORKSPACE and suggestion.command:
                result = self.tools.run_command(suggestion.command)
                return result.success

            # For typing commands (simulate keyboard input)
            if suggestion.action_type == ActionType.KEYS and suggestion.keys:
                # If it looks like a command to type (no modifier keys), use type_text
                if not any(mod in suggestion.keys.lower() for mod in ['ctrl', 'alt', 'super', 'shift+']):
                    result = self.tools.type_text(suggestion.keys)
                else:
                    result = self.tools.press_keys(suggestion.keys)
                return result.success

            # For i3 commands (exec, workspace, etc.)
            if suggestion.command:
                result = self.tools.run_command(suggestion.command)
                return result.success

            logger.warning("Suggestion has no executable command or keys")
            return False

        except Exception as e:
            logger.error(f"Failed to execute suggestion: {e}")
            return False
