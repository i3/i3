# vim:ts=2:sw=2:expandtab
#
# i3 - an improved dynamic tiling window manager
# © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
#
# parser-specs/config.spec: Specification file for generate-command-parser.pl
# which will generate the appropriate header files for our C parser.
#
# Use :source highlighting.vim in vim to get syntax highlighting
# for this file.

# TODO: get it to parse the default config :)
# TODO: comment handling (on their own line, at the end of a line)

state INITIAL:
  # We have an end token here for all the commands which just call some
  # function without using an explicit 'end' token.
  end ->
  #'[' -> call cmd_criteria_init(); CRITERIA
  'font' -> FONT
  'mode' -> MODENAME
  exectype = 'exec_always', 'exec'
      -> EXEC

# <exec|exec_always> [--no-startup-id] command
state EXEC:
  no_startup_id = '--no-startup-id'
      ->
  command = string
      -> call cfg_exec($exectype, $no_startup_id, $command)

state MODENAME:
  modename = word
      -> call cfg_enter_mode($modename); MODEBRACE

state MODEBRACE:
  '{'
      -> MODE

state MODE:
  bindtype = 'bindsym', 'bindcode'
      -> MODE_BINDING
  '}'
      -> INITIAL

state MODE_BINDING:
  modifiers = 'Mod1', 'Mod2', 'Mod3', 'Mod4', 'Mod5', 'Shift', 'Control'
      ->
  '+'
      ->
  key = word
      -> MODE_BINDCOMMAND

state MODE_BINDCOMMAND:
  command = string
      -> call cfg_mode_binding($bindtype, $modifiers, $key, $command); MODE

state FONT:
  font = string
      -> call cfg_font($font)
