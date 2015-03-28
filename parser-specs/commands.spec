# vim:ts=2:sw=2:expandtab
#
# i3 - an improved dynamic tiling window manager
# © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
#
# parser-specs/commands.spec: Specification file for generate-command-parser.pl
# which will generate the appropriate header files for our C parser.
#
# Use :source highlighting.vim in vim to get syntax highlighting
# for this file.

state INITIAL:
  # We have an end token here for all the commands which just call some
  # function without using an explicit 'end' token.
  end ->
  '[' -> call cmd_criteria_init(); CRITERIA
  'move' -> MOVE
  'exec' -> EXEC
  'exit' -> call cmd_exit()
  'restart' -> call cmd_restart()
  'reload' -> call cmd_reload()
  'shmlog' -> SHMLOG
  'debuglog' -> DEBUGLOG
  'border' -> BORDER
  'layout' -> LAYOUT
  'append_layout' -> APPEND_LAYOUT
  'workspace' -> WORKSPACE
  'focus' -> FOCUS
  'kill' -> KILL
  'open' -> call cmd_open()
  'fullscreen' -> FULLSCREEN
  'split' -> SPLIT
  'floating' -> FLOATING
  'mark' -> MARK
  'unmark' -> UNMARK
  'resize' -> RESIZE
  'rename' -> RENAME
  'nop' -> NOP
  'scratchpad' -> SCRATCHPAD
  'mode' -> MODE
  'bar' -> BAR

state CRITERIA:
  ctype = 'class' -> CRITERION
  ctype = 'instance' -> CRITERION
  ctype = 'window_role' -> CRITERION
  ctype = 'con_id' -> CRITERION
  ctype = 'id' -> CRITERION
  ctype = 'con_mark' -> CRITERION
  ctype = 'title' -> CRITERION
  ctype = 'urgent' -> CRITERION
  ']' -> call cmd_criteria_match_windows(); INITIAL

state CRITERION:
  '=' -> CRITERION_STR

state CRITERION_STR:
  cvalue = word
      -> call cmd_criteria_add($ctype, $cvalue); CRITERIA

# exec [--no-startup-id] <command>
state EXEC:
  nosn = '--no-startup-id'
      ->
  command = string
      -> call cmd_exec($nosn, $command)

# shmlog <size>|toggle|on|off
state SHMLOG:
  # argument may be a number
  argument = string
    -> call cmd_shmlog($argument)

# debuglog toggle|on|off
state DEBUGLOG:
  argument = 'toggle', 'on', 'off'
    -> call cmd_debuglog($argument)

# border normal|none|1pixel|toggle|1pixel
state BORDER:
  border_style = 'normal', 'pixel'
    -> BORDER_WIDTH
  border_style = 'none', 'toggle'
    -> call cmd_border($border_style, "0")
  border_style = '1pixel'
    -> call cmd_border($border_style, "1")

state BORDER_WIDTH:
  end
    -> call cmd_border($border_style, "2")
  border_width = word
    -> call cmd_border($border_style, $border_width)

# layout default|stacked|stacking|tabbed|splitv|splith
# layout toggle [split|all]
state LAYOUT:
  layout_mode = 'default', 'stacked', 'stacking', 'tabbed', 'splitv', 'splith'
      -> call cmd_layout($layout_mode)
  'toggle'
      -> LAYOUT_TOGGLE

# layout toggle [split|all]
state LAYOUT_TOGGLE:
  end
      -> call cmd_layout_toggle($toggle_mode)
  toggle_mode = 'split', 'all'
      -> call cmd_layout_toggle($toggle_mode)

# append_layout <path>
state APPEND_LAYOUT:
  path = string -> call cmd_append_layout($path)

# workspace next|prev|next_on_output|prev_on_output
# workspace back_and_forth
# workspace <name>
# workspace number <number>
state WORKSPACE:
  direction = 'next_on_output', 'prev_on_output', 'next', 'prev'
      -> call cmd_workspace($direction)
  'back_and_forth'
      -> call cmd_workspace_back_and_forth()
  'number'
      -> WORKSPACE_NUMBER
  workspace = string 
      -> call cmd_workspace_name($workspace)

state WORKSPACE_NUMBER:
  workspace = string
      -> call cmd_workspace_number($workspace)

# focus left|right|up|down
# focus output <output>
# focus tiling|floating|mode_toggle
# focus parent|child
# focus
state FOCUS:
  direction = 'left', 'right', 'up', 'down'
      -> call cmd_focus_direction($direction)
  'output'
      -> FOCUS_OUTPUT
  window_mode = 'tiling', 'floating', 'mode_toggle'
      -> call cmd_focus_window_mode($window_mode)
  level = 'parent', 'child'
      -> call cmd_focus_level($level)
  end
      -> call cmd_focus()

state FOCUS_OUTPUT:
  output = string
      -> call cmd_focus_output($output)

# kill [window|client]
state KILL:
  kill_mode = 'window', 'client'
      -> call cmd_kill($kill_mode)
  end
      -> call cmd_kill($kill_mode)

# fullscreen enable|toggle [global]
# fullscreen disable
# fullscreen [global]
state FULLSCREEN:
  action = 'disable'
      -> call cmd_fullscreen($action, "output")
  action = 'enable', 'toggle'
      -> FULLSCREEN_MODE
  action = ''
      -> FULLSCREEN_COMPAT

state FULLSCREEN_MODE:
  mode = 'global'
      -> call cmd_fullscreen($action, $mode)
  end
      -> call cmd_fullscreen($action, "output")

state FULLSCREEN_COMPAT:
  mode = 'global'
      -> call cmd_fullscreen("toggle", $mode)
  end
      -> call cmd_fullscreen("toggle", "output")

# split v|h|vertical|horizontal
state SPLIT:
  direction = 'horizontal', 'vertical', 'v', 'h'
      -> call cmd_split($direction)

# floating enable|disable|toggle
state FLOATING:
  floating = 'enable', 'disable', 'toggle'
      -> call cmd_floating($floating)

# mark [--toggle] <mark>
state MARK:
  toggle = '--toggle'
      ->
  mark = string
      -> call cmd_mark($mark, $toggle)

# unmark [mark]
state UNMARK:
  end
      -> call cmd_unmark($mark)
  mark = string
      -> call cmd_unmark($mark)

# resize
state RESIZE:
  way = 'grow', 'shrink'
      -> RESIZE_DIRECTION

state RESIZE_DIRECTION:
  direction = 'up', 'down', 'left', 'right', 'width', 'height'
      -> RESIZE_PX

state RESIZE_PX:
  resize_px = word
      -> RESIZE_TILING
  end
      -> call cmd_resize($way, $direction, "10", "10")

state RESIZE_TILING:
  'px'
      ->
  'or'
      -> RESIZE_TILING_OR
  end
      -> call cmd_resize($way, $direction, $resize_px, "10")

state RESIZE_TILING_OR:
  resize_ppt = word
      -> RESIZE_TILING_FINAL

state RESIZE_TILING_FINAL:
  'ppt', end
      -> call cmd_resize($way, $direction, $resize_px, $resize_ppt)

# rename workspace <name> to <name>
# rename workspace to <name>
state RENAME:
  'workspace'
      -> RENAME_WORKSPACE

state RENAME_WORKSPACE:
  old_name = 'to'
      -> RENAME_WORKSPACE_LIKELY_TO
  old_name = word
      -> RENAME_WORKSPACE_TO

state RENAME_WORKSPACE_LIKELY_TO:
  'to'
      -> RENAME_WORKSPACE_NEW_NAME
  new_name = word
      -> call cmd_rename_workspace(NULL, $new_name)

state RENAME_WORKSPACE_TO:
  'to'
      -> RENAME_WORKSPACE_NEW_NAME

state RENAME_WORKSPACE_NEW_NAME:
  end
      -> call cmd_rename_workspace(NULL, "to")
  new_name = string
      -> call cmd_rename_workspace($old_name, $new_name)

# move <direction> [<pixels> [px]]
# move [window|container] [to] workspace [<str>|next|prev|next_on_output|prev_on_output|current]
# move [window|container] [to] output <str>
# move [window|container] [to] scratchpad
# move workspace to [output] <str>
# move scratchpad
# move [window|container] [to] [absolute] position [ [<pixels> [px] <pixels> [px]] | center ]
state MOVE:
  'window'
      ->
  'container'
      ->
  'to'
      ->
  'workspace'
      -> MOVE_WORKSPACE
  'output'
      -> MOVE_TO_OUTPUT
  'scratchpad'
      -> call cmd_move_scratchpad()
  direction = 'left', 'right', 'up', 'down'
      -> MOVE_DIRECTION
  method = 'position'
      -> MOVE_TO_POSITION
  method = 'absolute'
      -> MOVE_TO_ABSOLUTE_POSITION

state MOVE_DIRECTION:
  pixels = word
      -> MOVE_DIRECTION_PX
  end
      -> call cmd_move_direction($direction, "10")

state MOVE_DIRECTION_PX:
  'px'
      -> call cmd_move_direction($direction, $pixels)
  end
      -> call cmd_move_direction($direction, $pixels)

state MOVE_WORKSPACE:
  'to '
      -> MOVE_WORKSPACE_TO_OUTPUT
  workspace = 'next_on_output', 'prev_on_output', 'next', 'prev', 'current'
      -> call cmd_move_con_to_workspace($workspace)
  'back_and_forth'
      -> call cmd_move_con_to_workspace_back_and_forth()
  'number'
      -> MOVE_WORKSPACE_NUMBER
  workspace = string
      -> call cmd_move_con_to_workspace_name($workspace)

state MOVE_WORKSPACE_NUMBER:
  number = string
      -> call cmd_move_con_to_workspace_number($number)

state MOVE_TO_OUTPUT:
  output = string
      -> call cmd_move_con_to_output($output)

state MOVE_WORKSPACE_TO_OUTPUT:
  'output'
      ->
  output = string
      -> call cmd_move_workspace_to_output($output)

state MOVE_TO_ABSOLUTE_POSITION:
  'position'
      -> MOVE_TO_POSITION

state MOVE_TO_POSITION:
  'center'
      -> call cmd_move_window_to_center($method)
  coord_x = word
      -> MOVE_TO_POSITION_X

state MOVE_TO_POSITION_X:
  'px'
      ->
  coord_y = word
      -> MOVE_TO_POSITION_Y

state MOVE_TO_POSITION_Y:
  'px', end
      -> call cmd_move_window_to_position($method, $coord_x, $coord_y)

# mode <string>
state MODE:
  mode = string
      -> call cmd_mode($mode)

state NOP:
  comment = string
      -> call cmd_nop($comment)
  end
      -> call cmd_nop(NULL)

state SCRATCHPAD:
  'show'
      -> call cmd_scratchpad_show()

# bar (hidden_state hide|show|toggle)|(mode dock|hide|invisible|toggle) [<bar_id>]
state BAR:
  bar_type = 'hidden_state'
      -> BAR_HIDDEN_STATE
  bar_type = 'mode'
      -> BAR_MODE

state BAR_HIDDEN_STATE:
  bar_value = 'hide', 'show', 'toggle'
      -> BAR_W_ID

state BAR_MODE:
  bar_value = 'dock', 'hide', 'invisible', 'toggle'
      -> BAR_W_ID

state BAR_W_ID:
  bar_id = word
      ->
  end
      -> call cmd_bar($bar_type, $bar_value, $bar_id)
