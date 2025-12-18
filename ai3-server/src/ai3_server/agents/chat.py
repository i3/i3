"""
Chat agent - interactive chat with full i3 context.
"""

import logging
import uuid
from typing import Optional

from ai3_server.agents.base import BaseAgent
from ai3_server.config import AI3Config
from ai3_server.i3.tools import I3Tools
from ai3_server.models import ChatResponse

logger = logging.getLogger(__name__)


class ChatAgent(BaseAgent):
    """Interactive chat agent with full i3 context and tool access."""

    def __init__(
        self,
        config: Optional[AI3Config] = None,
        tools: Optional[I3Tools] = None,
    ):
        """Initialize chat agent."""
        super().__init__("chat", config, tools)
        self._conversations: dict[str, list[dict]] = {}

    @property
    def system_prompt(self) -> str:
        """System prompt for chat agent."""
        return """You are an intelligent i3 window manager assistant.

You have full access to the i3 window manager through various tools. You can:
- View and analyze window layouts, workspaces, and containers
- Move, resize, focus, and close windows
- Change layouts (tiling, stacking, tabbed)
- Launch applications (browsers, terminals, editors, etc.)
- Execute any i3 command
- Simulate key presses and type text
- Capture screenshots
- Get system information

When the user asks questions or requests actions:
1. Use tools to gather necessary information
2. Execute requested changes using appropriate tools
3. Provide clear, helpful responses

For questions about the current state, use tools like:
- get_windows, get_workspaces, get_tree for layout info
- get_focused_window for current focus
- get_system_info for system context

For actions, use tools like:
- open_terminal to open a new terminal window
- launch_app to open applications (firefox, nautilus, gedit, etc.)
- focus_window, move_window, resize_window for window operations
- set_layout, split_container for layout changes
- switch_workspace for navigation
- run_command for any i3 command (use 'exec <app>' to launch apps)
- press_keys for simulating keyboard shortcuts
- type_text to type text into the focused window

Be concise but helpful. When making changes, explain what you did."""

    def get_conversation(self, conversation_id: Optional[str] = None) -> tuple[str, list[dict]]:
        """
        Get or create a conversation.

        Args:
            conversation_id: Existing conversation ID or None to create new

        Returns:
            Tuple of (conversation_id, messages)
        """
        if conversation_id and conversation_id in self._conversations:
            return conversation_id, self._conversations[conversation_id]

        # Create new conversation
        new_id = str(uuid.uuid4())[:8]
        messages = [{"role": "system", "content": self.system_prompt}]
        self._conversations[new_id] = messages
        return new_id, messages

    def clear_conversation(self, conversation_id: str) -> bool:
        """Clear a conversation history."""
        if conversation_id in self._conversations:
            del self._conversations[conversation_id]
            return True
        return False

    def run(
        self,
        message: str,
        conversation_id: Optional[str] = None,
        include_screenshot: bool = False,
    ) -> tuple[ChatResponse, str]:
        """
        Process a chat message.

        Args:
            message: User's message
            conversation_id: Optional conversation ID for context
            include_screenshot: Whether to include a screenshot

        Returns:
            Tuple of (ChatResponse, conversation_id)
        """
        self.total_runs += 1

        try:
            conv_id, messages = self.get_conversation(conversation_id)

            # Add user message
            user_content = message
            if include_screenshot:
                # For vision models, we could add image here
                # For now, just capture and describe
                screenshot_result = self.tools.screenshot()
                if screenshot_result.success:
                    user_content = f"{message}\n\n[Screenshot captured - {screenshot_result.data.get('size', 0)} bytes]"

            messages.append({"role": "user", "content": user_content})

            # Run completion
            response_text, tool_results = self.chat_completion(
                messages, max_iterations=10, include_tools=True
            )

            # Add assistant response to history
            messages.append({"role": "assistant", "content": response_text})

            # Keep conversation size manageable (last 20 messages + system)
            if len(messages) > 21:
                messages = [messages[0]] + messages[-20:]
                self._conversations[conv_id] = messages

            response = ChatResponse(
                message=response_text,
                tool_calls=[
                    {"tool": r["tool"], "success": r["result"]["success"]} for r in tool_results],
                tool_results=tool_results,
            )

            return response, conv_id

        except Exception as e:
            logger.error(f"Chat agent failed: {e}")
            self.errors += 1
            return ChatResponse(message=f"Error: {str(e)}"), conversation_id or ""

    def list_conversations(self) -> list[dict]:
        """List all active conversations."""
        return [
            {
                "id": cid,
                "messages": len(msgs),
                "last_message": msgs[-1]["content"][:50] if msgs else "",
            }
            for cid, msgs in self._conversations.items()
        ]
