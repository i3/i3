"""
HTTP client for ai3-server.
"""

from typing import Optional

import requests

from ai3_server.config import AI3Config, get_config
from ai3_server.models import ChatResponse, LayoutOptimization, ServerStatus, Suggestion


class AI3Client:
    """Client for interacting with ai3-server."""

    def __init__(self, config: Optional[AI3Config] = None):
        """Initialize client."""
        self.config = config or get_config()
        self._base_url = self.config.server_url

    def _request(
        self,
        method: str,
        endpoint: str,
        data: Optional[dict] = None,
        params: Optional[dict] = None,
    ) -> dict:
        """Make HTTP request to server."""
        url = f"{self._base_url}{endpoint}"

        try:
            if method == "GET":
                response = requests.get(url, params=params, timeout=120)
            elif method == "POST":
                response = requests.post(url, json=data, timeout=120)
            elif method == "DELETE":
                response = requests.delete(url, timeout=30)
            else:
                raise ValueError(f"Unsupported method: {method}")

            response.raise_for_status()
            return response.json()

        except requests.exceptions.ConnectionError:
            raise ConnectionError(
                f"Cannot connect to ai3-server at {self._base_url}. "
                "Make sure the server is running: ai3-server"
            )
        except requests.exceptions.Timeout:
            raise TimeoutError("Request timed out")
        except requests.exceptions.HTTPError as e:
            raise RuntimeError(f"HTTP error: {e}")

    def status(self) -> ServerStatus:
        """Get server status."""
        data = self._request("GET", "/status")
        return ServerStatus(**data)

    def is_running(self) -> bool:
        """Check if server is running."""
        try:
            status = self.status()
            return status.running
        except Exception:
            return False

    def suggest(self, include_screenshot: bool = False) -> Optional[Suggestion]:
        """Get next action suggestion."""
        data = self._request(
            "POST",
            "/suggest",
            data={"include_screenshot": include_screenshot},
        )
        if data.get("suggestion"):
            return Suggestion(**data["suggestion"])
        return None

    def accept_suggestion(self, suggestion: Suggestion) -> bool:
        """Accept and execute a suggestion."""
        data = self._request(
            "POST",
            "/suggest/accept",
            data=suggestion.model_dump(mode="json"),
        )
        return data.get("success", False)

    def optimize_layout(
        self,
        goal: Optional[str] = None,
        include_screenshot: bool = False,
    ) -> LayoutOptimization:
        """Optimize window layout."""
        data = self._request(
            "POST",
            "/layout/optimize",
            data={"goal": goal, "include_screenshot": include_screenshot},
        )
        if data.get("result"):
            return LayoutOptimization(**data["result"])
        return LayoutOptimization(success=False, description=data.get("error", "Unknown error"))

    def chat(
        self,
        message: str,
        conversation_id: Optional[str] = None,
        include_screenshot: bool = False,
    ) -> tuple[ChatResponse, str]:
        """Send chat message."""
        data = self._request(
            "POST",
            "/chat",
            data={
                "message": message,
                "conversation_id": conversation_id,
                "include_screenshot": include_screenshot,
            },
        )
        response = ChatResponse(**data["response"])
        return response, data["conversation_id"]

    def list_conversations(self) -> list[dict]:
        """List chat conversations."""
        data = self._request("GET", "/chat/conversations")
        return data.get("conversations", [])

    def clear_conversation(self, conversation_id: str) -> bool:
        """Clear a conversation."""
        data = self._request(
            "DELETE", f"/chat/conversations/{conversation_id}")
        return data.get("success", False)

    def get_tools(self) -> list[dict]:
        """Get available tools."""
        data = self._request("GET", "/tools")
        return data.get("tools", [])

    def execute_tool(self, name: str, arguments: Optional[dict] = None) -> dict:
        """Execute a tool directly."""
        data = self._request(
            "POST",
            "/tools/execute",
            data={"name": name, "arguments": arguments or {}},
        )
        return data

    # i3 helper methods

    def get_windows(self) -> list[dict]:
        """Get all windows."""
        data = self._request("GET", "/i3/windows")
        return data.get("windows", [])

    def get_workspaces(self) -> list[dict]:
        """Get all workspaces."""
        data = self._request("GET", "/i3/workspaces")
        return data.get("workspaces", [])

    def get_tree(self) -> dict:
        """Get container tree."""
        data = self._request("GET", "/i3/tree")
        return data.get("tree", {})

    def get_focused(self) -> Optional[dict]:
        """Get focused window."""
        data = self._request("GET", "/i3/focused")
        return data.get("focused")
