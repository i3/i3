"""
Agent endpoints: suggest, layout, chat.
"""

from typing import Optional

from fastapi import APIRouter, HTTPException

from ai3_server.models import (
    ActionType,
    ChatRequest,
    ChatResponseModel,
    LayoutRequest,
    LayoutResponse,
    Suggestion,
    SuggestRequest,
    SuggestResponse,
    ToolExecuteRequest,
    ToolExecuteResponse,
)

router = APIRouter(tags=["agents"])

# These will be set by the server during initialization
_suggest_agent = None
_layout_agent = None
_chat_agent = None


def set_agents(suggest_agent, layout_agent, chat_agent):
    """Set agent references from server."""
    global _suggest_agent, _layout_agent, _chat_agent
    _suggest_agent = suggest_agent
    _layout_agent = layout_agent
    _chat_agent = chat_agent


def _require_agent(agent, name: str):
    """Raise HTTPException if agent is not initialized."""
    if not agent:
        raise HTTPException(
            status_code=503, detail=f"{name} agent not initialized")
    return agent


@router.post("/suggest", response_model=SuggestResponse)
async def get_suggestion(request: SuggestRequest = SuggestRequest()):
    """Get next action suggestion."""
    agent = _require_agent(_suggest_agent, "Suggest")
    suggestion = agent.run(include_screenshot=request.include_screenshot)
    return SuggestResponse(suggestion=suggestion)


@router.post("/suggest/accept")
async def accept_suggestion(suggestion_data: dict):
    """Accept and execute a suggestion."""
    agent = _require_agent(_suggest_agent, "Suggest")

    try:
        suggestion = Suggestion(
            action_type=ActionType(
                suggestion_data.get("action_type", "command")),
            description=suggestion_data.get("description", ""),
            keys=suggestion_data.get("keys"),
            command=suggestion_data.get("command"),
            target_id=suggestion_data.get("target_id"),
            confidence=suggestion_data.get("confidence", 0.8),
        )
        success = agent.accept_suggestion(suggestion)
        return {"success": success}
    except Exception as e:
        return {"success": False, "error": str(e)}


@router.post("/layout/optimize", response_model=LayoutResponse)
async def optimize_layout(request: LayoutRequest = LayoutRequest()):
    """Optimize window layout."""
    agent = _require_agent(_layout_agent, "Layout")
    result = agent.run(goal=request.goal,
                       include_screenshot=request.include_screenshot)
    return LayoutResponse(result=result)


@router.post("/chat", response_model=ChatResponseModel)
async def chat(request: ChatRequest):
    """Chat with AI assistant."""
    agent = _require_agent(_chat_agent, "Chat")
    response, conv_id = agent.run(
        message=request.message,
        conversation_id=request.conversation_id,
        include_screenshot=request.include_screenshot,
    )
    return ChatResponseModel(response=response, conversation_id=conv_id)


@router.get("/chat/conversations")
async def list_conversations():
    """List active chat conversations."""
    agent = _require_agent(_chat_agent, "Chat")
    return {"conversations": agent.list_conversations()}


@router.delete("/chat/conversations/{conversation_id}")
async def clear_conversation(conversation_id: str):
    """Clear a chat conversation."""
    agent = _require_agent(_chat_agent, "Chat")
    success = agent.clear_conversation(conversation_id)
    return {"success": success}
