# vim:ts=2:sw=2:expandtab
#
# i3 - an improved dynamic tiling window manager
# © 2009 Michael Stapelberg and contributors (see also: LICENSE)
#
# parser-specs/config.spec: Specification file for generate-command-parser.pl
# which will generate the appropriate header files for our C parser.
#
# Use :source highlighting.vim in vim to get syntax highlighting
# for this file.

# TODO: should we implement an include statement for the criteria part so we DRY?

state INITIAL:
  # We have an end token here for all the commands which just call some
  # function without using an explicit 'end' token.
  end ->
  error ->
  '#'                                      -> IGNORE_LINE
  'set'                                    -> IGNORE_LINE
  bindtype = 'bindsym', 'bindcode', 'bind' -> BINDING
  'bar'                                    -> BARBRACE
  'font'                                   -> FONT
  'mode'                                   -> MODENAME
  'floating_minimum_size'                  -> FLOATING_MINIMUM_SIZE_WIDTH
  'floating_maximum_size'                  -> FLOATING_MAXIMUM_SIZE_WIDTH
  'floating_modifier'                      -> FLOATING_MODIFIER
  'default_orientation'                    -> DEFAULT_ORIENTATION
  'workspace_layout'                       -> WORKSPACE_LAYOUT
  windowtype = 'new_window', 'new_float'   -> NEW_WINDOW
  'hide_edge_borders'                      -> HIDE_EDGE_BORDERS
  'for_window'                             -> FOR_WINDOW
  'assign'                                 -> ASSIGN
  'no_focus'                               -> NO_FOCUS
  'focus_follows_mouse'                    -> FOCUS_FOLLOWS_MOUSE
  'mouse_warping'                          -> MOUSE_WARPING
  'force_focus_wrapping'                   -> FORCE_FOCUS_WRAPPING
  'force_xinerama', 'force-xinerama'       -> FORCE_XINERAMA
  'workspace_auto_back_and_forth'          -> WORKSPACE_BACK_AND_FORTH
  'fake_outputs', 'fake-outputs'           -> FAKE_OUTPUTS
  'force_display_urgency_hint'             -> FORCE_DISPLAY_URGENCY_HINT
  'delay_exit_on_zero_displays'            -> DELAY_EXIT_ON_ZERO_DISPLAYS
  'focus_on_window_activation'             -> FOCUS_ON_WINDOW_ACTIVATION
  'show_marks'                             -> SHOW_MARKS
  'workspace'                              -> WORKSPACE
  'ipc_socket', 'ipc-socket'               -> IPC_SOCKET
  'restart_state'                          -> RESTART_STATE
  'popup_during_fullscreen'                -> POPUP_DURING_FULLSCREEN
  exectype = 'exec_always', 'exec'         -> EXEC
  colorclass = 'client.background'
      -> COLOR_SINGLE
  colorclass = 'client.focused_inactive', 'client.focused', 'client.unfocused', 'client.urgent', 'client.placeholder'
      -> COLOR_BORDER

# We ignore comments and 'set' lines (variables).
state IGNORE_LINE:
  line
      -> INITIAL

# floating_minimum_size <width> x <height>
state FLOATING_MINIMUM_SIZE_WIDTH:
  width = number
      -> FLOATING_MINIMUM_SIZE_X

state FLOATING_MINIMUM_SIZE_X:
  'x'
      -> FLOATING_MINIMUM_SIZE_HEIGHT

state FLOATING_MINIMUM_SIZE_HEIGHT:
  height = number
      -> call cfg_floating_minimum_size(&width, &height)

# floating_maximum_size <width> x <height>
state FLOATING_MAXIMUM_SIZE_WIDTH:
  width = number
      -> FLOATING_MAXIMUM_SIZE_X

state FLOATING_MAXIMUM_SIZE_X:
  'x'
      -> FLOATING_MAXIMUM_SIZE_HEIGHT

state FLOATING_MAXIMUM_SIZE_HEIGHT:
  height = number
      -> call cfg_floating_maximum_size(&width, &height)

# floating_modifier <modifier>
state FLOATING_MODIFIER:
  modifiers = 'Mod1', 'Mod2', 'Mod3', 'Mod4', 'Mod5', 'Shift', 'Control', 'Ctrl'
      ->
  '+'
      ->
  end
      -> call cfg_floating_modifier($modifiers)

# default_orientation <horizontal|vertical|auto>
state DEFAULT_ORIENTATION:
  orientation = 'horizontal', 'vertical', 'auto'
      -> call cfg_default_orientation($orientation)

# workspace_layout <default|stacking|tabbed>
state WORKSPACE_LAYOUT:
  layout = 'default', 'stacking', 'stacked', 'tabbed'
      -> call cfg_workspace_layout($layout)

# new_window <normal|1pixel|none>
# new_float <normal|1pixel|none>
# TODO: new_float is not in the userguide yet
# TODO: pixel is not in the userguide yet
state NEW_WINDOW:
  border = 'normal', 'pixel'
      -> NEW_WINDOW_PIXELS
  border = '1pixel', 'none'
      -> call cfg_new_window($windowtype, $border, -1)

state NEW_WINDOW_PIXELS:
  end
      -> call cfg_new_window($windowtype, $border, 2)
  width = number
      -> NEW_WINDOW_PIXELS_PX

state NEW_WINDOW_PIXELS_PX:
  'px'
      ->
  end
      -> call cfg_new_window($windowtype, $border, &width)

# hide_edge_borders <none|vertical|horizontal|both>
# also hide_edge_borders <bool> for compatibility
state HIDE_EDGE_BORDERS:
  hide_borders = 'none', 'vertical', 'horizontal', 'both'
      -> call cfg_hide_edge_borders($hide_borders)
  hide_borders = '1', 'yes', 'true', 'on', 'enable', 'active'
      -> call cfg_hide_edge_borders($hide_borders)

# for_window <criteria> command
state FOR_WINDOW:
  '['
      -> call cfg_criteria_init(FOR_WINDOW_COMMAND); CRITERIA

state FOR_WINDOW_COMMAND:
  command = string
      -> call cfg_for_window($command)

# assign <criteria> [→] workspace
state ASSIGN:
  '['
      -> call cfg_criteria_init(ASSIGN_WORKSPACE); CRITERIA

state ASSIGN_WORKSPACE:
  '→'
      ->
  workspace = string
      -> call cfg_assign($workspace)

# no_focus <criteria>
state NO_FOCUS:
  '['
      -> call cfg_criteria_init(NO_FOCUS_END); CRITERIA

state NO_FOCUS_END:
  end
      -> call cfg_no_focus()

# Criteria: Used by for_window and assign.
state CRITERIA:
  ctype = 'class'       -> CRITERION
  ctype = 'instance'    -> CRITERION
  ctype = 'window_role' -> CRITERION
  ctype = 'con_id'      -> CRITERION
  ctype = 'id'          -> CRITERION
  ctype = 'window_type' -> CRITERION
  ctype = 'con_mark'    -> CRITERION
  ctype = 'title'       -> CRITERION
  ctype = 'urgent'      -> CRITERION
  ']'
      -> call cfg_criteria_pop_state()

state CRITERION:
  '=' -> CRITERION_STR

state CRITERION_STR:
  cvalue = word
      -> call cfg_criteria_add($ctype, $cvalue); CRITERIA

# focus_follows_mouse bool
state FOCUS_FOLLOWS_MOUSE:
  value = word
      -> call cfg_focus_follows_mouse($value)

# mouse_warping warping_t
state MOUSE_WARPING:
  value = 'none', 'output'
      -> call cfg_mouse_warping($value)

# force_focus_wrapping
state FORCE_FOCUS_WRAPPING:
  value = word
      -> call cfg_force_focus_wrapping($value)

# force_xinerama
state FORCE_XINERAMA:
  value = word
      -> call cfg_force_xinerama($value)

# workspace_back_and_forth
state WORKSPACE_BACK_AND_FORTH:
  value = word
      -> call cfg_workspace_back_and_forth($value)


# fake_outputs (for testcases)
state FAKE_OUTPUTS:
  outputs = string
      -> call cfg_fake_outputs($outputs)

# force_display_urgency_hint <timeout> ms
state FORCE_DISPLAY_URGENCY_HINT:
  duration_ms = number
      -> FORCE_DISPLAY_URGENCY_HINT_MS

# show_marks
state SHOW_MARKS:
  value = word
      -> call cfg_show_marks($value)

state FORCE_DISPLAY_URGENCY_HINT_MS:
  'ms'
      ->
  end
      -> call cfg_force_display_urgency_hint(&duration_ms)

# delay_exit_on_zero_displays <delay> ms
state DELAY_EXIT_ON_ZERO_DISPLAYS:
  duration_ms = number
      -> DELAY_EXIT_ON_ZERO_DISPLAYS_MS

state DELAY_EXIT_ON_ZERO_DISPLAYS_MS:
  'ms'
      ->
  end
      -> call cfg_delay_exit_on_zero_displays(&duration_ms)

# focus_on_window_activation <smart|urgent|focus|none>
state FOCUS_ON_WINDOW_ACTIVATION:
  mode = word
      -> call cfg_focus_on_window_activation($mode)

# workspace <workspace> output <output>
state WORKSPACE:
  workspace = word
    -> WORKSPACE_OUTPUT

state WORKSPACE_OUTPUT:
  'output'
      -> WORKSPACE_OUTPUT_STR

state WORKSPACE_OUTPUT_STR:
  output = word
      -> call cfg_workspace($workspace, $output)

# ipc-socket <path>
state IPC_SOCKET:
  path = string
      -> call cfg_ipc_socket($path)

# restart_state <path> (for testcases)
state RESTART_STATE:
  path = string
      -> call cfg_restart_state($path)

# popup_during_fullscreen
state POPUP_DURING_FULLSCREEN:
  value = 'ignore', 'leave_fullscreen', 'smart'
      -> call cfg_popup_during_fullscreen($value)

# client.background <hexcolor>
state COLOR_SINGLE:
  color = word
      -> call cfg_color_single($colorclass, $color)

# colorclass border background text indicator
state COLOR_BORDER:
  border = word
      -> COLOR_BACKGROUND

state COLOR_BACKGROUND:
  background = word
      -> COLOR_TEXT

state COLOR_TEXT:
  text = word
      -> COLOR_INDICATOR

state COLOR_INDICATOR:
  indicator = word
      -> call cfg_color($colorclass, $border, $background, $text, $indicator)
  end
      -> call cfg_color($colorclass, $border, $background, $text, NULL)

# <exec|exec_always> [--no-startup-id] command
state EXEC:
  no_startup_id = '--no-startup-id'
      ->
  command = string
      -> call cfg_exec($exectype, $no_startup_id, $command)

# font font
state FONT:
  font = string
      -> call cfg_font($font)

# bindsym/bindcode
state BINDING:
  release = '--release'
      ->
  border = '--border'
      ->
  whole_window = '--whole-window'
      ->
  modifiers = 'Mod1', 'Mod2', 'Mod3', 'Mod4', 'Mod5', 'Shift', 'Control', 'Ctrl', 'Mode_switch', '$mod'
      ->
  '+'
      ->
  key = word
      -> BINDCOMMAND

state BINDCOMMAND:
  release = '--release'
      ->
  border = '--border'
      ->
  whole_window = '--whole-window'
      ->
  command = string
      -> call cfg_binding($bindtype, $modifiers, $key, $release, $border, $whole_window, $command)

################################################################################
# Mode configuration
################################################################################

state MODENAME:
  modename = word
      -> call cfg_enter_mode($modename); MODEBRACE

state MODEBRACE:
  end
      ->
  '{'
      -> MODE

state MODE:
  end ->
  error ->
  '#' -> MODE_IGNORE_LINE
  'set' -> MODE_IGNORE_LINE
  bindtype = 'bindsym', 'bindcode', 'bind'
      -> MODE_BINDING
  '}'
      -> INITIAL

# We ignore comments and 'set' lines (variables).
state MODE_IGNORE_LINE:
  line
      -> MODE

state MODE_BINDING:
  release = '--release'
      ->
  border = '--border'
      ->
  whole_window = '--whole-window'
      ->
  modifiers = 'Mod1', 'Mod2', 'Mod3', 'Mod4', 'Mod5', 'Shift', 'Control', 'Ctrl', 'Mode_switch', '$mod'
      ->
  '+'
      ->
  key = word
      -> MODE_BINDCOMMAND

state MODE_BINDCOMMAND:
  release = '--release'
      ->
  border = '--border'
      ->
  whole_window = '--whole-window'
      ->
  command = string
      -> call cfg_mode_binding($bindtype, $modifiers, $key, $release, $border, $whole_window, $command); MODE

################################################################################
# Bar configuration (i3bar)
################################################################################

state BARBRACE:
  end
      ->
  '{'
      -> BAR

state BAR:
  end ->
  error ->
  '#' -> BAR_IGNORE_LINE
  'set' -> BAR_IGNORE_LINE
  'i3bar_command'          -> BAR_BAR_COMMAND
  'status_command'         -> BAR_STATUS_COMMAND
  'socket_path'            -> BAR_SOCKET_PATH
  'mode'                   -> BAR_MODE
  'hidden_state'           -> BAR_HIDDEN_STATE
  'id'                     -> BAR_ID
  'modifier'               -> BAR_MODIFIER
  'wheel_up_cmd'           -> BAR_WHEEL_UP_CMD
  'wheel_down_cmd'         -> BAR_WHEEL_DOWN_CMD
  'position'               -> BAR_POSITION
  'output'                 -> BAR_OUTPUT
  'tray_output'            -> BAR_TRAY_OUTPUT
  'font'                   -> BAR_FONT
  'separator_symbol'       -> BAR_SEPARATOR_SYMBOL
  'binding_mode_indicator' -> BAR_BINDING_MODE_INDICATOR
  'workspace_buttons'      -> BAR_WORKSPACE_BUTTONS
  'strip_workspace_numbers' -> BAR_STRIP_WORKSPACE_NUMBERS
  'verbose'                -> BAR_VERBOSE
  'colors'                 -> BAR_COLORS_BRACE
  '}'
      -> call cfg_bar_finish(); INITIAL

# We ignore comments and 'set' lines (variables).
state BAR_IGNORE_LINE:
  line
      -> BAR

state BAR_BAR_COMMAND:
  command = string
      -> call cfg_bar_i3bar_command($command); BAR

state BAR_STATUS_COMMAND:
  command = string
      -> call cfg_bar_status_command($command); BAR

state BAR_SOCKET_PATH:
  path = string
      -> call cfg_bar_socket_path($path); BAR

state BAR_MODE:
  mode = 'dock', 'hide', 'invisible'
      -> call cfg_bar_mode($mode); BAR

state BAR_HIDDEN_STATE:
  hidden_state = 'hide', 'show'
      -> call cfg_bar_hidden_state($hidden_state); BAR

state BAR_ID:
  bar_id = word
      -> call cfg_bar_id($bar_id); BAR

state BAR_MODIFIER:
  modifier = 'Mod1', 'Mod2', 'Mod3', 'Mod4', 'Mod5', 'Control', 'Ctrl', 'Shift'
      -> call cfg_bar_modifier($modifier); BAR

state BAR_WHEEL_UP_CMD:
  command = string
      -> call cfg_bar_wheel_up_cmd($command); BAR

state BAR_WHEEL_DOWN_CMD:
  command = string
      -> call cfg_bar_wheel_down_cmd($command); BAR

state BAR_POSITION:
  position = 'top', 'bottom'
      -> call cfg_bar_position($position); BAR

state BAR_OUTPUT:
  output = string
      -> call cfg_bar_output($output); BAR

state BAR_TRAY_OUTPUT:
  output = word
      -> call cfg_bar_tray_output($output); BAR

state BAR_FONT:
  font = string
      -> call cfg_bar_font($font); BAR

state BAR_SEPARATOR_SYMBOL:
  separator = string
      -> call cfg_bar_separator_symbol($separator); BAR

state BAR_BINDING_MODE_INDICATOR:
  value = word
      -> call cfg_bar_binding_mode_indicator($value); BAR

state BAR_WORKSPACE_BUTTONS:
  value = word
      -> call cfg_bar_workspace_buttons($value); BAR

state BAR_STRIP_WORKSPACE_NUMBERS:
  value = word
      -> call cfg_bar_strip_workspace_numbers($value); BAR

state BAR_VERBOSE:
  value = word
      -> call cfg_bar_verbose($value); BAR

state BAR_COLORS_BRACE:
  end
      ->
  '{'
      -> BAR_COLORS

state BAR_COLORS:
  end ->
  '#' -> BAR_COLORS_IGNORE_LINE
  'set' -> BAR_COLORS_IGNORE_LINE
  colorclass = 'background', 'statusline', 'separator'
      -> BAR_COLORS_SINGLE
  colorclass = 'focused_workspace', 'active_workspace', 'inactive_workspace', 'urgent_workspace'
      -> BAR_COLORS_BORDER
  '}'
      -> BAR

# We ignore comments and 'set' lines (variables).
state BAR_COLORS_IGNORE_LINE:
  line
      -> BAR_COLORS

state BAR_COLORS_SINGLE:
  color = word
      -> call cfg_bar_color_single($colorclass, $color); BAR_COLORS

state BAR_COLORS_BORDER:
  border = word
      -> BAR_COLORS_BACKGROUND

state BAR_COLORS_BACKGROUND:
  background = word
      -> BAR_COLORS_TEXT

state BAR_COLORS_TEXT:
  end
      -> call cfg_bar_color($colorclass, $border, $background, NULL); BAR_COLORS
  text = word
      -> call cfg_bar_color($colorclass, $border, $background, $text); BAR_COLORS
