Release notes for i3 v3.ε
-----------------------------

This is the fifth version (3.ε, transcribed 3.e) of i3. It is considered
stable.

A really big change in this release is the support of RandR instead of
Xinerama. The Xinerama API is a subset of RandR and its limitations clearly
showed when you reconfigured outputs using xrandr(1) during runtime (it was
not designed to handle such changes). The implementation of RandR fixes some
long-standing bugs (workspaces were messed up when reconfiguring outputs)
and cleans up some code. Furthermore, you are now able to assign workspaces
to outputs (like LVDS1, VGA1, …) instead of the formerly used heuristics
like "the screen at position (x, y)" or "the second screen in the list".

Furthermore, another big change is the separation of debug output (the
so-called logfile): you now need to enable verbose output (parameter -V)
and you need to specify which (if any) debug output you want to see (parameter
-d <loglevels>). When starting without -V, i3 will only log errors. This is
what you usually want for a production system. When enabling verbose output,
you will see the names and window classes of new windows (useful for creating
assignments in your configuration file) and other useful messages. For an
explanation of the debuglevels, please see the "How to debug" document (for
the impatient: "-d all" gives you full output).

In 3.δ, a new parser/lexer was introduced and available using the -l option.
The old parser/lexer has been removed in the meantime, so in 3.ε, the "new"
parser/lexer is always used and you do not need the -l option anymore. To
make debugging errors in your configuration easier, the error messages have
been very much improved. Also, the parser tries to skip invalid lines (though
it may not always succeed, it usually works and does not crash i3).

Starting from version 3.ε, i3 obeys the XDG base directory specification,
meaning that you can now put your configuration file into ~/.config/i3/config,
which might be useful if you manage your ~/.config directory in some way (git,
…). The old configuration file path is still supported (there are no plans
to change this), but using ~/.config seems reasonable for clean setups.

You can disable the internal workspace bar in this release. Instead of the
internal bar, you can use dzen2 (or similar) in dock mode (-dock for dzen2,
but you need an svn revision). The sample implementation i3-wsbar takes
stdin, generates a combined bar (workspaces + stdin) and starts dzen2 on
your outputs as needed (does the right thing when you reconfigure your
monitors dynamically).

To accomplish the external workspace bar feature, the IPC interface has
seen much love: requests and replies now use JSON for serialization of
data structures and provide a nice and simple way to get information (like
the current workspaces or outputs) from i3 or send commands to it. You can
also subscribe to certain types of events (workspace or output changes).
See the AnyEvent::I3 module for a sample implementation of a library.

Thanks for this release go out to Merovius, badboy, xeen, Atsutane, Ciprian,
dirkson, Mirko, sur5r, artoj, Scytale, fallen, Thomas, Sasha, dothebart, msi
and all other people who reported bugs/made suggestions.

A complete list of changes follows:

 * Implement RandR instead of Xinerama
 * Obey the XDG Base Directory Specification for config file paths
 * lexer/parser: proper error messages
 * Add new options -V for verbose mode and -d <loglevel> for debug log levels
 * Implement resize command for floating clients
 * Include date of the last commit in version string
 * Fixed cursor orientation when resizing
 * Added focus_follows_mouse config option
 * Feature: Cycle through workspaces
 * Fix bindings using the cursor keys in default config
 * added popup for handling SIGSEGV or SIGFPE
 * Correctly exit when another window manager is already running
 * Take into account the window’s base_{width,height} when resizing
 * Disable XKB instead of quitting with an error
 * Make containers containing exactly one window behave like default containers
 * Also warp the pointer when moving a window to a another visible workspace
 * work around clients setting 0xFFFF as resize increments
 * Move autostart after creating the IPC socket in start process
 * Restore geometry of all windows before exiting/restarting
 * When in fullscreen mode, focus whole screens instead of denying to focus
 * draw consistent borders for each frame in a tabbed/stacked container
 * Update fullscreen client position/size when an output changes
 * i3-input: Bugfix: repeatedly grab the keyboard if it does not succeed
 * put windows with WM_CLIENT_LEADER on the workspace of their leader
 * use real functions instead of nested functions (enables compilation with
   llvm-clang)
 * implement screen-spanning fullscreen mode
 * floating resize now uses arbitrary corners
 * floating resize now works proportionally when pressing shift
 * Don’t use SYNC key bindings for mode_switch but re-grab keys
 * support PREFIX and SYSCONFDIR in Makefile
 * make pointer follow the focus when moving to a different screen also for
   floating clients
 * start dock clients on the output they request to be started on according
   to their geometry
 * handle destroy notify events like unmap notify events
 * ewmh: correctly set _NET_CURRENT_DESKTOP to the number of the active
   workspace
 * ewmh: correctly set _NET_ACTIVE_WINDOW
 * ewmh: implement support for _NET_WORKAREA (rdesktop can use that)
 * default ipc-socket path is now ~/.i3/ipc.sock, enabled in the default config
 * Bugfix: Containers could lose their snap state
 * Bugfix: Use ev_loop_new to not block SIGCHLD
 * Bugfix: if a font provides no per-char info for width, fall back to default
 * Bugfix: lexer: return to INITIAL state after floating_modifier
 * Bugfix: Don’t leak IPC socket to launched processes
 * Bugfix: Use both parts of WM_CLASS (it contains instance and class)
 * Bugfix: Correctly do boundary checking/moving to other workspaces when
   moving floating clients via keyboard
 * Bugfix: checked for wrong flag in size hints
 * Bugfix: Correctly render workspace names containing some non-ascii chars
 * Bugfix: Correctly position floating windows sending configure requests
 * Bugfix: Don’t remap stack windows errnously when changing workspaces
 * Bugfix: configure floating windows above tiling windows when moving them
   to another workspace
 * Bugfix: Take window out of fullscreen mode before entering floating mode
 * Bugfix: Don’t enter BIND_A2WS_COND state too early
 * Bugfix: only restore focus if the workspace is focused, not if it is visible
 * Bugfix: numlock state will now be filtered in i3-input and signal handler
 * Bugfix: Don’t unmap windows when current workspace gets reassigned
 * Bugfix: correctly translate coordinates for floating windows when outputs
   change
 * Bugfix: Correctly switch workspace when using the "jump" command
 * Bugfix: Fix rendering of workspace names after "reload"
 * Bugfix: Correctly ignore clicks when in fullscreen mode
 * Bugfix: Don’t allow fullscreen floating windows to be moved
 * Bugfix: Don’t render containers which are not visible on hint changes
 * Some memory leaks/invalid accesses have been fixed

 -- Michael Stapelberg, 2010-03-30
