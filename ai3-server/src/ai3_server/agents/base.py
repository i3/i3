"""
Base agent class for OpenAI function calling with i3 tools.
"""

import json
import logging
from abc import ABC, abstractmethod
from typing import Any, Optional

from openai import OpenAI
from openai.types.chat import ChatCompletionMessageToolCall

from ai3_server.config import AI3Config, get_config
from ai3_server.i3.tools import I3Tools, ToolResult, get_i3_tools

logger = logging.getLogger(__name__)


class BaseAgent(ABC):
    """
    Base class for AI agents that use OpenAI with i3 tools.

    Provides common functionality for:
    - OpenAI client management
    - Tool execution
    - Chat completion with automatic tool calling
    - Usage statistics tracking

    Subclasses must implement:
    - system_prompt: The system prompt for the agent
    - run(): The main entry point for the agent
    """

    def __init__(
        self,
        name: str,
        config: Optional[AI3Config] = None,
        tools: Optional[I3Tools] = None,
    ):
        """
        Initialize agent.

        Args:
            name: Agent name for identification and logging.
            config: Optional configuration. Uses global config if not provided.
            tools: Optional I3Tools instance. Uses global instance if not provided.
        """
        self.name = name
        self.config = config or get_config()
        self.tools = tools or get_i3_tools()
        self._client: Optional[OpenAI] = None

        # Statistics
        self.total_runs = 0
        self.total_tokens = 0
        self.errors = 0

    @property
    def client(self) -> OpenAI:
        """Get or create OpenAI client (lazy initialization)."""
        if self._client is None:
            self._client = self._create_openai_client()
        return self._client

    def _create_openai_client(self) -> OpenAI:
        """Create a new OpenAI client with configured settings."""
        kwargs = {"api_key": self.config.openai_api_key}
        if self.config.openai_base_url:
            kwargs["base_url"] = self.config.openai_base_url
        return OpenAI(**kwargs)

    @property
    @abstractmethod
    def system_prompt(self) -> str:
        """Get the system prompt for this agent. Must be implemented by subclasses."""
        pass

    @property
    def tool_definitions(self) -> list[dict]:
        """
        Get tool definitions for this agent.

        Override in subclasses to customize available tools.

        Returns:
            List of OpenAI function calling tool definitions.
        """
        return I3Tools.get_tool_definitions()

    def _execute_tool_call(self, name: str, arguments: str) -> dict[str, Any]:
        """
        Execute a tool call and return the result.

        Args:
            name: Tool name to execute.
            arguments: JSON string of tool arguments.

        Returns:
            Dictionary with 'success', 'data', and 'error' keys.
        """
        try:
            args = json.loads(arguments) if arguments else {}
            result: ToolResult = self.tools.execute_tool(name, args)
            return result.to_dict()
        except json.JSONDecodeError as e:
            logger.error(f"Invalid JSON arguments for {name}: {e}")
            return {"success": False, "data": None, "error": f"Invalid arguments: {e}"}
        except Exception as e:
            logger.error(f"Tool execution failed: {name} - {e}")
            return {"success": False, "data": None, "error": str(e)}

    def _process_tool_calls(
        self,
        tool_calls: list[ChatCompletionMessageToolCall],
        messages: list[dict]
    ) -> tuple[list[dict], list[dict]]:
        """
        Process tool calls and append results to messages.

        Args:
            tool_calls: List of tool calls from the API response.
            messages: Current message history to append to.

        Returns:
            Tuple of (updated messages, list of tool results).
        """
        results = []

        for tool_call in tool_calls:
            name = tool_call.function.name
            arguments = tool_call.function.arguments
            logger.debug(f"Executing tool: {name} with args: {arguments}")

            result = self._execute_tool_call(name, arguments)
            results.append({"tool": name, "result": result,
                           "success": result["success"]})

            messages.append({
                "role": "tool",
                "tool_call_id": tool_call.id,
                "content": json.dumps(result),
            })

        return messages, results

    def chat_completion(
        self,
        messages: list[dict],
        max_iterations: int = 5,
        include_tools: bool = True,
    ) -> tuple[str, list[dict]]:
        """
        Run chat completion with automatic tool calling.

        Handles the complete conversation loop including:
        - Making API calls to OpenAI
        - Automatically executing tool calls
        - Tracking token usage
        - Iterating until a final response or max iterations

        Args:
            messages: Initial message history.
            max_iterations: Maximum number of API call iterations.
            include_tools: Whether to include tool definitions.

        Returns:
            Tuple of (final response text, list of all tool results).
        """
        all_tool_results: list[dict] = []

        for iteration in range(max_iterations):
            logger.debug(
                f"Chat completion iteration {iteration + 1}/{max_iterations}")

            response = self._make_api_call(messages, include_tools)
            self._update_token_stats(response)

            message = response.choices[0].message

            if not message.tool_calls:
                # No tool calls - we have a final response
                return message.content or "", all_tool_results

            # Process tool calls and continue loop
            self._append_assistant_message(messages, message)
            messages, results = self._process_tool_calls(
                message.tool_calls, messages)
            all_tool_results.extend(results)

        # Max iterations reached
        logger.warning(f"Max iterations ({max_iterations}) reached")
        return self._get_last_content(messages), all_tool_results

    def _make_api_call(self, messages: list[dict], include_tools: bool):
        """Make a single API call to OpenAI."""
        kwargs = {
            "model": self.config.openai_model,
            "messages": messages,
            "temperature": self.config.temperature,
            "max_tokens": self.config.max_tokens,
        }

        if include_tools and self.tool_definitions:
            kwargs["tools"] = self.tool_definitions
            kwargs["tool_choice"] = "auto"

        return self.client.chat.completions.create(**kwargs)

    def _update_token_stats(self, response) -> None:
        """Update token usage statistics from response."""
        if response.usage:
            self.total_tokens += response.usage.total_tokens

    def _append_assistant_message(self, messages: list[dict], message) -> None:
        """Append assistant message with tool calls to message history."""
        messages.append({
            "role": "assistant",
            "content": message.content,
            "tool_calls": [
                {
                    "id": tc.id,
                    "type": "function",
                    "function": {
                        "name": tc.function.name,
                        "arguments": tc.function.arguments,
                    },
                }
                for tc in message.tool_calls
            ],
        })

    def _get_last_content(self, messages: list[dict]) -> str:
        """Get content from the last message."""
        if messages:
            return messages[-1].get("content", "")
        return ""

    @abstractmethod
    def run(self, **kwargs) -> Any:
        """
        Run the agent's main task.

        Must be implemented by subclasses to define the agent's behavior.

        Returns:
            Agent-specific result type.
        """
        pass
