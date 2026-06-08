---
name: customize-i3
description: Edit and manage i3 window manager configuration at ~/.config/i3/config. Use when user wants to add/change keybindings, workspaces, exec rules, window assignments, layouts, gaps, bar config, or any i3 setting. Triggers include "add keybinding", "configure i3", "i3 config", "set workspace", "autostart", "floating rules", "i3bar", or editing ~/.config/i3/.
---

# customize-i3

## Config location

`~/.config/i3/config` (fallback: `~/.i3/config`)

Always read the file before editing. After edits: `i3-msg reload` (safe, preserves session) or `i3-msg restart` (full restart, preserves layout).

## Key concepts

| Concept | Syntax |
|---|---|
| Modifier | `set $mod Mod4` (Win) or `Mod1` (Alt) |
| Keybind | `bindsym $mod+key command` |
| Keycode bind | `bindcode $mod+36 command` |
| Variable | `set $name value` |
| Exec on start | `exec --no-startup-id cmd` |
| Exec always | `exec_always --no-startup-id cmd` |
| Reload config | `i3-msg reload` |

## Examples

### Keybindings

```
# Terminal
bindsym $mod+Return exec i3-sensible-terminal

# Kill window
bindsym $mod+Shift+q kill

# App launcher
bindsym $mod+d exec --no-startup-id dmenu_run

# Screenshot
bindsym Print exec --no-startup-id scrot ~/Screenshots/%Y-%m-%d_%H%M%S.png

# Lock screen
bindsym $mod+l exec --no-startup-id i3lock -c 000000
```

### Workspaces

```
# Named workspaces
set $ws1 "1:term"
set $ws2 "2:web"
set $ws3 "3:chat"

bindsym $mod+1 workspace number $ws1
bindsym $mod+Shift+1 move container to workspace number $ws1

# Pin workspace to output
workspace $ws1 output HDMI-1
workspace $ws2 output eDP-1
```

### Window assignment

```
# assign [class] → workspace
assign [class="Firefox"] $ws2
assign [class="Slack"] $ws3

# Float by class/title
for_window [class="Pavucontrol"] floating enable
for_window [title="Picture-in-Picture"] floating enable, sticky enable

# Float all dialogs
for_window [window_role="dialog"] floating enable
```

### Layouts & gaps (i3-gaps)

```
# Default layout: default | tabbed | stacking
workspace_layout tabbed

# Gaps (requires i3-gaps)
gaps inner 10
gaps outer 5
smart_gaps on
smart_borders on
```

### Binding modes

```
# Resize mode
mode "resize" {
    bindsym h resize shrink width 10 px or 10 ppt
    bindsym l resize grow width 10 px or 10 ppt
    bindsym k resize shrink height 10 px or 10 ppt
    bindsym j resize grow height 10 px or 10 ppt
    bindsym Return mode "default"
    bindsym Escape mode "default"
}
bindsym $mod+r mode "resize"
```

### Bar (i3bar / i3status)

```
bar {
    position top
    status_command i3status
    font pango:JetBrains Mono 10
    colors {
        background #1e1e2e
        statusline #cdd6f4
        focused_workspace  #89b4fa #89b4fa #1e1e2e
        inactive_workspace #313244 #313244 #cdd6f4
    }
}
```

### Colors

```
# class                 border  bg      text    indicator child_border
client.focused          #89b4fa #89b4fa #1e1e2e #f38ba8   #89b4fa
client.unfocused        #313244 #313244 #cdd6f4 #313244   #313244
client.urgent           #f38ba8 #f38ba8 #1e1e2e #f38ba8   #f38ba8
```

## Workflow

1. Read `~/.config/i3/config`
2. Make targeted edit (preserve existing style/comments)
3. Validate syntax: `i3 -C -c ~/.config/i3/config`
4. Apply: `i3-msg reload`
5. If restart needed: `i3-msg restart`

## Useful commands

```bash
# Find window class for assignment rules
xprop | grep WM_CLASS

# List current keybindings
i3-msg -t get_binding_modes

# Query tree
i3-msg -t get_tree | python3 -m json.tool | less
```

## Reference

Full docs: https://i3wm.org/docs/userguide.html
