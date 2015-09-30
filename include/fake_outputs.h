/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * Faking outputs is useful in pathological situations (like network X servers
 * which don’t support multi-monitor in a useful way) and for our testsuite.
 *
 */
#pragma once

/**
 * Creates outputs according to the given specification.
 * The specification must be in the format wxh+x+y, for example 1024x768+0+0,
 * with multiple outputs separated by commas:
 *   1900x1200+0+0,1280x1024+1900+0
 *
 */
void fake_outputs_init(const char *output_spec);
