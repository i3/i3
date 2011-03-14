/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2010 Michael Stapelberg and contributors (see also: LICENSE)
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
 * TODO
 *
 */

/* Forward definitions */
typedef struct Font i3Font;
typedef struct Binding Binding;
typedef struct Rect Rect;
typedef struct xoutput Output;
typedef struct Con Con;
typedef struct Match Match;
typedef struct Window i3Window;


/******************************************************************************
 * Helper types
 *****************************************************************************/
typedef enum { D_LEFT, D_RIGHT, D_UP, D_DOWN } direction_t;
typedef enum { NO_ORIENTATION = 0, HORIZ, VERT } orientation_t;
typedef enum { BS_NORMAL = 0, BS_NONE = 1, BS_1PIXEL = 2 } border_style_t;

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
 * Stores the reserved pixels on each screen edge read from a
 * _NET_WM_STRUT_PARTIAL.
 *
 */
struct reservedpx {
    uint32_t left;
    uint32_t right;
    uint32_t top;
    uint32_t bottom;
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

struct Ignore_Event {
    int sequence;
    time_t added;

    SLIST_ENTRY(Ignore_Event) ignore_events;
};

/******************************************************************************
 * Major types
 *****************************************************************************/

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
 * Data structure for cached font information:
 * - font id in X11 (load it once)
 * - font height (multiple calls needed to get it)
 *
 */
struct Font {
    /** The height of the font, built from font_ascent + font_descent */
    int height;
    /** The xcb-id for the font */
    xcb_font_t id;
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

    /** Pointer to the Con which represents this output */
    Con *con;

    /** Whether the output is currently active (has a CRTC attached with a
     * valid mode) */
    bool active;

    /** Internal flags, necessary for querying RandR screens (happens in
     * two stages) */
    bool changed;
    bool to_be_disabled;
    bool primary;

    /** x, y, width, height */
    Rect rect;

#if 0
    /** The bar window */
    xcb_window_t bar;
    xcb_gcontext_t bargc;

    /** Contains all clients with _NET_WM_WINDOW_TYPE ==
     * _NET_WM_WINDOW_TYPE_DOCK */
    SLIST_HEAD(dock_clients_head, Client) dock_clients;
#endif

    TAILQ_ENTRY(xoutput) outputs;
};

struct Window {
    xcb_window_t id;

    /** Holds the xcb_window_t (just an ID) for the leader window (logical
     * parent for toolwindows and similar floating windows) */
    xcb_window_t leader;
    xcb_window_t transient_for;

    char *class_class;
    char *class_instance;

    /** The name of the window as it will be passed to X11 (in UCS2 if the
     * application supports _NET_WM_NAME, in COMPOUND_TEXT otherwise). */
    char *name_x;

    /** The name of the window as used in JSON (in UTF-8 if the application
     * supports _NET_WM_NAME, in COMPOUND_TEXT otherwise) */
    char *name_json;

    /** The length of the name in glyphs (not bytes) */
    int name_len;

    /** Whether the application used _NET_WM_NAME */
    bool uses_net_wm_name;

    /** Whether the window says it is a dock window */
    enum { W_NODOCK = 0, W_DOCK_TOP = 1, W_DOCK_BOTTOM = 2 } dock;

    /** Pixels the window reserves. left/right/top/bottom */
    struct reservedpx reserved;
};

struct Match {
    enum { M_WINDOW, M_CON } what;

    char *title;
    int title_len;
    char *application;
    char *class;
    char *instance;
    char *mark;
    enum {
        M_DONTCHECK = -1,
        M_NODOCK = 0,
        M_DOCK_ANY = 1,
        M_DOCK_TOP = 2,
        M_DOCK_BOTTOM = 3
    } dock;
    xcb_window_t id;
    Con *con_id;
    enum { M_ANY = 0, M_TILING, M_FLOATING } floating;

    enum { M_GLOBAL = 0, M_OUTPUT, M_WORKSPACE } levels;

    enum { M_USER = 0, M_RESTART } source;

    /* Where the window looking for a match should be inserted:
     *
     * M_HERE   = the matched container will be replaced by the window
     *            (layout saving)
     * M_ACTIVE = the window will be inserted next to the currently focused
     *            container below the matched container
     *            (assignments)
     * M_BELOW  = the window will be inserted as a child of the matched container
     *            (dockareas)
     *
     */
    enum { M_HERE = 0, M_ACTIVE, M_BELOW } insert_where;

    TAILQ_ENTRY(Match) matches;
};

struct Con {
    bool mapped;
    enum {
        CT_ROOT = 0,
        CT_OUTPUT = 1,
        CT_CON = 2,
        CT_FLOATING_CON = 3,
        CT_WORKSPACE = 4,
        CT_DOCKAREA = 5
    } type;
    orientation_t orientation;
    struct Con *parent;

    struct Rect rect;
    struct Rect window_rect;
    struct Rect deco_rect;
    /** the geometry this window requested when getting mapped */
    struct Rect geometry;

    char *name;

    /** the workspace number, if this Con is of type CT_WORKSPACE and the
     * workspace is not a named workspace (for named workspaces, num == -1) */
    int num;

    /* a sticky-group is an identifier which bundles several containers to a
     * group. The contents are shared between all of them, that is they are
     * displayed on whichever of the containers is currently visible */
    char *sticky_group;

    /* user-definable mark to jump to this container later */
    char *mark;

    double percent;

    /* proportional width/height, calculated from WM_NORMAL_HINTS, used to
     * apply an aspect ratio to windows (think of MPlayer) */
    int proportional_width;
    int proportional_height;
    /* the wanted size of the window, used in combination with size
     * increments (see below). */
    int base_width;
    int base_height;

    /* the x11 border pixel attribute */
    int border_width;

    /* minimum increment size specified for the window (in pixels) */
    int width_increment;
    int height_increment;

    struct Window *window;

    /* Should this container be marked urgent? This gets set when the window
     * inside this container (if any) sets the urgency hint, for example. */
    bool urgent;

    /* ids/gc for the frame window */
    xcb_window_t frame;
    xcb_gcontext_t gc;

    /* Only workspace-containers can have floating clients */
    TAILQ_HEAD(floating_head, Con) floating_head;

    TAILQ_HEAD(nodes_head, Con) nodes_head;
    TAILQ_HEAD(focus_head, Con) focus_head;

    TAILQ_HEAD(swallow_head, Match) swallow_head;

    enum { CF_NONE = 0, CF_OUTPUT = 1, CF_GLOBAL = 2 } fullscreen_mode;
    enum { L_DEFAULT = 0, L_STACKED = 1, L_TABBED = 2, L_DOCKAREA = 3, L_OUTPUT = 4 } layout;
    border_style_t border_style;
    /** floating? (= not in tiling layout) This cannot be simply a bool
     * because we want to keep track of whether the status was set by the
     * application (by setting _NET_WM_WINDOW_TYPE appropriately) or by the
     * user. The user’s choice overwrites automatic mode, of course. The
     * order of the values is important because we check with >=
     * FLOATING_AUTO_ON if a client is floating. */
    enum {
        FLOATING_AUTO_OFF = 0,
        FLOATING_USER_OFF = 1,
        FLOATING_AUTO_ON = 2,
        FLOATING_USER_ON = 3
    } floating;

    /** This counter contains the number of UnmapNotify events for this
     * container (or, more precisely, for its ->frame) which should be ignored.
     * UnmapNotify events need to be ignored when they are caused by i3 itself,
     * for example when reparenting or when unmapping the window on a workspace
     * change. */
    uint8_t ignore_unmap;

    TAILQ_ENTRY(Con) nodes;
    TAILQ_ENTRY(Con) focused;
    TAILQ_ENTRY(Con) all_cons;
    TAILQ_ENTRY(Con) floating_windows;

    /** callbacks */
    void(*on_remove_child)(Con *);
};

#endif
