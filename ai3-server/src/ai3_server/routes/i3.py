"""
i3 information endpoints.
"""

from fastapi import APIRouter, HTTPException

from ai3_server.i3 import get_i3_tools

router = APIRouter(prefix="/i3", tags=["i3"])


def _get_i3_data(method_name: str, key: str):
    """Helper to get i3 data and handle errors."""
    tools = get_i3_tools()
    result = getattr(tools, method_name)()
    if result.success:
        return {key: result.data}
    raise HTTPException(status_code=500, detail=result.error)


@router.get("/windows")
async def get_windows():
    """Get all windows."""
    return _get_i3_data("get_windows", "windows")


@router.get("/workspaces")
async def get_workspaces():
    """Get all workspaces."""
    return _get_i3_data("get_workspaces", "workspaces")


@router.get("/tree")
async def get_tree():
    """Get container tree."""
    return _get_i3_data("get_tree", "tree")


@router.get("/outputs")
async def get_outputs():
    """Get display outputs."""
    return _get_i3_data("get_outputs", "outputs")


@router.get("/focused")
async def get_focused():
    """Get focused window."""
    return _get_i3_data("get_focused_window", "focused")
