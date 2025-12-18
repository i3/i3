"""
Pydantic models for ai3-server.
"""

from enum import Enum
from typing import Any, Optional

from pydantic import BaseModel, Field


class ActionType(str, Enum):
    """Types of suggested actions."""

    FOCUS = "focus"
    MOVE = "move"
    RESIZE = "resize"
    LAYOUT = "layout"
    WORKSPACE = "workspace"
    CLOSE = "close"
    FULLSCREEN = "fullscreen"
    FLOATING = "floating"
    SPLIT = "split"
    COMMAND = "command"
    KEYS = "keys"


class Suggestion(BaseModel):
    """AI-generated action suggestion."""

    action_type: ActionType
    description: str
    keys: Optional[str] = None
    command: Optional[str] = None
    target_id: Optional[int] = None
    confidence: float = Field(ge=0.0, le=1.0, default=0.8)
    reason: Optional[str] = None


class LayoutOptimization(BaseModel):
    """Layout optimization result."""

    success: bool
    changes_made: int = 0
    description: str = ""
    commands_executed: list[str] = Field(default_factory=list)
    duration_ms: int = 0


class ChatMessage(BaseModel):
    """Chat message."""

    role: str  # "user", "assistant", "system"
    content: str


class ChatResponse(BaseModel):
    """Chat response from AI."""

    message: str
    tool_calls: list[dict[str, Any]] = Field(default_factory=list)
    tool_results: list[dict[str, Any]] = Field(default_factory=list)


class AgentStatus(BaseModel):
    """Status of an AI agent."""

    name: str
    running: bool = False
    total_runs: int = 0
    total_tokens: int = 0
    errors: int = 0


class ServerStatus(BaseModel):
    """Overall server status."""

    running: bool
    version: str
    uptime_seconds: float
    i3_connected: bool
    openai_configured: bool
    agents: list[AgentStatus] = Field(default_factory=list)
    total_requests: int = 0
    total_tokens: int = 0


class ToolCall(BaseModel):
    """A tool call from OpenAI."""

    id: str
    name: str
    arguments: dict[str, Any]


class WindowInfo(BaseModel):
    """Information about a window."""

    id: int
    name: Optional[str] = None
    window_class: Optional[str] = None
    window_instance: Optional[str] = None
    focused: bool = False
    floating: bool = False
    fullscreen: bool = False
    workspace: Optional[str] = None
    x: int = 0
    y: int = 0
    width: int = 0
    height: int = 0


class WorkspaceInfo(BaseModel):
    """Information about a workspace."""

    name: str
    num: int
    focused: bool = False
    visible: bool = False
    urgent: bool = False
    output: Optional[str] = None


# API Request/Response models


class SuggestRequest(BaseModel):
    """Request for action suggestion."""

    include_screenshot: bool = False


class SuggestResponse(BaseModel):
    """Response with action suggestion."""

    suggestion: Optional[Suggestion] = None
    error: Optional[str] = None


class LayoutRequest(BaseModel):
    """Request for layout optimization."""

    goal: Optional[str] = None
    include_screenshot: bool = False


class LayoutResponse(BaseModel):
    """Response from layout optimization."""

    result: Optional[LayoutOptimization] = None
    error: Optional[str] = None


class ChatRequest(BaseModel):
    """Chat request."""

    message: str
    include_screenshot: bool = False
    conversation_id: Optional[str] = None


class ChatResponseModel(BaseModel):
    """Chat response."""

    response: ChatResponse
    conversation_id: str
    error: Optional[str] = None


class ToolExecuteRequest(BaseModel):
    """Request to execute a tool directly."""

    name: str
    arguments: dict[str, Any] = Field(default_factory=dict)


class ToolExecuteResponse(BaseModel):
    """Response from tool execution."""

    success: bool
    data: Any = None
    error: Optional[str] = None
