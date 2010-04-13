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
typedef enum { HORIZ, VERT, NO_ORIENTATION } orientation_t;

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

        /** Whether the output is currently active (has a CRTC attached with a
         * valid mode) */
        bool active;

        /** Internal flags, necessary for querying RandR screens (happens in
         * two stages) */
        bool changed;
        bool to_be_disabled;

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

struct Window {
    xcb_window_t id;

    const char *class_class;
    const char *class_instance;
    const char *name_ucs2;
    const char *name_utf8;
    int name_len;
    bool uses_net_wm_name;
};

struct Match {
    enum { M_WINDOW, M_CON } what;

    char *title;
    int title_len;
    char *application;
    char *class;
    char *instance;
    xcb_window_t id;
    bool floating;

    enum { M_GLOBAL, M_OUTPUT, M_WORKSPACE } levels;

    enum { M_USER, M_RESTART } source;

    /* wo das fenster eingefügt werden soll. bei here wird es direkt
     * diesem Con zugewiesen, also layout saving. bei active ist es
     * ein assignment, welches an der momentan fokussierten stelle einfügt */
    enum { M_HERE, M_ACTIVE } insert_where;

    TAILQ_ENTRY(Match) matches;
};

struct Con {
    bool mapped;
    enum { CT_ROOT = 0, CT_OUTPUT = 1, CT_CON = 2, CT_FLOATING_CON = 3 } type;
    orientation_t orientation;
    struct Con *parent;
    /* parent before setting it to floating */
    struct Con *old_parent;

    struct Rect rect;
    struct Rect window_rect;
    struct Rect deco_rect;

    char *name;

    double percent;

    struct Window *window;

    /* ids/gc for the frame window */
    xcb_window_t frame;
    xcb_gcontext_t gc;

    /* Only workspace-containers can have floating clients */
    TAILQ_HEAD(floating_head, Con) floating_head;

    TAILQ_HEAD(nodes_head, Con) nodes_head;
    TAILQ_HEAD(focus_head, Con) focus_head;

    TAILQ_HEAD(swallow_head, Match) swallow_head;

    enum { CF_NONE = 0, CF_OUTPUT = 1, CF_GLOBAL = 2 } fullscreen_mode;
    enum { L_DEFAULT = 0, L_STACKED = 1, L_TABBED = 2 } layout;
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


    TAILQ_ENTRY(Con) nodes;
    TAILQ_ENTRY(Con) focused;
    TAILQ_ENTRY(Con) all_cons;
    TAILQ_ENTRY(Con) floating_windows;
};

#endif
