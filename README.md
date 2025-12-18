<p align="center">
  <img src="ai3.png" alt="ai3 Logo" width="280">
</p>

<p align="center">
  <em>What if your window manager could think?</em>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-experimental-orange.svg" alt="Status: Experimental">
  <a href="https://i3wm.org/"><img src="https://img.shields.io/badge/i3wm-compatible-brightgreen.svg" alt="i3wm Compatible"></a>
  <a href="https://openai.com/"><img src="https://img.shields.io/badge/OpenAI-powered-412991.svg" alt="OpenAI Powered"></a>
</p>

<p align="center">
  <a href="#what-is-this">About</a> •
  <a href="#demo">Demo</a> •
  <a href="#features">Features</a> •
  <a href="#installation">Install</a> •
  <a href="#usage">Usage</a>
</p>

---

> **⚠️ EXPERIMENTAL PROJECT - NOT FOR PRODUCTION USE**
>
> This is an experimental research project exploring AI-integrated window management. It is intended for testing and experimentation only. Do not use in production environments or with sensitive data.

## What is this?

ai3 is an experimental AI-powered window manager assistant that integrates OpenAI's language models directly into the [i3 window manager](https://i3wm.org/). This project explores what a **future AI-native operating system or window server** could look like—where AI is deeply embedded into the core desktop experience, not just bolted on as an afterthought.

The window manager communicates with OpenAI's API to understand your desktop context, predict your next actions, optimize window layouts, and respond to natural language commands about your workspace.

### Vision

This project envisions a future where:
- Your window manager **understands intent**, not just commands
- AI can **see and reason** about your entire desktop state
- Natural language replaces memorizing keyboard shortcuts
- The OS **anticipates** what you need before you ask
- Window management becomes a **conversation**, not a configuration file

### Why a fork?

Currently, ai3 works entirely through i3's existing IPC protocol—no modifications to i3 itself are required. We regularly sync with upstream i3 to stay current. However, this repository is maintained as a fork to keep the door open for deeper integration in the future, should we explore changes at the window manager level.

## Demo

![ai3 Demo](ai3demo.gif)

The demo above shows:
- **Layout optimization** (`Ctrl+Shift+O`) — AI reorganizes windows based on your current workflow
- **Chat with function calling** — Natural language conversation with direct access to i3's IPC interface
- **Next action prediction** — AI anticipates your next move based on keyboard input (e.g., suggesting actions after `ls -al`)

> **Note:** Currently all AI requests are sent to OpenAI's API. Support for additional providers (including local models) may be added in the future.

## Features

- **Next Action Prediction**: AI predicts and suggests your next window management action
- **Layout Optimization**: AI analyzes and optimizes your window layout
- **Chat Mode**: Interactive chat with full context of your i3 environment
- **Tools**: All i3 functionality exposed as tools for AI agents
- **i3bar Integration**: AI status displayed in the i3 status bar
- **Screenshot Analysis**: AI can see your screen to provide contextual help

## How It Works

1. The ai3-server runs alongside i3wm and connects via i3's IPC protocol
2. It exposes your desktop state (windows, workspaces, layouts) as context
3. OpenAI agents receive this context and can execute i3 commands as tools
4. You interact via CLI commands, keyboard shortcuts, or the status bar

## Installation

```bash
cd ai3-server
pip install -e .
```

## Configuration

Set your OpenAI API key:

```bash
export OPENAI_KEY="your-api-key"
```

Optional configuration:

```bash
export OPENAI_MODEL="gpt-4.1-mini"       # Default model
export OPENAI_BASE_URL="..."       # Custom API endpoint (optional)
export AI3_SERVER_PORT=7878        # Server port
```

## Usage

### Start the server

```bash
ai3-server
# or
ai3 server start
```

### CLI Commands

```bash
# Get next action suggestion
ai3 suggest

# Optimize layout
ai3 layout optimize
ai3 layout optimize "maximize browser window"

# Chat with AI about your desktop
ai3 chat "how can I organize these windows better?"
ai3 chat  # Interactive mode

# Get current status
ai3 status

# List available tools
ai3 tools
```

### Python API

```python
from ai3_server import AI3Client

client = AI3Client()

# Get suggestion
suggestion = client.suggest()
print(suggestion.description)

# Optimize layout
result = client.optimize_layout("focus on coding")

# Chat
response = client.chat("What windows do I have open?")
```

## Architecture

```
ai3-server/
├── src/ai3_server/
│   ├── __init__.py
│   ├── cli.py           # CLI interface
│   ├── server.py        # FastAPI server
│   ├── config.py        # Configuration
│   ├── i3/
│   │   ├── __init__.py
│   │   ├── connection.py    # i3ipc wrapper
│   │   └── tools.py         # tools for i3
│   ├── agents/
│   │   ├── __init__.py
│   │   ├── base.py          # Base agent class
│   │   ├── suggest.py       # Suggestion agent
│   │   ├── layout.py        # Layout optimizer agent
│   │   └── chat.py          # Chat agent
│   └── models/
│       ├── __init__.py
│       └── schemas.py       # Pydantic models
```

## Tools

The following i3 operations are exposed as tools:

- `get_workspaces` - List all workspaces
- `get_windows` - List all windows with details
- `get_focused_window` - Get currently focused window
- `get_tree` - Get full container tree
- `focus_window` - Focus a specific window
- `move_window` - Move window to workspace/direction
- `resize_window` - Resize a window
- `set_layout` - Change container layout
- `run_command` - Execute any i3 command
- `get_outputs` - Get display outputs
- `screenshot` - Capture current screen

## License

MIT
