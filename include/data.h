/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * (c) 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * include/data.h: This file defines all data structures used by i3
 *
 */
#include <xcb/xcb.h>
#include <stdbool.h>

#ifndef _DATA_H
#define _DATA_H
#include "queue.h"

/*
 * To get the big concept: There are helper structures like struct Colorpixel or
 * struct Stack_Window. Everything which is also defined as type (see forward definitions)
 * is considered to be a major structure, thus important.
 *
 * Let’s start from the biggest to the smallest:
 * - An i3Screen is a virtual screen (Xinerama). This can be a single one, though two monitors
 *   might be connected, if you’re running clone mode. There can also be multiple of them.
 *
 * - Each i3Screen contains Workspaces. The concept is known from various other window managers.
 *   Basically, a workspace is a specific set of windows, usually grouped thematically (irc,
 *   www, work, …). You can switch between these.
 *
 * - Each Workspace has a table, which is our layout abstraction. You manage your windows
 *   by moving them around in your table. It grows as necessary.
 *
 * - Each cell of the table has a container, which can be in default or stacking mode. In default
 *   mode, each client is given equally much space in the container. In stacking mode, only one
 *   client is shown at a time, but all the titlebars are rendered at the top.
 *
 * - Inside the container are clients, which is X11-speak for a window.
 *
 */

/* Forward definitions */
typedef struct Cell Cell;
typedef struct Font i3Font;
typedef struct Container Container;
typedef struct Client Client;
typedef struct Binding Binding;
typedef struct Workspace Workspace;
typedef struct Rect Rect;
typedef struct Screen i3Screen;

/******************************************************************************
 * Helper types
 *****************************************************************************/
typedef enum { D_LEFT, D_RIGHT, D_UP, D_DOWN } direction_t;

enum {
        BIND_NONE = 0,
        BIND_SHIFT = XCB_MOD_MASK_SHIFT,        /* (1 << 0) */
        BIND_CONTROL = XCB_MOD_MASK_CONTROL,    /* (1 << 2) */
        BIND_MOD1 = XCB_MOD_MASK_1,             /* (1 << 3) */
        BIND_MOD2 = XCB_MOD_MASK_2,             /* (1 << 4) */
        BIND_MOD3 = XCB_MOD_MASK_3,             /* (1 << 5) */
        BIND_MOD4 = XCB_MOD_MASK_4,             /* (1 << 6) */
        BIND_MOD5 = XCB_MOD_MASK_5,             /* (1 << 7) */
        BIND_MODE_SWITCH = (1 << 8)
};

struct Rect {
        uint32_t x, y;
        uint32_t width, height;
};

/*
 * Defines a position in the table
 *
 */
struct Cell {
        int row;
        int column;
};

/*
 * Used for the cache of colorpixels.
 *
 */
struct Colorpixel {
        uint32_t pixel;

        char *hex;

        SLIST_ENTRY(Colorpixel) colorpixels;
};

/*
 * Contains data for the windows needed to draw the titlebars on in stacking mode
 *
 */
struct Stack_Window {
        xcb_window_t window;
        xcb_gcontext_t gc;
        uint32_t width, height;

        /* Backpointer to the container this stack window is in */
        Container *container;

        SLIST_ENTRY(Stack_Window) stack_windows;
};


/******************************************************************************
 * Major types
 *****************************************************************************/

/*
 * The concept of Workspaces is known from various other window managers. Basically,
 * a workspace is a specific set of windows, usually grouped thematically (irc,
 * www, work, …). You can switch between these.
 *
 */
struct Workspace {
        /* x, y, width, height */
        Rect rect;

        /* table dimensions */
        int cols;
        int rows;

        /* These are stored here only while this workspace is _not_ shown (see show_workspace()) */
        int current_row;
        int current_col;

        Client *fullscreen_client;

        /* Contains all clients with _NET_WM_WINDOW_TYPE == _NET_WM_WINDOW_TYPE_DOCK */
        SLIST_HEAD(dock_clients_head, Client) dock_clients;

        /* Backpointer to the screen this workspace is on */
        i3Screen *screen;

        /* This is a two-dimensional dynamic array of Container-pointers. I’ve always wanted
         * to be a three-star programmer :) */
        Container ***table;
};

/*
 * Holds a keybinding, consisting of a keycode combined with modifiers and the command
 * which is executed as soon as the key is pressed (see src/command.c)
 *
 */
struct Binding {
        /* Keycode to bind */
        uint32_t keycode;
        /* Bitmask consisting of BIND_MOD_1, BIND_MODE_SWITCH, … */
        uint32_t mods;
        /* Command, like in command mode */
        char *command;

        TAILQ_ENTRY(Binding) bindings;
};

/*
 * Data structure for cached font information:
 * - font id in X11 (load it once)
 * - font height (multiple calls needed to get it)
 *
 */
struct Font {
        /* The name of the font, that is what the pattern resolves to */
        char *name;
        /* A copy of the pattern to build a cache */
        char *pattern;
        /* The height of the font, built from font_ascent + font_descent */
        int height;
        /* The xcb-id for the font */
        xcb_font_t id;

        TAILQ_ENTRY(Font) fonts;
};

/*
 * A client is X11-speak for a window.
 *
 */
struct Client {
        /* if you set a client to floating and set it back to managed, it does remember its old
           position and *tries* to get back there */
        Cell old_position;

        /* Backpointer. A client is inside a container */
        Container *container;

        /* x, y, width, height of the frame */
        Rect rect;
        /* x, y, width, height of the child (relative to its frame) */
        Rect child_rect;

        /* Height which was determined by reading the _NET_WM_STRUT_PARTIAL top/bottom of the screen
           reservation */
        int desired_height;

        /* Name */
        char *name;
        int name_len;

        /* fullscreen is pretty obvious */
        bool fullscreen;

        /* Ensure TITLEBAR_TOP maps to 0 because we use calloc for initialization later */
        enum { TITLEBAR_TOP = 0, TITLEBAR_LEFT, TITLEBAR_RIGHT, TITLEBAR_BOTTOM, TITLEBAR_OFF } titlebar_position;

        /* If a client is set as a dock, it is placed at the very bottom of the screen and its
           requested size is used */
        bool dock;

        /* After leaving fullscreen mode, a client needs to be reconfigured (configuration =
           setting X, Y, width and height). By setting the force_reconfigure flag, render_layout()
           will reconfigure the client. */
        bool force_reconfigure;

        /* When reparenting a window, an unmap-notify is sent. As we delete windows when they’re
           unmapped, we need to ignore that one. Therefore, this flag is set when reparenting. */
        bool awaiting_useless_unmap;

        /* XCB contexts */
        xcb_window_t frame;             /* Our window: The frame around the client */
        xcb_gcontext_t titlegc;         /* The titlebar’s graphic context inside the frame */
        xcb_window_t child;             /* The client’s window */

        /* Cache of colorpixels for this client */
        /* TODO: Couldn’t we move them outside here, as they should only depend on the
           root window? */
        SLIST_HEAD(colorpixel_head, Colorpixel) colorpixels;

        /* The following entry provides the necessary list pointers to use Client with LIST_* macros */
        CIRCLEQ_ENTRY(Client) clients;
        SLIST_ENTRY(Client) dock_clients;
};

/*
 * A container is either in default or stacking mode. It sits inside each cell of the table.
 *
 */
struct Container {
        /* Those are speaking for themselves: */
        Client *currently_focused;
        int colspan;
        int rowspan;

        /* Position of the container inside our table */
        int row;
        int col;
        /* Xinerama: X/Y of the container */
        int x;
        int y;
        /* Width/Height of the container. Changeable by the user */
        int width;
        int height;
        /* width_factor and height_factor contain the amount of space (percentage) a window
           has of all the space which is available for resized windows. This ensures that
           non-resized windows (newly opened, for example) have the same size as always */
        float width_factor;
        float height_factor;

        /* When in stacking mode, we draw the titlebars of each client onto a separate window */
        struct Stack_Window stack_win;

        /* Backpointer to the workspace this container is in */
        Workspace *workspace;

        /* Ensure MODE_DEFAULT maps to 0 because we use calloc for initialization later */
        enum { MODE_DEFAULT = 0, MODE_STACK } mode;
        CIRCLEQ_HEAD(client_head, Client) clients;
};

/*
 * This is a virtual screen (Xinerama). This can be a single one, though two monitors
 * might be connected, if you’re running clone mode. There can also be multiple of them.
 *
 */
struct Screen {
        /* Virtual screen number */
        int num;

        /* Current workspace selected on this virtual screen */
        int current_workspace;

        /* x, y, width, height */
        Rect rect;

        TAILQ_ENTRY(Screen) screens;
};

#endif
