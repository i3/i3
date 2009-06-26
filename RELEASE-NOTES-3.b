Release notes for i3 v3.β
-----------------------------

This is the second version (3.β, transcribed 3.b) of i3. It is considered stable.

The most important change probably is the implementation of floating clients,
primarily useful for dialog/toolbar/popup/splash windows. When using i3 for
managing floating windows other than the ones mentioned beforehand, please
keep in mind that i3 is a tiling window manager in the first place and thus
you might better use a "traditional" window manager when having to deal a
lot with floating windows.

Now that you’re warned, let’s have a quick glance at the other new features:
  * jumping to other windows by specifying their position or window class/title
  * assigning clients to specific workspaces by window class/title
  * automatically starting programs (such as i3status + dzen2)
  * configurable colors
  * variables in configfile

Furthermore, we now have a user’s guide which should be the first document
you read when new to i3 (apart from the manpage).

Thanks for this release go out to mist, Atsutane, ch3ka, urs, Moredread,
badboy and all other people who reported bugs/made suggestions.

A list of changes follows:

  * Bugfix: Correctly handle col-/rowspanned containers when setting focus.
  * Bugfix: Correctly handle col-/rowspanned containers when snapping.
  * Bugfix: Force reconfiguration of all windows on workspaces which are
    re-assigned because a screen was detached.
  * Bugfix: Several bugs in resizing table columns fixed.
  * Bugfix: Resizing should now work correctly in all cases.
  * Bugfix: Correctly re-assign dock windows when workspace is destroyed.
  * Bugfix: Correctly handle Mode_switch modifier.
  * Bugfix: Don't raise clients in fullscreen mode.
  * Bugfix: Re-assign dock windows to different workspaces when a workspace
    is detached.
  * Bugfix: Fix crash because of workspace-pointer which did not get updated
  * Bugfix: Correctly initialize screen when Xinerama is disabled.
  * Bugfix: Fullscreen window movement and focus problems fixed
  * Implement jumping to other windows by specifying their position or
    window class/title.
  * Implement jumping back by using the focus stack.
  * Implement autostart (exec-command in configuration file).
  * Implement floating.
  * Implement automatically assigning clients on specific workspaces.
  * Implement variables in configfile.
  * Colors are now configurable.

-- Michael Stapelberg, 2009-06-21
