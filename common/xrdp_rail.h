/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2012-2014
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if !defined(_XRDP_RAIL_H)
#define _XRDP_RAIL_H

#include "ms-rdperp.h"

/* Xrdp-Chansrv RAIL_DRAWING_ORDERS */
#define RAIL_CREATE_WINDOW          2
#define RAIL_DESTROY_WINDOW         4
#define RAIL_SHOW_WINDOW            6
#define RAIL_UPDATE_WINDOW_TITLE    8
#define RAIL_CONFIGURE_WINDOW      10
#define RAIL_SYNC                  12

#define RAIL_DRAWING_ORDER_TO_STR(rail_order_type) \
    ((rail_order_type) == RAIL_CREATE_WINDOW ? "RAIL_CREATE_WINDOW" : \
     (rail_order_type) == RAIL_DESTROY_WINDOW ? "RAIL_DESTROY_WINDOW" : \
     (rail_order_type) == RAIL_SHOW_WINDOW ? "RAIL_SHOW_WINDOW" : \
     (rail_order_type) == RAIL_UPDATE_WINDOW_TITLE ? "RAIL_UPDATE_WINDOW_TITLE" : \
     (rail_order_type) == RAIL_CONFIGURE_WINDOW ? "RAIL_CONFIGURE_WINDOW" : \
     (rail_order_type) == RAIL_SYNC ? "RAIL_SYNC" : \
     "unknown" \
     )

/******************************************************************************/
/*
 * [Win32] Window Styles and Extended Window Styles
 */

/* for tooltips */
#define RAIL_STYLE_TOOLTIP (0x80000000)
#define RAIL_EXT_STYLE_TOOLTIP (0x00000080 | 0x00000008)

/* for normal desktop windows */
#define RAIL_STYLE_NORMAL (0x00C00000 | 0x00080000 | 0x00040000 | 0x00010000 | 0x00020000)
#define RAIL_EXT_STYLE_NORMAL (0x00040000)

/* for dialogs */
#define RAIL_STYLE_DIALOG (0x80000000)
#define RAIL_EXT_STYLE_DIALOG (0x00040000)

#define RAIL_STYLE_TO_STR(code) \
    ((code) == RAIL_STYLE_TOOLTIP     ? "RAIL_STYLE_TOOLTIP" : \
     (code) == RAIL_STYLE_NORMAL      ? "RAIL_STYLE_NORMAL" : \
     (code) == RAIL_STYLE_DIALOG      ? "RAIL_STYLE_DIALOG" : \
     "unknown" \
    )
    
#define RAIL_EXT_STYLE_TO_STR(code) \
    ((code) == RAIL_EXT_STYLE_TOOLTIP ? "RAIL_EXT_STYLE_TOOLTIP" : \
     (code) == RAIL_EXT_STYLE_NORMAL  ? "RAIL_EXT_STYLE_NORMAL" : \
     (code) == RAIL_EXT_STYLE_DIALOG  ? "RAIL_EXT_STYLE_DIALOG" : \
     "unknown" \
    )
    
struct rail_icon_info
{
    int bpp;
    int width;
    int height;
    int cmap_bytes;
    int mask_bytes;
    int data_bytes;
    char *mask;
    char *cmap;
    char *data;
};

struct rail_window_rect
{
    short left;
    short top;
    short right;
    short bottom;
};

struct rail_notify_icon_infotip
{
    int timeout;
    int flags;
    char *text;
    char *title;
};

struct rail_window_state_order
{
    int owner_window_id;
    int style;
    int extended_style;
    int show_state;
    char *title_info;
    int client_offset_x;
    int client_offset_y;
    int client_area_width;
    int client_area_height;
    int rp_content;
    int root_parent_handle;
    int window_offset_x;
    int window_offset_y;
    int window_client_delta_x;
    int window_client_delta_y;
    int window_width;
    int window_height;
    int num_window_rects;
    struct rail_window_rect *window_rects;
    int visible_offset_x;
    int visible_offset_y;
    int num_visibility_rects;
    struct rail_window_rect *visibility_rects;
};

struct rail_notify_state_order
{
    int version;
    char *tool_tip;
    struct rail_notify_icon_infotip infotip;
    int state;
    int icon_cache_entry;
    int icon_cache_id;
    struct rail_icon_info icon_info;
};

struct rail_monitored_desktop_order
{
    int active_window_id;
    int num_window_ids;
    int *window_ids;
};

#endif
