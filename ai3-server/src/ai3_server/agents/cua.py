"""
Computer Use Agent (CUA) - Autonomous computer control using OpenAI's CUA model.

This agent uses the computer-use-preview model to take control of the computer
and perform tasks autonomously based on user goals.
"""

import base64
import logging
import subprocess
import threading
import time
from dataclasses import dataclass
from enum import Enum
from typing import Callable, Optional

from openai import OpenAI

from ai3_server.config import AI3Config, get_config

logger = logging.getLogger(__name__)


class CUAStatus(str, Enum):
    """Status of the CUA agent."""
    IDLE = "idle"
    RUNNING = "running"
    PAUSED = "paused"
    STOPPING = "stopping"
    ERROR = "error"


@dataclass
class CUAState:
    """Current state of the CUA agent."""
    status: CUAStatus = CUAStatus.IDLE
    goal: str = ""
    current_action: str = ""
    actions_taken: int = 0
    last_reasoning: str = ""
    error: Optional[str] = None
    started_at: Optional[float] = None
    response_id: Optional[str] = None


@dataclass
class ActionResult:
    """Result of executing an action."""
    success: bool
    screenshot_base64: Optional[str] = None
    error: Optional[str] = None


class ComputerUseAgent:
    """
    Agent that uses OpenAI's Computer Use Agent model to autonomously
    control the computer based on user goals.
    """

    DISPLAY_WIDTH = 1920
    DISPLAY_HEIGHT = 1080

    def __init__(self, config: Optional[AI3Config] = None):
        """Initialize the CUA agent."""
        self.config = config or get_config()
        self._client: Optional[OpenAI] = None
        self.state = CUAState()
        self._stop_requested = False
        self._thread: Optional[threading.Thread] = None
        self._status_callback: Optional[Callable[[CUAState], None]] = None

    @property
    def client(self) -> OpenAI:
        """Get or create OpenAI client."""
        if self._client is None:
            kwargs = {"api_key": self.config.openai_api_key}
            if self.config.openai_base_url:
                kwargs["base_url"] = self.config.openai_base_url
            self._client = OpenAI(**kwargs)
        return self._client

    def set_status_callback(self, callback: Callable[[CUAState], None]):
        """Set callback for status updates."""
        self._status_callback = callback

    def _notify_status(self):
        """Notify status callback if set."""
        if self._status_callback:
            try:
                self._status_callback(self.state)
            except Exception as e:
                logger.error(f"Status callback error: {e}")

    def _capture_screenshot(self) -> Optional[str]:
        """Capture screenshot and return as base64."""
        try:
            result = subprocess.run(
                ["scrot", "-o", "/tmp/cua_screenshot.png"],
                capture_output=True,
                timeout=5,
            )
            if result.returncode == 0:
                with open("/tmp/cua_screenshot.png", "rb") as f:
                    return base64.b64encode(f.read()).decode("utf-8")
            logger.error(f"scrot failed: {result.stderr}")
            return None
        except Exception as e:
            logger.error(f"Screenshot capture failed: {e}")
            return None

    def _execute_action(self, action: dict) -> ActionResult:
        """Execute a computer action and return the result with screenshot."""
        action_type = action.get("type", "")

        try:
            if action_type == "click":
                x, y = action.get("x", 0), action.get("y", 0)
                button = action.get("button", "left")
                logger.info(f"CUA: click at ({x}, {y}) button={button}")

                # Use xdotool for clicking
                btn_map = {"left": "1", "right": "3", "middle": "2"}
                btn = btn_map.get(button, "1")
                subprocess.run(
                    ["xdotool", "mousemove", str(x), str(y), "click", btn],
                    timeout=5
                )

            elif action_type == "double_click":
                x, y = action.get("x", 0), action.get("y", 0)
                logger.info(f"CUA: double_click at ({x}, {y})")
                subprocess.run(
                    ["xdotool", "mousemove", str(x), str(
                        y), "click", "--repeat", "2", "1"],
                    timeout=5
                )

            elif action_type == "type":
                text = action.get("text", "")
                logger.info(f"CUA: type text: {text[:50]}...")
                # Use xdotool type with delay for reliability
                subprocess.run(
                    ["xdotool", "type", "--delay", "50", text],
                    timeout=30
                )

            elif action_type == "keypress":
                keys = action.get("keys", [])
                logger.info(f"CUA: keypress {keys}")
                for key in keys:
                    # Map common key names
                    key_map = {
                        "enter": "Return",
                        "return": "Return",
                        "tab": "Tab",
                        "escape": "Escape",
                        "esc": "Escape",
                        "space": "space",
                        "backspace": "BackSpace",
                        "delete": "Delete",
                        "up": "Up",
                        "down": "Down",
                        "left": "Left",
                        "right": "Right",
                        "home": "Home",
                        "end": "End",
                        "pageup": "Page_Up",
                        "pagedown": "Page_Down",
                        "ctrl": "ctrl",
                        "alt": "alt",
                        "shift": "shift",
                        "super": "super",
                    }
                    mapped_key = key_map.get(key.lower(), key)
                    subprocess.run(["xdotool", "key", mapped_key], timeout=5)

            elif action_type == "scroll":
                x, y = action.get("x", 0), action.get("y", 0)
                scroll_x = action.get("scroll_x", 0)
                scroll_y = action.get("scroll_y", 0)
                logger.info(
                    f"CUA: scroll at ({x}, {y}) by ({scroll_x}, {scroll_y})")

                # Move mouse to position first
                subprocess.run(
                    ["xdotool", "mousemove", str(x), str(y)], timeout=5)

                # Scroll (xdotool uses button 4/5 for scroll)
                if scroll_y > 0:
                    for _ in range(abs(scroll_y) // 50):
                        subprocess.run(["xdotool", "click", "5"],
                                       timeout=5)  # scroll down
                elif scroll_y < 0:
                    for _ in range(abs(scroll_y) // 50):
                        subprocess.run(["xdotool", "click", "4"],
                                       timeout=5)  # scroll up

            elif action_type == "drag":
                start_x, start_y = action.get(
                    "start_x", 0), action.get("start_y", 0)
                end_x, end_y = action.get("end_x", 0), action.get("end_y", 0)
                logger.info(
                    f"CUA: drag from ({start_x}, {start_y}) to ({end_x}, {end_y})")
                subprocess.run([
                    "xdotool", "mousemove", str(start_x), str(start_y),
                    "mousedown", "1",
                    "mousemove", str(end_x), str(end_y),
                    "mouseup", "1"
                ], timeout=10)

            elif action_type == "wait":
                logger.info("CUA: wait")
                time.sleep(2)

            elif action_type == "screenshot":
                logger.info("CUA: screenshot requested")
                # Just capture screenshot, no action needed

            else:
                logger.warning(f"CUA: unknown action type: {action_type}")

            # Wait a bit for action to take effect
            time.sleep(0.5)

            # Capture screenshot after action
            screenshot = self._capture_screenshot()
            return ActionResult(success=True, screenshot_base64=screenshot)

        except Exception as e:
            logger.error(f"Action execution failed: {e}")
            screenshot = self._capture_screenshot()
            return ActionResult(success=False, screenshot_base64=screenshot, error=str(e))

    def _run_loop(self, goal: str, initial_screenshot: Optional[str] = None):
        """Main CUA loop - runs in background thread."""
        self.state.status = CUAStatus.RUNNING
        self.state.goal = goal
        self.state.started_at = time.time()
        self.state.actions_taken = 0
        self.state.error = None
        self._notify_status()

        try:
            # Check if responses API is available
            if not hasattr(self.client, 'responses'):
                raise RuntimeError(
                    "Auto Mode requires OpenAI's Computer Use Agent (CUA) which is not available. "
                    "The 'responses' API is not supported in the standard OpenAI SDK. "
                    "This feature requires a special API endpoint. "
                    "Please use 'chat' mode instead for interactive AI assistance."
                )

            # Build initial input
            input_content = [
                {
                    "type": "input_text",
                    "text": f"Goal: {goal}\\n\\nPlease help me accomplish this goal by controlling the computer. Take actions step by step."
                }
            ]

            # Include initial screenshot if available
            if initial_screenshot:
                input_content.append({
                    "type": "input_image",
                    "image_url": f"data:image/png;base64,{initial_screenshot}"
                })

            # First request to the model
            logger.info(f"CUA: Starting with goal: {goal}")
            try:
                response = self.client.responses.create(
                    model="computer-use-preview",
                    tools=[{
                        "type": "computer_use_preview",
                        "display_width": self.DISPLAY_WIDTH,
                        "display_height": self.DISPLAY_HEIGHT,
                        "environment": "ubuntu"
                    }],
                    input=[{
                        "role": "user",
                        "content": input_content
                    }],
                    reasoning={"summary": "concise"},
                    truncation="auto"
                )
            except AttributeError:
                raise RuntimeError(
                    "Auto Mode is not available. The OpenAI 'responses' API (required for Computer Use Agent) "
                    "is not supported. Use 'ai3 chat' for interactive assistance instead."
                )
            except Exception as api_error:
                error_msg = str(api_error)
                if "400" in error_msg or "not found" in error_msg.lower():
                    raise RuntimeError(
                        f"Auto Mode API Error: The Computer Use Agent model is not accessible. "
                        f"This feature requires OpenAI's 'computer-use-preview' model which may not be available "
                        f"in your API configuration. Original error: {error_msg}"
                    )
                raise

            self.state.response_id = response.id

            # Main loop
            while not self._stop_requested:
                # Find computer_call in output
                computer_calls = [
                    item for item in response.output
                    if getattr(item, 'type', None) == "computer_call"
                ]

                # Check for reasoning
                reasoning_items = [
                    item for item in response.output
                    if getattr(item, 'type', None) == "reasoning"
                ]
                if reasoning_items:
                    for r in reasoning_items:
                        if hasattr(r, 'summary') and r.summary:
                            for s in r.summary:
                                if hasattr(s, 'text'):
                                    self.state.last_reasoning = s.text
                                    logger.info(f"CUA reasoning: {s.text}")

                if not computer_calls:
                    # No more actions - task complete or model stopped
                    logger.info(
                        "CUA: No computer_call in response - task may be complete")

                    # Check for text output
                    for item in response.output:
                        if hasattr(item, 'type') and item.type == "message":
                            if hasattr(item, 'content'):
                                for c in item.content:
                                    if hasattr(c, 'text'):
                                        logger.info(f"CUA message: {c.text}")
                                        self.state.last_reasoning = c.text
                    break

                computer_call = computer_calls[0]
                call_id = computer_call.call_id
                action = computer_call.action

                # Update state
                action_type = getattr(action, 'type', 'unknown')
                self.state.current_action = f"{action_type}"
                self.state.actions_taken += 1
                self._notify_status()

                # Handle safety checks
                pending_checks = getattr(
                    computer_call, 'pending_safety_checks', [])
                acknowledged_checks = []
                if pending_checks:
                    logger.warning(
                        f"CUA: Safety checks pending: {pending_checks}")
                    # Auto-acknowledge for now (in production, should prompt user)
                    for check in pending_checks:
                        acknowledged_checks.append({
                            "id": check.id,
                            "code": check.code,
                            "message": check.message
                        })

                # Execute the action
                action_dict = {
                    "type": action_type,
                }
                # Copy all action attributes
                for attr in dir(action):
                    if not attr.startswith('_') and attr != 'type':
                        val = getattr(action, attr, None)
                        if val is not None and not callable(val):
                            action_dict[attr] = val

                result = self._execute_action(action_dict)

                if not result.screenshot_base64:
                    logger.error(
                        "CUA: Failed to capture screenshot after action")
                    self.state.error = "Failed to capture screenshot"
                    self.state.status = CUAStatus.ERROR
                    break

                # Check if stop requested
                if self._stop_requested:
                    break

                # Send screenshot back to model
                input_data = [{
                    "type": "computer_call_output",
                    "call_id": call_id,
                    "output": {
                        "type": "input_image",
                        "image_url": f"data:image/png;base64,{result.screenshot_base64}"
                    }
                }]

                if acknowledged_checks:
                    input_data[0]["acknowledged_safety_checks"] = acknowledged_checks

                response = self.client.responses.create(
                    model="computer-use-preview",
                    previous_response_id=response.id,
                    tools=[{
                        "type": "computer_use_preview",
                        "display_width": self.DISPLAY_WIDTH,
                        "display_height": self.DISPLAY_HEIGHT,
                        "environment": "ubuntu"
                    }],
                    input=input_data,
                    truncation="auto"
                )

                self.state.response_id = response.id
                self._notify_status()

                # Small delay between iterations
                time.sleep(0.3)

        except Exception as e:
            logger.error(f"CUA loop error: {e}")
            self.state.error = str(e)
            self.state.status = CUAStatus.ERROR
            self._notify_status()
            return

        # Clean finish
        if self._stop_requested:
            self.state.status = CUAStatus.IDLE
            logger.info("CUA: Stopped by user request")
        else:
            self.state.status = CUAStatus.IDLE
            logger.info(
                f"CUA: Completed after {self.state.actions_taken} actions")

        self.state.current_action = ""
        self._notify_status()

    def start(self, goal: str) -> bool:
        """
        Start autonomous mode with the given goal.

        Args:
            goal: The task goal to accomplish

        Returns:
            True if started successfully
        """
        if self.state.status == CUAStatus.RUNNING:
            logger.warning("CUA is already running")
            return False

        if not goal.strip():
            logger.error("Goal cannot be empty")
            return False

        self._stop_requested = False

        # Capture initial screenshot
        initial_screenshot = self._capture_screenshot()

        # Start background thread
        self._thread = threading.Thread(
            target=self._run_loop,
            args=(goal, initial_screenshot),
            daemon=True
        )
        self._thread.start()

        return True

    def stop(self) -> bool:
        """
        Stop autonomous mode.

        Returns:
            True if stop was requested
        """
        if self.state.status != CUAStatus.RUNNING:
            logger.warning("CUA is not running")
            return False

        logger.info("CUA: Stop requested")
        self._stop_requested = True
        self.state.status = CUAStatus.STOPPING
        self._notify_status()

        return True

    def get_state(self) -> dict:
        """Get current state as dictionary."""
        return {
            "status": self.state.status.value,
            "goal": self.state.goal,
            "current_action": self.state.current_action,
            "actions_taken": self.state.actions_taken,
            "last_reasoning": self.state.last_reasoning,
            "error": self.state.error,
            "running_time": (
                time.time() - self.state.started_at
                if self.state.started_at and self.state.status == CUAStatus.RUNNING
                else 0
            )
        }


# Global CUA instance
_cua_instance: Optional[ComputerUseAgent] = None


def get_cua() -> ComputerUseAgent:
    """Get or create the global CUA instance."""
    global _cua_instance
    if _cua_instance is None:
        _cua_instance = ComputerUseAgent()
    return _cua_instance
