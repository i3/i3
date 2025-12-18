"""
Layout optimization agent - analyzes and improves window layouts.
"""

import logging
import time
from typing import Optional

from ai3_server.agents.base import BaseAgent
from ai3_server.config import AI3Config
from ai3_server.i3.tools import I3Tools
from ai3_server.models import LayoutOptimization

logger = logging.getLogger(__name__)


class LayoutAgent(BaseAgent):
    """Agent that optimizes window layouts based on user goals."""

    # Safe tools for layout optimization
    ALLOWED_TOOLS = [
        "get_workspaces",
        "get_windows",
        "get_focused_window",
        "get_tree",
        "get_outputs",
        "resize_window",
    ]

    def __init__(
        self,
        config: Optional[AI3Config] = None,
        tools: Optional[I3Tools] = None,
    ):
        """Initialize layout agent."""
        super().__init__("layout", config, tools)

    @property
    def system_prompt(self) -> str:
        """System prompt for layout agent."""
        return """You are a careful i3 window manager layout optimizer. Your job is to make SUBTLE, SENSIBLE adjustments to improve the layout.

IMPORTANT: Look at the screenshot to understand the current layout before making changes.

RULES - BE CONSERVATIVE:
1. DO NOT dramatically resize windows - only adjust by 5-15 percentage points at most
2. DO NOT make windows tiny - every window should remain usable
3. DO NOT center windows or change their position dramatically
4. PRESERVE the general layout structure (if windows are side-by-side, keep them that way)

WHAT TO DO:
- Make ACTIVE/IMPORTANT windows slightly larger (editor, browser you're working in)
- Make BACKGROUND/LESS IMPORTANT windows slightly smaller (empty terminals, status windows)
- If windows are roughly equal size but one is clearly the "main" window, give it 55-60% instead of 50%

SIZING GUIDELINES:
- Main/active window: 55-65% of space (NOT more)
- Secondary windows: 35-45% of space (NOT less than 25%)
- Use moderate adjustments: resize by 10-20 ppt at a time

TOOLS:
- resize_window(con_id, direction, amount, unit):
  - direction: 'width' or 'height'
  - amount: use values like 10, 15, 20, -10, -15, -20
  - unit: 'ppt' for percentage points
- get_windows(): Get window list with IDs
- get_focused_window(): Get the currently focused window

DO NOT USE these for layout optimization:
- move_window (don't move windows around)
- set_layout (don't change tiling modes)
- focus_window (don't change focus)

IMPORTANT: Make MULTIPLE resize calls (3-5 operations) to properly optimize the layout.
Adjust both width AND height as needed. Call resize_window multiple times in sequence.
After making your adjustments, describe what you changed."""

    @property
    def tool_definitions(self) -> list[dict]:
        """Limited tools for layout - only resize and read operations."""
        all_tools = I3Tools.get_tool_definitions()
        return [t for t in all_tools if t["function"]["name"] in self.ALLOWED_TOOLS]

    def run(
        self,
        goal: Optional[str] = None,
        include_screenshot: bool = True,
    ) -> LayoutOptimization:
        """
        Run layout optimization.

        Args:
            goal: Optional goal description
            include_screenshot: Whether to include a screenshot (default True)

        Returns:
            LayoutOptimization result
        """
        self.total_runs += 1
        import time

        start_time = time.time()
        commands_executed = []

        try:
            # Build user message with screenshot
            user_content = []

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
                    logger.info("Screenshot included in layout optimization")

            user_content.append({
                "type": "text",
                "text": f"""Look at the screenshot and optimize the layout with MULTIPLE resize operations.

Goal: {goal or 'Make the main/active window larger, balance secondary windows appropriately'}

IMPORTANT - MAKE MULTIPLE TOOL CALLS:
- Use resize_window with amounts of 10-20 ppt
- Make 3-5 resize operations to properly optimize
- Adjust BOTH width AND height where appropriate
- Call resize_window multiple times in a row

Steps:
1. Look at the screenshot - identify the main window and secondary windows
2. Get window IDs with get_windows
3. Make MULTIPLE resize_window calls:
   - Resize main window width (e.g., +15 ppt)
   - Resize main window height if needed
   - Resize secondary windows to balance
   - Fine-tune with additional resizes
4. After 3-5 resize operations, describe what you changed"""
            })

            messages = [
                {"role": "system", "content": self.system_prompt},
                {"role": "user", "content": user_content},
            ]

            # Track tool executions
            class ToolTracker:
                def __init__(self, original_execute):
                    self.original = original_execute
                    self.calls = []

                def __call__(self, name, args):
                    result = self.original(name, args)
                    if name in [
                        "resize_window",
                        "move_window",
                        "set_layout",
                        "focus_window",
                        "close_window",
                        "toggle_fullscreen",
                        "toggle_floating",
                        "split_container",
                        "run_command",
                    ]:
                        self.calls.append(f"{name}({args})")
                    return result

            # Wrap tool execution to track calls
            original_execute = self.tools.execute_tool
            tracker = ToolTracker(original_execute)
            self.tools.execute_tool = tracker

            try:
                # Allow enough iterations for multiple resize operations
                response_text, tool_results = self.chat_completion(
                    messages, max_iterations=6, include_tools=True
                )
                commands_executed = tracker.calls
            finally:
                # Restore original
                self.tools.execute_tool = original_execute

            duration_ms = int((time.time() - start_time) * 1000)

            return LayoutOptimization(
                success=True,
                changes_made=len(commands_executed),
                description=response_text,
                commands_executed=commands_executed,
                duration_ms=duration_ms,
            )

        except Exception as e:
            logger.error(f"Layout optimization failed: {e}")
            self.errors += 1
            duration_ms = int((time.time() - start_time) * 1000)
            return LayoutOptimization(
                success=False,
                changes_made=0,
                description=f"Error: {str(e)}",
                commands_executed=commands_executed,
                duration_ms=duration_ms,
            )
