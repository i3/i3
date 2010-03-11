/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * include/data.h: This file defines all data structures used by i3
 *
 */
#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/xcb_atom.h>
#include <stdbool.h>

#ifndef _DATA_H
#define _DATA_H
#include "queue.h"

/*
 * To get the big concept: There are helper structures like struct Colorpixel
 * or struct Stack_Window. Everything which is also defined as type (see
 * forward definitions) is considered to be a major structure, thus important.
 *
 * Let’s start from the biggest to the smallest:
 *
 * - An Output is a physical output on your graphics driver. Outputs which
 *   are currently in use have (output->active == true). Each output has a
 *   position and a mode. An output usually corresponds to one connected
 *   screen (except if you are running multiple screens in clone mode).
 *
 * - Each Output contains Workspaces. The concept is known from various
 *   other window managers.  Basically, a workspace is a specific set of
 *   windows, usually grouped thematically (irc, www, work, …). You can switch
 *   between these.
 *
 * - Each Workspace has a table, which is our layout abstraction. You manage
 *   your windows by moving them around in your table. It grows as necessary.
 *
 * - Each cell of the table has a container, which can be in default or
 *   stacking mode. In default mode, each client is given equally much space
 *   in the container. In stacking mode, only one client is shown at a time,
 *   but all the titlebars are rendered at the top.
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
typedef struct xoutput Output;

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

/**
 * Stores a rectangle, for example the size of a window, the child window etc.
 * It needs to be packed so that the compiler will not add any padding bytes.
 * (it is used in src/ewmh.c for example)
 *
 * Note that x and y can contain signed values in some cases (for example when
 * used for the coordinates of a window, which can be set outside of the
 * visible area, but not when specifying the position of a workspace for the
 * _NET_WM_WORKAREA hint). Not declaring x/y as int32_t saves us a lot of
 * typecasts.
 *
 */
struct Rect {
        uint32_t x;
        uint32_t y;
        uint32_t width;
        uint32_t height;
} __attribute__((packed));

/**
 * Defines a position in the table
 *
 */
struct Cell {
        int row;
        int column;
};

/**
 * Used for the cache of colorpixels.
 *
 */
struct Colorpixel {
        uint32_t pixel;
        char *hex;
        SLIST_ENTRY(Colorpixel) colorpixels;
};

struct Cached_Pixmap {
        xcb_pixmap_t id;

        /* We’re going to paint on it, so a graphics context will be needed */
        xcb_gcontext_t gc;

        /* The rect with which the pixmap was created */
        Rect rect;

        /* The rect of the object to which this pixmap belongs. Necessary to
         * find out when we need to re-create the pixmap. */
        Rect *referred_rect;

        xcb_drawable_t referred_drawable;
};

/**
 * Contains data for the windows needed to draw the titlebars on in stacking
 * mode
 *
 */
struct Stack_Window {
        xcb_window_t window;
        struct Cached_Pixmap pixmap;
        Rect rect;

        /** Backpointer to the container this stack window is in */
        Container *container;

        SLIST_ENTRY(Stack_Window) stack_windows;
};

struct Ignore_Event {
        int sequence;
        time_t added;

        SLIST_ENTRY(Ignore_Event) ignore_events;
};

/**
 * Emulates the behaviour of tables of libxcb-wm, which in libxcb 0.3.4
 * suddenly vanished.
 *
 */
struct keyvalue_element {
        uint32_t key;
        void *value;
        TAILQ_ENTRY(keyvalue_element) elements;
};

/******************************************************************************
 * Major types
 *****************************************************************************/

/**
 * The concept of Workspaces is known from various other window
 * managers. Basically, a workspace is a specific set of windows, usually
 * grouped thematically (irc, www, work, …). You can switch between these.
 *
 */
struct Workspace {
        /** Number of this workspace, starting from 0 */
        int num;

        /** Name of the workspace (in UTF-8) */
        char *utf8_name;

        /** Name of the workspace (in UCS-2) */
        char *name;

        /** Length of the workspace’s name (in glyphs) */
        int name_len;

        /** Width of the workspace’s name (in pixels) rendered in config.font */
        int text_width;

        /** x, y, width, height */
        Rect rect;

        /** table dimensions */
        int cols;
        /** table dimensions */
        int rows;

        /** These are stored here only while this workspace is _not_ shown
         * (see show_workspace()) */
        int current_row;
        /** These are stored here only while this workspace is _not_ shown
         * (see show_workspace()) */
        int current_col;

        /** Should clients on this workspace be automatically floating? */
        bool auto_float;
        /** Are the floating clients on this workspace currently hidden? */
        bool floating_hidden;

        /** The name of the RandR output this screen should be on */
        char *preferred_output;

        /** Temporary flag needed for re-querying xinerama screens */
        bool reassigned;

        /** True if any client on this workspace has its urgent flag set */
        bool urgent;

        /** the client who is started in fullscreen mode on this workspace,
         * NULL if there is none */
        Client *fullscreen_client;

        /** The focus stack contains the clients in the correct order of focus
           so that the focus can be reverted correctly when a client is
           closed */
        SLIST_HEAD(focus_stack_head, Client) focus_stack;

        /** This tail queue contains the floating clients in order of when
         * they were first set to floating (new floating clients are just
         * appended) */
        TAILQ_HEAD(floating_clients_head, Client) floating_clients;

        /** Backpointer to the output this workspace is on */
        Output *output;

        /** This is a two-dimensional dynamic array of
         * Container-pointers. I’ve always wanted to be a three-star
         * programmer :) */
        Container ***table;

        /** width_factor and height_factor contain the amount of space
         * (percentage) a column/row has of all the space which is available
         * for resized windows. This ensures that non-resized windows (newly
         * opened, for example) have the same size as always */
        float *width_factor;
        float *height_factor;

        TAILQ_ENTRY(Workspace) workspaces;
};

/**
 * Holds a keybinding, consisting of a keycode combined with modifiers and the
 * command which is executed as soon as the key is pressed (see src/command.c)
 *
 */
struct Binding {
        /** Symbol the user specified in configfile, if any. This needs to be
         * stored with the binding to be able to re-convert it into a keycode
         * if the keyboard mapping changes (using Xmodmap for example) */
        char *symbol;

        /** Only in use if symbol != NULL. Gets set to the value to which the
         * symbol got translated when binding. Useful for unbinding and
         * checking which binding was used when a key press event comes in.
         *
         * This is an array of number_keycodes size. */
        xcb_keycode_t *translated_to;

        uint32_t number_keycodes;

        /** Keycode to bind */
        uint32_t keycode;

        /** Bitmask consisting of BIND_MOD_1, BIND_MODE_SWITCH, … */
        uint32_t mods;

        /** Command, like in command mode */
        char *command;

        TAILQ_ENTRY(Binding) bindings;
};

/**
 * Holds a command specified by an exec-line in the config (see src/config.c)
 *
 */
struct Autostart {
        /** Command, like in command mode */
        char *command;
        TAILQ_ENTRY(Autostart) autostarts;
};

/**
 * Holds an assignment for a given window class/title to a specific workspace
 * (see src/config.c)
 *
 */
struct Assignment {
        char *windowclass_title;
        /** floating is true if this was an assignment to the special
         * workspace "~".  Matching clients will be put into floating mode
         * automatically. */
        enum {
                ASSIGN_FLOATING_NO,   /* don’t float, but put on a workspace */
                ASSIGN_FLOATING_ONLY, /* float, but don’t assign on a workspace */
                ASSIGN_FLOATING       /* float and put on a workspace */
        } floating;

        /** The number of the workspace to assign to. */
        int workspace;
        TAILQ_ENTRY(Assignment) assignments;
};

/**
 * Data structure for cached font information:
 * - font id in X11 (load it once)
 * - font height (multiple calls needed to get it)
 *
 */
struct Font {
        /** The name of the font, that is what the pattern resolves to */
        char *name;
        /** A copy of the pattern to build a cache */
        char *pattern;
        /** The height of the font, built from font_ascent + font_descent */
        int height;
        /** The xcb-id for the font */
        xcb_font_t id;

        TAILQ_ENTRY(Font) fonts;
};

/**
 * A client is X11-speak for a window.
 *
 */
struct Client {
        /** initialized will be set to true if the client was fully
         * initialized by manage_window() and all functions can be used
         * normally */
        bool initialized;

        /** if you set a client to floating and set it back to managed, it
         * does remember its old position and *tries* to get back there */
        Cell old_position;

        /** Backpointer. A client is inside a container */
        Container *container;
        /** Because dock clients don’t have a container, we have this
         * workspace-backpointer */
        Workspace *workspace;

        /** x, y, width, height of the frame */
        Rect rect;
        /** Position in floating mode and in tiling mode are saved
         * separately */
        Rect floating_rect;
        /** x, y, width, height of the child (relative to its frame) */
        Rect child_rect;

        /** contains the size calculated from the hints set by the window or 0
         * if the client did not send any hints */
        int proportional_height;
        int proportional_width;

        int base_height;
        int base_width;

        /** The amount of pixels which X will draw around the client. */
        int border_width;

        /** contains the minimum increment size as specified for the window
         * (in pixels). */
        int width_increment;
        int height_increment;

        /** Height which was determined by reading the _NET_WM_STRUT_PARTIAL
         * top/bottom of the screen reservation */
        int desired_height;

        /** Name (= window title) */
        char *name;
        /** name_len stores the real string length (glyphs) of the window
         * title if the client uses _NET_WM_NAME. Otherwise, it is set to -1
         * to indicate that name should be just passed to X as 8-bit string
         * and therefore will not be rendered correctly. This behaviour is to
         * support legacy applications which do not set _NET_WM_NAME */
        int name_len;
        /** This will be set to true as soon as the first _NET_WM_NAME comes
         * in. If set to true, legacy window names are ignored. */
        bool uses_net_wm_name;

        /** Holds the WM_CLASS (which consists of two strings, the instance
         * and the class), useful for matching the client in commands */
        char *window_class_instance;
        char *window_class_class;

        /** Holds the client’s mark, for vim-like jumping */
        char *mark;

        /** Holds the xcb_window_t (just an ID) for the leader window (logical
         * parent for toolwindows and similar floating windows) */
        xcb_window_t leader;

        /** fullscreen is pretty obvious */
        bool fullscreen;

        /** floating? (= not in tiling layout) This cannot be simply a bool
         * because we want to keep track of whether the status was set by the
         * application (by setting WM_CLASS to tools for example) or by the
         * user. The user’s choice overwrites automatic mode, of course. The
         * order of the values is important because we check with >=
         * FLOATING_AUTO_ON if a client is floating. */
        enum { FLOATING_AUTO_OFF = 0, FLOATING_USER_OFF = 1, FLOATING_AUTO_ON = 2, FLOATING_USER_ON = 3 } floating;

        /** Ensure TITLEBAR_TOP maps to 0 because we use calloc for
         * initialization later */
        enum { TITLEBAR_TOP = 0, TITLEBAR_LEFT, TITLEBAR_RIGHT, TITLEBAR_BOTTOM, TITLEBAR_OFF } titlebar_position;

        /** Contains a bool specifying whether this window should not be drawn
         * with the usual decorations */
        bool borderless;

        /** If a client is set as a dock, it is placed at the very bottom of
         * the screen and its requested size is used */
        bool dock;

        /** True if the client set the urgency flag in its WM_HINTS property */
        bool urgent;

        /* After leaving fullscreen mode, a client needs to be reconfigured
         * (configuration = setting X, Y, width and height). By setting the
         * force_reconfigure flag, render_layout() will reconfigure the
         * client. */
        bool force_reconfigure;

        /* When reparenting a window, an unmap-notify is sent. As we delete
         * windows when they’re unmapped, we need to ignore that
         * one. Therefore, this flag is set when reparenting. */
        bool awaiting_useless_unmap;

        /* XCB contexts */
        xcb_window_t frame;             /**< Our window: The frame around the
                                         * client */
        xcb_gcontext_t titlegc;         /**< The titlebar’s graphic context
                                         * inside the frame */
        xcb_window_t child;             /**< The client’s window */

        /** The following entry provides the necessary list pointers to use
         * Client with LIST_* macros */
        CIRCLEQ_ENTRY(Client) clients;
        SLIST_ENTRY(Client) dock_clients;
        SLIST_ENTRY(Client) focus_clients;
        TAILQ_ENTRY(Client) floating_clients;
};

/**
 * A container is either in default, stacking or tabbed mode. There is one for
 * each cell of the table.
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

        /* When in stacking mode, we draw the titlebars of each client onto a
         * separate window */
        struct Stack_Window stack_win;

        /* Backpointer to the workspace this container is in */
        Workspace *workspace;

        /* Ensure MODE_DEFAULT maps to 0 because we use calloc for
         * initialization later */
        enum { MODE_DEFAULT = 0, MODE_STACK, MODE_TABBED } mode;

        /* When in stacking, one can either have unlimited windows inside the
         * container or set a limit for the rows or columns the stack window
         * should display to use the screen more efficiently. */
        enum { STACK_LIMIT_NONE = 0, STACK_LIMIT_COLS, STACK_LIMIT_ROWS } stack_limit;

        /* The number of columns or rows to limit to, see stack_limit */
        int stack_limit_value;

        CIRCLEQ_HEAD(client_head, Client) clients;
};

/**
 * An Output is a physical output on your graphics driver. Outputs which
 * are currently in use have (output->active == true). Each output has a
 * position and a mode. An output usually corresponds to one connected
 * screen (except if you are running multiple screens in clone mode).
 *
 */
struct xoutput {
        /** Output id, so that we can requery the output directly later */
        xcb_randr_output_t id;
        /** Name of the output */
        char *name;

        /** Whether the output is currently (has a CRTC attached with a valid
         * mode) */
        bool active;

        /** Internal flags, necessary for querying RandR screens (happens in
         * two stages) */
        bool changed;
        bool to_be_disabled;

        /** Current workspace selected on this virtual screen */
        Workspace *current_workspace;

        /** x, y, width, height */
        Rect rect;

        /** The bar window */
        xcb_window_t bar;
        xcb_gcontext_t bargc;

        /** Contains all clients with _NET_WM_WINDOW_TYPE ==
         * _NET_WM_WINDOW_TYPE_DOCK */
        SLIST_HEAD(dock_clients_head, Client) dock_clients;

        TAILQ_ENTRY(xoutput) outputs;
};

#endif
