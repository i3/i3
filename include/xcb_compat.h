/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * xcb_compat.h: uses #define to create aliases for xcb functions which got
 *               renamed. Makes the code work with >= 0.3.8 xcb-util and
 *               older versions.
 *
 */
#pragma once

#define xcb_icccm_get_wm_protocols_reply_t xcb_get_wm_protocols_reply_t
#define xcb_icccm_get_wm_protocols xcb_get_wm_protocols
#define xcb_icccm_get_wm_protocols_unchecked xcb_get_wm_protocols_unchecked
#define xcb_icccm_get_wm_protocols_reply xcb_get_wm_protocols_reply
#define xcb_icccm_get_wm_protocols_reply_wipe xcb_get_wm_protocols_reply_wipe
#define XCB_ICCCM_WM_STATE_NORMAL XCB_WM_STATE_NORMAL
#define XCB_ICCCM_WM_STATE_WITHDRAWN XCB_WM_STATE_WITHDRAWN
#define xcb_icccm_get_wm_size_hints_from_reply xcb_get_wm_size_hints_from_reply
#define xcb_icccm_get_wm_size_hints_reply xcb_get_wm_size_hints_reply
#define xcb_icccm_get_wm_normal_hints xcb_get_wm_normal_hints
#define xcb_icccm_get_wm_normal_hints_reply xcb_get_wm_normal_hints_reply
#define xcb_icccm_get_wm_normal_hints_unchecked xcb_get_wm_normal_hints_unchecked
#define XCB_ICCCM_SIZE_HINT_P_MIN_SIZE XCB_SIZE_HINT_P_MIN_SIZE
#define XCB_ICCCM_SIZE_HINT_P_MAX_SIZE XCB_SIZE_HINT_P_MAX_SIZE
#define XCB_ICCCM_SIZE_HINT_P_RESIZE_INC XCB_SIZE_HINT_P_RESIZE_INC
#define XCB_ICCCM_SIZE_HINT_BASE_SIZE XCB_SIZE_HINT_BASE_SIZE
#define XCB_ICCCM_SIZE_HINT_P_ASPECT XCB_SIZE_HINT_P_ASPECT
#define xcb_icccm_wm_hints_t xcb_wm_hints_t
#define xcb_icccm_get_wm_hints xcb_get_wm_hints
#define xcb_icccm_get_wm_hints_from_reply xcb_get_wm_hints_from_reply
#define xcb_icccm_get_wm_hints_reply xcb_get_wm_hints_reply
#define xcb_icccm_get_wm_hints_unchecked xcb_get_wm_hints_unchecked
#define xcb_icccm_wm_hints_get_urgency xcb_wm_hints_get_urgency
#define xcb_icccm_get_wm_transient_for_from_reply xcb_get_wm_transient_for_from_reply

#define XCB_ATOM_CARDINAL CARDINAL
#define XCB_ATOM_WINDOW WINDOW
#define XCB_ATOM_WM_TRANSIENT_FOR WM_TRANSIENT_FOR
#define XCB_ATOM_WM_NAME WM_NAME
#define XCB_ATOM_WM_CLASS WM_CLASS
#define XCB_ATOM_WM_HINTS WM_HINTS
#define XCB_ATOM_ATOM ATOM
#define XCB_ATOM_WM_NORMAL_HINTS WM_NORMAL_HINTS
#define XCB_ATOM_STRING STRING
