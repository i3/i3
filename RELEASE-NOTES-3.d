Release notes for i3 v3.δ
-----------------------------

This is the third version (3.δ, transcribed 3.d) of i3. It is considered stable.

This release features tabbing and some more advanced modifications of the
stacking window (see the user’s guide), vim-like marks, support for the
urgency hint, horizontal resizing of containers (finally), modes (which can
make your keybindings a lot simpler), an unlimited amount of workspaces
and several bugfixes (see below for the complete list of changes).

Furthermore, the configuration file parsing has been rewritten to use a
lex/yacc based lexer/parser. This makes our configuration file more easy to
understand and more flexible from the point of view of a developer. For some
of the new features, you already need the new lexer/parser. To not break your
current configuration, however, the old parser is still included and used by
default. I strongly recommend you to add the flag -l when starting i3 and
switch your configuration file to the new lexer/parser. This should only
require minor changes, if at all. In the next released version of i3, the
old configuration file parsing will be removed!

Also, this release includes the testcases which were developed in a separate
branch so far. They use Perl, together with X11::XCB, which you can download
from CPAN. Please make sure you are not doing anything important when running
the testcases, as they may modify your layout and use different workspaces.
They also might, of course, actually find bugs and crash i3 ;-).

Thanks for this release go out to xeen, mist, badboy, Mikael, mxf, Atsutane,
tsdh, litemotiv, shatter, msi, yurifury, dirkson, Scytale, Grauwolf and all
other people who reported bugs/made suggestions.

A list of changes follows:

  * Implement tabbing (command "T")
  * Implement horizontal resize of containers (containers! not windows)
  * Implement the urgency hint for windows/workspaces
  * Implement vim-like marks (mark/goto command)
  * Implement stack-limit for further defining how stack windows should look
  * Implement modes which allow you to use a different set of keybindings
    when inside a specific mode
  * Implement changing the default mode of containers
  * Implement long options (--version, --no-autostart, --help, --config)
  * Implement 'bt' to toggle between the different border styles
  * Implement an option to specify the default border style
  * Use a yacc/lex parser/lexer for the configuration file
  * The number of workspaces is now dynamic instead of limited to 10
  * Floating windows (and tiled containers) can now be resized using
    floating_modifier and right mouse button
  * Dock windows can now reconfigure their height
  * Bugfix: Correctly handle multiple messages on the IPC socket
  * Bugfix: Correctly use base_width, base_height and size increment hints
  * Bugfix: Correctly send fake configure_notify events
  * Bugfix: Don’t crash if the numlock symbol cannot be found
  * Bugfix: Don’t display a colon after unnamed workspaces
  * Bugfix: If the pointer is outside of the screen when starting, fall back to
    the first screen.
  * Bugfix: Initialize screens correctly when not using Xinerama
  * Bugfix: Correctly handle unmap_notify events when resizing
  * Bugfix: Correctly warp pointer after rendering the layout
  * Bugfix: Fix NULL pointer dereference when reconfiguring screens

 -- Michael Stapelberg, 2009-11-09
