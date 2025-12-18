"""
FastAPI server for ai3.
"""

import logging
import time
from contextlib import asynccontextmanager
from typing import Optional

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware

from ai3_server.agents import ChatAgent, LayoutAgent, SuggestAgent
from ai3_server.agents.cua import get_cua
from ai3_server.config import get_config
from ai3_server.i3 import I3Tools, get_i3, get_i3_tools
from ai3_server.models import ServerStatus, ToolExecuteRequest, ToolExecuteResponse
from ai3_server.routes import agents as agents_routes
from ai3_server.routes import auto as auto_routes
from ai3_server.routes import i3 as i3_routes

# Setup logging to file
logging.basicConfig(
    level=logging.DEBUG,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    handlers=[
        logging.FileHandler("/tmp/ai3-server.log"),
        logging.StreamHandler(),
    ],
)
logger = logging.getLogger(__name__)


# Server state
_start_time: float = 0
_total_requests: int = 0
_suggest_agent: Optional[SuggestAgent] = None
_layout_agent: Optional[LayoutAgent] = None
_chat_agent: Optional[ChatAgent] = None


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan handler."""
    global _start_time, _suggest_agent, _layout_agent, _chat_agent

    # Startup
    _start_time = time.time()
    logger.info("ai3-server starting up...")

    # Initialize agents
    config = get_config()
    tools = get_i3_tools()

    _suggest_agent = SuggestAgent(config, tools)
    _layout_agent = LayoutAgent(config, tools)
    _chat_agent = ChatAgent(config, tools)
    cua = get_cua()

    # Set agents in route modules
    agents_routes.set_agents(_suggest_agent, _layout_agent, _chat_agent)
    auto_routes.set_cua_agent(cua)

    logger.info("ai3-server ready")

    yield

    # Shutdown
    logger.info("ai3-server shutting down...")

    # Stop CUA if running
    if cua and cua.state.status.value == "running":
        cua.stop()


# Create FastAPI app
app = FastAPI(
    title="ai3-server",
    description="AI-powered i3wm assistant with tools",
    version="1.0.0",
    lifespan=lifespan,
)

# Add CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Include routers
app.include_router(i3_routes.router, prefix="/i3")
app.include_router(agents_routes.router)
app.include_router(auto_routes.router)


@app.get("/")
async def root():
    """Root endpoint."""
    return {"name": "ai3-server", "version": "1.0.0", "status": "running"}


@app.get("/status", response_model=ServerStatus)
async def get_status():
    """Get server status."""
    global _total_requests
    _total_requests += 1

    config = get_config()
    i3_connected = False

    try:
        i3 = get_i3()
        i3.get_tree()
        i3_connected = True
    except Exception:
        pass

    agents = []
    for agent, name in [
        (_suggest_agent, "suggest"),
        (_layout_agent, "layout"),
        (_chat_agent, "chat"),
    ]:
        if agent:
            agents.append(
                {
                    "name": name,
                    "running": False,
                    "total_runs": agent.total_runs,
                    "total_tokens": agent.total_tokens,
                    "errors": agent.errors,
                }
            )

    total_tokens = sum(
        a.total_tokens for a in [_suggest_agent, _layout_agent, _chat_agent] if a
    )

    return ServerStatus(
        running=True,
        version="1.0.0",
        uptime_seconds=time.time() - _start_time,
        i3_connected=i3_connected,
        openai_configured=bool(config.openai_api_key),
        agents=agents,
        total_requests=_total_requests,
        total_tokens=total_tokens,
    )


@app.get("/tools")
async def list_tools():
    """List available tools."""
    return {"tools": I3Tools.get_tool_definitions()}


@app.post("/tools/execute", response_model=ToolExecuteResponse)
async def execute_tool(request: ToolExecuteRequest):
    """Execute a tool directly."""
    global _total_requests
    _total_requests += 1

    tools = get_i3_tools()
    result = tools.execute_tool(request.name, request.arguments)

    return ToolExecuteResponse(
        success=result.success,
        data=result.data,
        error=result.error,
    )


def main():
    """Run the server."""
    import uvicorn

    config = get_config()

    uvicorn.run(
        "ai3_server.server:app",
        host=config.server_host,
        port=config.server_port,
        reload=False,
    )


if __name__ == "__main__":
    main()
