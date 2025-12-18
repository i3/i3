"""
Auto mode and Computer Use Agent (CUA) endpoints.
"""

from typing import Optional

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

router = APIRouter(tags=["auto"])


class AutoModeStartRequest(BaseModel):
    """Request to start auto mode."""

    goal: str


class AutoModeResponse(BaseModel):
    """Response for auto mode operations."""

    success: bool
    message: str
    state: Optional[dict] = None


# CUA agent reference - set by server during initialization
_cua_agent = None


def set_cua_agent(agent):
    """Set CUA agent reference from server."""
    global _cua_agent
    _cua_agent = agent


def _require_cua_agent():
    """Raise HTTPException if CUA agent is not initialized."""
    if not _cua_agent:
        raise HTTPException(status_code=503, detail="CUA not initialized")
    return _cua_agent


@router.post("/auto/start", response_model=AutoModeResponse)
async def start_auto_mode(request: AutoModeStartRequest):
    """Start autonomous mode with a goal."""
    cua = _require_cua_agent()

    if not request.goal.strip():
        return AutoModeResponse(
            success=False,
            message="Goal cannot be empty",
            state=cua.get_state(),
        )

    success = cua.start(request.goal)

    if success:
        return AutoModeResponse(
            success=True,
            message=f"Auto mode started with goal: {request.goal}",
            state=cua.get_state(),
        )
    return AutoModeResponse(
        success=False,
        message="Failed to start auto mode (may already be running)",
        state=cua.get_state(),
    )


@router.post("/auto/stop", response_model=AutoModeResponse)
async def stop_auto_mode():
    """Stop autonomous mode."""
    cua = _require_cua_agent()

    success = cua.stop()

    if success:
        return AutoModeResponse(
            success=True,
            message="Auto mode stop requested",
            state=cua.get_state(),
        )
    return AutoModeResponse(
        success=False,
        message="Auto mode is not running",
        state=cua.get_state(),
    )


@router.get("/auto/status", response_model=AutoModeResponse)
async def get_auto_mode_status():
    """Get autonomous mode status."""
    cua = _require_cua_agent()

    state = cua.get_state()
    status = state.get("status", "idle")

    messages = {
        "idle": "Auto mode is idle",
        "running": f"Auto mode running: {state.get('current_action', 'working')}",
        "stopping": "Auto mode is stopping",
        "error": f"Auto mode error: {state.get('error', 'unknown')}",
    }

    return AutoModeResponse(
        success=True,
        message=messages.get(status, "Unknown status"),
        state=state,
    )
