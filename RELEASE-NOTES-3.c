Release notes for i3 v3.γ
-----------------------------

This is the third version (3.γ, transcribed 3.c) of i3. It is considered stable.

This release contains many small improvements like using keysymbols in the
configuration file, named workspaces, borderless windows, an IPC interface
etc. (see below for a complete list of changes)

Thanks for this release go out to bapt, badboy, Atsutane, tsdh, xeen, mxf,
and all other people who reported bugs/made suggestions.

Special thanks go to steckdenis, yellowiscool and farvardin who designed a logo
for i3.

A list of changes follows:

  * Implement a reload command
  * Implement keysymbols in configuration file
  * Implement assignments of workspaces to screens
  * Implement named workspaces
  * Implement borderless/1-px-border windows
  * Implement command to focus screens
  * Implement IPC via unix sockets
  * Correctly render decoration of floating windows
  * Map floating windows requesting (0x0) to center of their leader/workspace
  * Optimization: Render stack windows on pixmaps to reduce flickering
  * Optimization: Directly position new windows to their final position
  * Bugfix: Repeatedly try to find screens if none are available
  * Bugfix: Correctly redecorate clients when changing focus
  * Bugfix: Don’t crash when clients reconfigure themselves
  * Bugfix: Fix screen wrapping
  * Bugfix: Fix selecting a different screen with your mouse when not having
    any windows on the current workspace
  * Bugfix: Correctly unmap stack windows and don’t re-map them too early
  * Bugfix: Allow switching layout if there are no clients in the this container
  * Bugfix: Set WM_STATE_WITHDRAWN when unmapping, unmap windows when
    destroying
  * Bugfix: Don’t hide assigned clients to inactive but visible workspaces

-- Michael Stapelberg, 2009-08-19
