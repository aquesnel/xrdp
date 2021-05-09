/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2015
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
 *
 * libxup main file
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xup.h"
#include "log.h"
#include "trans.h"
#include "string_calls.h"

static int
send_server_monitor_resize(struct mod *mod, struct stream *s, int width, int height, int bpp);

static int
send_server_monitor_full_invalidate(struct mod *mod, struct stream *s, int width, int height);

static int
send_server_version_message(struct mod *v, struct stream *s);

static int
lib_mod_process_message(struct mod *mod, struct stream *s);

/******************************************************************************/
static int
lib_send_copy(struct mod *mod, struct stream *s)
{
    return trans_write_copy_s(mod->trans, s);
}

/******************************************************************************/
/* return error */
int
lib_mod_start(struct mod *mod, int w, int h, int bpp)
{
    mod->width = w;
    mod->height = h;
    mod->bpp = bpp;
    return 0;
}

/******************************************************************************/
static int
lib_mod_log_peer(struct mod *mod)
{
    int my_pid;
    int pid;
    int uid;
    int gid;

    my_pid = g_getpid();
    if (g_sck_get_peer_cred(mod->trans->sck, &pid, &uid, &gid) == 0)
    {
        LOG(LOG_LEVEL_INFO, "[xorgxrdp] xrdp_pid=%d connected "
            "to X11rdp_pid=%d, X11rdp_uid=%d, X11rdp_gid=%d, "
            "client_ip=%s, client_port=%s",
            my_pid, pid, uid, gid,
            mod->client_info.client_addr,
            mod->client_info.client_port);
    }
    else
    {
        LOG(LOG_LEVEL_WARNING, "lib_mod_log_peer: g_sck_get_peer_cred "
            "failed");
    }
    return 0;
}

/******************************************************************************/
static int
lib_data_in(struct trans *trans)
{
    struct mod *self;
    struct stream *s;
    int len;

    if (trans == 0)
    {
        LOG(LOG_LEVEL_ERROR, "trans is NULL");
        return 1;
    }

    self = (struct mod *)(trans->callback_data);
    s = trans_get_in_s(trans);

    if (s == 0)
    {
        LOG(LOG_LEVEL_ERROR, "trans input stream is NULL");
        return 1;
    }

    switch (trans->extra_flags)
    {
        case 1:
            s->p = s->data;
            in_uint8s(s, 4); /* processed later in lib_mod_process_message */
            in_uint32_le(s, len);
            if (len < 0 || len > 128 * 1024)
            {
                LOG(LOG_LEVEL_ERROR, 
                    "Received [xorgxrdp] invalid message length %d", len);
                return 1;
            }
            if (len > 0)
            {
                trans->header_size = len + 8;
                trans->extra_flags = 2;
                break;
            }
        /* fall through */
        case 2:
            s->p = s->data;
            if (lib_mod_process_message(self, s) != 0)
            {
                LOG(LOG_LEVEL_ERROR, "lib_data_in: lib_mod_process_message failed");
                return 1;
            }
            init_stream(s, 0);
            trans->header_size = 8;
            trans->extra_flags = 1;
            break;
    }

    return 0;
}

/******************************************************************************/
/* return error */
int
lib_mod_connect(struct mod *mod)
{
    int error;
    int i;
    int use_uds;
    struct stream *s;
    char con_port[256];
    char text[256];
    
    /* only support 8, 15, 16, 24, and 32 bpp connections from rdp client */
    if (mod->bpp != 8 && mod->bpp != 15 && mod->bpp != 16 && mod->bpp != 24 && mod->bpp != 32)
    {
        g_sprintf(text, "ERROR: Invalid bits-per-pixel setting. "
                  "Supported bits-per-pixel: 8, 15, 16, 24, and 32. "
                  "Received: %d", mod->bpp);
        mod->server_msg(mod, text, 0);
        return 1;
    }

    if (g_strcmp(mod->ip, "") == 0)
    {
        mod->server_msg(mod, "ERROR: ip address is not set", 0);
        return 1;
    }

    g_sprintf(text, "connecting to: %s", mod->port);
    mod->server_msg(mod, text, 0);
    
    make_stream(s);
    g_sprintf(con_port, "%s", mod->port);
    use_uds = 0;

    if (con_port[0] == '/')
    {
        use_uds = 1;
    }

    error = 0;
    mod->sck_closed = 0;
    i = 0;

    if (use_uds)
    {
        mod->trans = trans_create(TRANS_MODE_UNIX, 8 * 8192, 8192);
        if (mod->trans == 0)
        {
            mod->server_msg(mod, "ERROR: Failed to allocate transport", 0);
            free_stream(s);
            return 1;
        }
    }
    else
    {
        mod->trans = trans_create(TRANS_MODE_TCP, 8 * 8192, 8192);
        if (mod->trans == 0)
        {
            mod->server_msg(mod, "ERROR: Failed to allocate transport", 0);
            free_stream(s);
            return 1;
        }
    }

    mod->trans->si = mod->si;
    mod->trans->my_source = XRDP_SOURCE_MOD;

    g_sprintf(text, "Connecting to Xserver using Xorgxrdp/X11rdp protocol "
              "via %s socket at %s %s", 
              (use_uds ? "UNIX" : "TCP"), mod->ip, con_port);
    mod->server_msg(mod, text, 0);
    while (1)
    {
        
        error = -1;
        if (trans_connect(mod->trans, mod->ip, con_port, 3000) == 0)
        {
            mod->server_msg(mod, "Socket connected", 0);
            error = 0;
        }

        if (error == 0)
        {
            break;
        }

        if (mod->server_is_term(mod))
        {
            break;
        }

        i++;

        if (i >= 60)
        {
            g_sprintf(text, "ERROR: Giving up after %d failed connection "
                      "attempts to Xserver using Xorgxrdp/X11rdp protocol "
                      "via %s socket at %s %s", 
                      i, (use_uds ? "UNIX" : "TCP"), mod->ip, con_port);
            mod->server_msg(mod, text, 0);
            break;
        }

        g_sleep(500);
    }

    if (error == 0)
    {
        if (use_uds)
        {
            lib_mod_log_peer(mod);
        }
    }

    if (error == 0)
    {
        error = send_server_version_message(mod, s);
        if (error)
        {
            mod->server_msg(mod, "ERROR: failed to send version message", 0);
        }
    }

    if (error == 0)
    {
        error = send_server_monitor_resize(mod, s, mod->width, mod->height, mod->bpp);
        if (error)
        {
            mod->server_msg(mod, "ERROR: failed to send screen size message", 0);
        }
    }

    if (error == 0)
    {
        error = send_server_monitor_full_invalidate(mod, s, mod->width, mod->height);
        if (error)
        {
            mod->server_msg(mod, "ERROR: failed to send screen invalidate message", 0);
        }
    }

    free_stream(s);

    if (error != 0)
    {
        trans_delete(mod->trans);
        mod->trans = 0;
        mod->server_msg(mod, "ERROR: Connection to Xserver failed", 0);
        return 1;
    }
    else
    {
        mod->server_msg(mod, "Connection to Xserver succeeded", 0);
        mod->trans->trans_data_in = lib_data_in;
        mod->trans->header_size = 8;
        mod->trans->callback_data = mod;
        mod->trans->no_stream_init_on_data_in = 1;
        mod->trans->extra_flags = 1;
    }

    return 0;
}

/******************************************************************************/
/* return error */
int
lib_mod_event(struct mod *mod, int msg, tbus param1, tbus param2,
              tbus param3, tbus param4)
{
    struct stream *s;
    int len;
    int key;
    int rv;

    make_stream(s);

    if ((msg >= 15) && (msg <= 16)) /* key events */
    {
        key = param2;

        if (key > 0)
        {
            if (key == 65027) /* altgr */
            {
                if (mod->shift_state)
                {
                    LOG_DEVEL(LOG_LEVEL_TRACE, "Special [xorgxrdp] fix for mstsc sending left control down with altgr");
                    /* fix for mstsc sending left control down with altgr */
                    /* control down / up
                    msg param1 param2 param3 param4
                    15  0      65507  29     0
                    16  0      65507  29     49152 */
                    init_stream(s, 8192);
                    s_push_layer(s, iso_hdr, 4);
                    out_uint16_le(s, XORGXRDP_CLIENT_INPUT);
                    out_uint32_le(s, WM_KEYUP); /* key up */
                    out_uint32_le(s, 0);
                    out_uint32_le(s, 65507); /* left control */
                    out_uint32_le(s, 29); /* RDP scan code */
                    out_uint32_le(s, 0xc000); /* flags */
                    s_mark_end(s);
                    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [xorgxrdp] CLIENT_INPUT "
                              "msg_type WM_KEYUP, param1 0, param2 65507, param3 29, param4 0xc000");
                    
                    len = (int)(s->end - s->data);
                    s_pop_layer(s, iso_hdr);
                    out_uint32_le(s, len);
                    LOG_DEVEL(LOG_LEVEL_TRACE, 
                              "Adding header [xorgxrdp] XORGXRDP_MSG_HDR "
                              "msg_length %d, msg_type CLIENT_INPUT", len);
                    lib_send_copy(mod, s);
                }
            }

            if (key == 65507) /* left control */
            {
                mod->shift_state = msg == 15;
            }
        }
    }

    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, XORGXRDP_CLIENT_INPUT);
    out_uint32_le(s, msg);
    out_uint32_le(s, param1);
    out_uint32_le(s, param2);
    out_uint32_le(s, param3);
    out_uint32_le(s, param4);
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [xorgxrdp] CLIENT_INPUT "
              "msg_type %d, param1 %ld, param2 %ld, param3 %ld, param4 %ld",
              msg, param1, param2, param3, param4);

    len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    LOG_DEVEL(LOG_LEVEL_TRACE, 
              "Adding header [xorgxrdp] XORGXRDP_MSG_HDR "
              "msg_length %d, msg_type CLIENT_INPUT", len);
    rv = lib_send_copy(mod, s);
    free_stream(s);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_fill_rect(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int cx;
    int cy;

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] FILL_RECT "
                "x %d, y %d, cx %d, cy %d", 
                x, y, cx, cy);
    
    rv = mod->server_fill_rect(mod, x, y, cx, cy);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_screen_blt(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int cx;
    int cy;
    int srcx;
    int srcy;

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    in_sint16_le(s, srcx);
    in_sint16_le(s, srcy);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] SCREEN_BLT "
                "x %d, y %d, cx %d, cy %d, srcx %d, srcy %d", 
                x, y, cx, cy, srcx, srcy);
                
    rv = mod->server_screen_blt(mod, x, y, cx, cy, srcx, srcy);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_paint_rect(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int cx;
    int cy;
    int len_bmpdata;
    char *bmpdata;
    int width;
    int height;
    int srcx;
    int srcy;

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    in_uint32_le(s, len_bmpdata);
    in_uint8p(s, bmpdata, len_bmpdata);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    in_sint16_le(s, srcx);
    in_sint16_le(s, srcy);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] PAINT_RECT "
                "x %d, y %d, cx %d, cy %d, len_bmpdata %d, bmpdata <omitted>, "
                "width %d, height %d, srcx %d, srcy %d", 
                x, y, cx, cy, len_bmpdata, width, height, srcx, srcy);
                
    rv = mod->server_paint_rect(mod, x, y, cx, cy,
                                bmpdata, width, height,
                                srcx, srcy);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_clip(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int cx;
    int cy;

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] SET_CLIP "
                "x %d, y %d, cx %d, cy %d", 
                x, y, cx, cy);
                
    rv = mod->server_set_clip(mod, x, y, cx, cy);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_reset_clip(struct mod *mod, struct stream *s)
{
    int rv;
    
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] RESET_CLIP");
    rv = mod->server_reset_clip(mod);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_fgcolor(struct mod *mod, struct stream *s)
{
    int rv;
    int fgcolor;

    in_uint32_le(s, fgcolor);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] SET_FGCOLOR "
                "fgcolor %d", fgcolor);
                
    rv = mod->server_set_fgcolor(mod, fgcolor);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_bgcolor(struct mod *mod, struct stream *s)
{
    int rv;
    int bgcolor;

    in_uint32_le(s, bgcolor);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] SET_BGCOLOR "
                "bgcolor %d", bgcolor);
                
    rv = mod->server_set_bgcolor(mod, bgcolor);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_opcode(struct mod *mod, struct stream *s)
{
    int rv;
    int opcode;

    in_uint16_le(s, opcode);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] SET_OPCODE "
                "opcode %d", opcode);
                
    rv = mod->server_set_opcode(mod, opcode);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_pen(struct mod *mod, struct stream *s)
{
    int rv;
    int style;
    int width;

    in_uint16_le(s, style);
    in_uint16_le(s, width);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] SET_PEN "
                "style %d, width %d", style, width);
                
    rv = mod->server_set_pen(mod, style, width);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_draw_line(struct mod *mod, struct stream *s)
{
    int rv;
    int x1;
    int y1;
    int x2;
    int y2;

    in_sint16_le(s, x1);
    in_sint16_le(s, y1);
    in_sint16_le(s, x2);
    in_sint16_le(s, y2);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] DRAW_LINE "
                "x1 %d, y1 %d, x2 %d, y2 %d", x1, y1, x2, y2);
                
    rv = mod->server_draw_line(mod, x1, y1, x2, y2);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_cursor(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    char cur_data[32 * (32 * 3)];
    char cur_mask[32 * (32 / 8)];

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint8a(s, cur_data, 32 * (32 * 3));
    in_uint8a(s, cur_mask, 32 * (32 / 8));
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] SET_CURSOR "
                "x %d, y %d, cur_data <omitted>, cur_mask <omitted>", x, y);
                
    rv = mod->server_set_cursor(mod, x, y, cur_data, cur_mask);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_create_os_surface(struct mod *mod, struct stream *s)
{
    int rv;
    int rdpid;
    int width;
    int height;

    in_uint32_le(s, rdpid);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] CREATE_OS_SURFACE "
                "rdpid %d, width %d, height %d", rdpid, width, height);
                
    rv = mod->server_create_os_surface(mod, rdpid, width, height);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_switch_os_surface(struct mod *mod, struct stream *s)
{
    int rv;
    int rdpid;

    in_uint32_le(s, rdpid);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] SWITCH_OS_SURFACE "
                "rdpid %d", rdpid);
                
    rv = mod->server_switch_os_surface(mod, rdpid);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_delete_os_surface(struct mod *mod, struct stream *s)
{
    int rv;
    int rdpid;

    in_uint32_le(s, rdpid);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] DELETE_OS_SURFACE "
                "rdpid %d", rdpid);
                
    rv = mod->server_delete_os_surface(mod, rdpid);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_paint_rect_os(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int cx;
    int cy;
    int rdpid;
    int srcx;
    int srcy;

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    in_uint32_le(s, rdpid);
    in_sint16_le(s, srcx);
    in_sint16_le(s, srcy);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] PAINT_RECT_OS "
                "x %d, y %d, cx %d, cy %d, rdpid %d, srcx %d, srcy %d", 
                x, y, cx, cy, rdpid, srcx, srcy);
                
    rv = mod->server_paint_rect_os(mod, x, y, cx, cy,
                                   rdpid, srcx, srcy);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_hints(struct mod *mod, struct stream *s)
{
    int rv;
    int hints;
    int mask;

    in_uint32_le(s, hints);
    in_uint32_le(s, mask);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] SET_HINTS "
                "hints 0x%8.8x, mask 0x%8.8x", 
                hints, mask);
                
    rv = mod->server_set_hints(mod, hints, mask);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_window_new_update(struct mod *mod, struct stream *s)
{
    int flags;
    int window_id;
    int title_bytes;
    int index;
    int bytes;
    int rv;
    struct rail_window_state_order rwso;

    g_memset(&rwso, 0, sizeof(rwso));
    in_uint32_le(s, window_id);
    in_uint32_le(s, rwso.owner_window_id);
    in_uint32_le(s, rwso.style);
    in_uint32_le(s, rwso.extended_style);
    in_uint32_le(s, rwso.show_state);
    in_uint16_le(s, title_bytes);

    if (title_bytes > 0)
    {
        rwso.title_info = g_new(char, title_bytes + 1);
        in_uint8a(s, rwso.title_info, title_bytes);
        rwso.title_info[title_bytes] = 0;
    }

    in_uint32_le(s, rwso.client_offset_x);
    in_uint32_le(s, rwso.client_offset_y);
    in_uint32_le(s, rwso.client_area_width);
    in_uint32_le(s, rwso.client_area_height);
    in_uint32_le(s, rwso.rp_content);
    in_uint32_le(s, rwso.root_parent_handle);
    in_uint32_le(s, rwso.window_offset_x);
    in_uint32_le(s, rwso.window_offset_y);
    in_uint32_le(s, rwso.window_client_delta_x);
    in_uint32_le(s, rwso.window_client_delta_y);
    in_uint32_le(s, rwso.window_width);
    in_uint32_le(s, rwso.window_height);
    in_uint16_le(s, rwso.num_window_rects);

    if (rwso.num_window_rects > 0)
    {
        bytes = sizeof(struct rail_window_rect) * rwso.num_window_rects;
        rwso.window_rects = (struct rail_window_rect *)g_malloc(bytes, 0);

        for (index = 0; index < rwso.num_window_rects; index++)
        {
            in_uint16_le(s, rwso.window_rects[index].left);
            in_uint16_le(s, rwso.window_rects[index].top);
            in_uint16_le(s, rwso.window_rects[index].right);
            in_uint16_le(s, rwso.window_rects[index].bottom);
        }
    }

    in_uint32_le(s, rwso.visible_offset_x);
    in_uint32_le(s, rwso.visible_offset_y);
    in_uint16_le(s, rwso.num_visibility_rects);

    if (rwso.num_visibility_rects > 0)
    {
        bytes = sizeof(struct rail_window_rect) * rwso.num_visibility_rects;
        rwso.visibility_rects = (struct rail_window_rect *)g_malloc(bytes, 0);

        for (index = 0; index < rwso.num_visibility_rects; index++)
        {
            in_uint16_le(s, rwso.visibility_rects[index].left);
            in_uint16_le(s, rwso.visibility_rects[index].top);
            in_uint16_le(s, rwso.visibility_rects[index].right);
            in_uint16_le(s, rwso.visibility_rects[index].bottom);
        }
    }

    in_uint32_le(s, flags);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] WINDOW_NEW_UPDATE (RAIL?) "
                "TBD");
                
    mod->server_window_new_update(mod, window_id, &rwso, flags);
    rv = 0;
    g_free(rwso.title_info);
    g_free(rwso.window_rects);
    g_free(rwso.visibility_rects);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_window_delete(struct mod *mod, struct stream *s)
{
    int window_id;
    int rv;

    in_uint32_le(s, window_id);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] WINDOW_DELETE "
                "window_id %d", window_id);
                
    mod->server_window_delete(mod, window_id);
    rv = 0;
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_window_show(struct mod *mod, struct stream *s)
{
    int window_id;
    int rv;
    int flags;
    struct rail_window_state_order rwso;

    g_memset(&rwso, 0, sizeof(rwso));
    in_uint32_le(s, window_id);
    in_uint32_le(s, flags);
    in_uint32_le(s, rwso.show_state);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] WINDOW_SHOW "
                "window_id %d, flags 0x%8.8x, show_state %d", 
                window_id, flags, rwso.show_state);
                
    mod->server_window_new_update(mod, window_id, &rwso, flags);
    rv = 0;
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_add_char(struct mod *mod, struct stream *s)
{
    int rv;
    int font;
    int character;
    int x;
    int y;
    int cx;
    int cy;
    int len_bmpdata;
    char *bmpdata;

    in_uint16_le(s, font);
    in_uint16_le(s, character);
    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    in_uint16_le(s, len_bmpdata);
    in_uint8p(s, bmpdata, len_bmpdata);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] ADD_CHAR "
                "font %d, character %d, x %d, y %d, cx %d, cy %d, "
                "len_bmpdata %d, bmpdata <omitted>", 
                font, character, x, y, cx, cy, len_bmpdata);
                
    rv = mod->server_add_char(mod, font, character, x, y, cx, cy, bmpdata);
    return rv;
}


/******************************************************************************/
/* return error */
static int
process_server_add_char_alpha(struct mod *mod, struct stream *s)
{
    int rv;
    int font;
    int character;
    int x;
    int y;
    int cx;
    int cy;
    int len_bmpdata;
    char *bmpdata;

    in_uint16_le(s, font);
    in_uint16_le(s, character);
    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    in_uint16_le(s, len_bmpdata);
    in_uint8p(s, bmpdata, len_bmpdata);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] ADD_CHAR_ALPHA "
                "font %d, character %d, x %d, y %d, cx %d, cy %d, "
                "len_bmpdata %d, bmpdata <omitted>", 
                font, character, x, y, cx, cy, len_bmpdata);
                
    rv = mod->server_add_char_alpha(mod, font, character, x, y, cx, cy,
                                    bmpdata);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_draw_text(struct mod *mod, struct stream *s)
{
    int rv;
    int font;
    int flags;
    int mixmode;
    int clip_left;
    int clip_top;
    int clip_right;
    int clip_bottom;
    int box_left;
    int box_top;
    int box_right;
    int box_bottom;
    int x;
    int y;
    int len_bmpdata;
    char *bmpdata;

    in_uint16_le(s, font);
    in_uint16_le(s, flags);
    in_uint16_le(s, mixmode);
    in_sint16_le(s, clip_left);
    in_sint16_le(s, clip_top);
    in_sint16_le(s, clip_right);
    in_sint16_le(s, clip_bottom);
    in_sint16_le(s, box_left);
    in_sint16_le(s, box_top);
    in_sint16_le(s, box_right);
    in_sint16_le(s, box_bottom);
    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, len_bmpdata);
    in_uint8p(s, bmpdata, len_bmpdata);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] DRAW_TEXT "
                "font %d, flags 0x%4.4x, mixmode %d, clip_left %d, clip_top %d, "
                "clip_right %d, clip_bottom %d, box_left %d, box_top %d, "
                "box_right %d, box_bottom %d, x %d, y %d, "
                "len_bmpdata %d, bmpdata <omitted>", 
                font, flags, mixmode, clip_left, clip_top,
                clip_right, clip_bottom, box_left, box_top,
                box_right, box_bottom, x, y, len_bmpdata);
                
    rv = mod->server_draw_text(mod, font, flags, mixmode, clip_left, clip_top,
                               clip_right, clip_bottom, box_left, box_top,
                               box_right, box_bottom, x, y, bmpdata, len_bmpdata);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_create_os_surface_bpp(struct mod *mod, struct stream *s)
{
    int rv;
    int rdpid;
    int width;
    int height;
    int bpp;

    in_uint32_le(s, rdpid);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    in_uint8(s, bpp);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] CREATE_OS_SURFACE_BPP "
                "rdpid %d, width %d, height %d, bpp %d", 
                rdpid, width, height, bpp);
                
    rv = mod->server_create_os_surface_bpp(mod, rdpid, width, height, bpp);
    return rv;
}


/******************************************************************************/
/* return error */
static int
process_server_paint_rect_bpp(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int cx;
    int cy;
    int len_bmpdata;
    char *bmpdata;
    int width;
    int height;
    int srcx;
    int srcy;
    int bpp;

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    in_uint32_le(s, len_bmpdata);
    in_uint8p(s, bmpdata, len_bmpdata);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    in_sint16_le(s, srcx);
    in_sint16_le(s, srcy);
    in_uint8(s, bpp);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] PAINT_RECT_BPP "
                "x %d, y %d, cx %d, cy %d, len_bmpdata %d, bmpdata <omitted>, "
                "width %d, height %d, srcx %d, srcy %d, bpp %d", 
                x, y, cx, cy, len_bmpdata, width, height, srcx, srcy, bpp);
                
    rv = mod->server_paint_rect_bpp(mod, x, y, cx, cy,
                                    bmpdata, width, height,
                                    srcx, srcy, bpp);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_composite(struct mod *mod, struct stream *s)
{
    int rv;
    int srcidx;
    int srcformat;
    int srcwidth;
    int srcrepeat;
    int transform[10];
    int mskflags;
    int mskidx;
    int mskformat;
    int mskwidth;
    int mskrepeat;
    int op;
    int srcx;
    int srcy;
    int mskx;
    int msky;
    int dstx;
    int dsty;
    int width;
    int height;
    int dstformat;

    in_uint16_le(s, srcidx);
    in_uint32_le(s, srcformat);
    in_uint16_le(s, srcwidth);
    in_uint8(s, srcrepeat);
    g_memcpy(transform, s->p, 40);
    in_uint8s(s, 40);
    in_uint8(s, mskflags);
    in_uint16_le(s, mskidx);
    in_uint32_le(s, mskformat);
    in_uint16_le(s, mskwidth);
    in_uint8(s, mskrepeat);
    in_uint8(s, op);
    in_sint16_le(s, srcx);
    in_sint16_le(s, srcy);
    in_sint16_le(s, mskx);
    in_sint16_le(s, msky);
    in_sint16_le(s, dstx);
    in_sint16_le(s, dsty);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    in_uint32_le(s, dstformat);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] COMPOSITE "
                "srcidx %d, srcformat %d, srcwidth %d, srcrepeat %d, "
                "transform <omitted>, mskflags %d, mskidx %d, mskformat %d, "
                "mskwidth %d, mskrepeat %d, op %d, srcx %d, srcy %d, mskx %d, msky %d, "
                "dstx %d, dsty %d, width %d, height %d, dstformat %d", 
                srcidx, srcformat, srcwidth, srcrepeat,
                mskflags, mskidx, mskformat,
                mskwidth, mskrepeat, op, srcx, srcy, mskx, msky,
                dstx, dsty, width, height, dstformat);
                
    rv = mod->server_composite(mod, srcidx, srcformat, srcwidth, srcrepeat,
                               transform, mskflags, mskidx, mskformat,
                               mskwidth, mskrepeat, op, srcx, srcy, mskx, msky,
                               dstx, dsty, width, height, dstformat);
    return rv;
}

/******************************************************************************/
/* return error */
static int
process_server_set_pointer_ex(struct mod *mod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int bpp;
    int Bpp;
    char cur_data[32 * (32 * 4)];
    char cur_mask[32 * (32 / 8)];

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, bpp);
    Bpp = (bpp == 0) ? 3 : (bpp + 7) / 8;
    in_uint8a(s, cur_data, 32 * (32 * Bpp));
    in_uint8a(s, cur_mask, 32 * (32 / 8));
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] SET_POINTER_EX "
                "x %d, y %d, bpp %d, cur_data <omitted>, cur_mask <omitted>", 
                x, y, bpp);
                
    rv = mod->server_set_cursor_ex(mod, x, y, cur_data, cur_mask, bpp);
    return rv;
}

/******************************************************************************/
/* return error */
static int
send_paint_rect_ack(struct mod *mod, int flags, int x, int y, int cx, int cy,
                    int frame_id)
{
    int len;
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, XORGXRDP_PAINT_RECT_ACK);
    out_uint32_le(s, flags);
    out_uint32_le(s, frame_id);
    out_uint32_le(s, x);
    out_uint32_le(s, y);
    out_uint32_le(s, cx);
    out_uint32_le(s, cy);
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [xorgxrdp] PAINT_RECT_ACK "
                "flags 0x%8.8x, frame_id %d, x %d, y%d, width %d, height %d",
                flags, frame_id, x, y, cx, cy);

    len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    LOG_DEVEL(LOG_LEVEL_TRACE, 
              "Adding header [xorgxrdp] XORGXRDP_MSG_HDR "
              "msg_length %d, msg_type PAINT_RECT_ACK", len);

    lib_send_copy(mod, s);
    free_stream(s);
    return 0;
}

/******************************************************************************/
/* return error */
static int
process_server_paint_rect_shmem(struct mod *amod, struct stream *s)
{
    int rv;
    int x;
    int y;
    int cx;
    int cy;
    int flags;
    int frame_id;
    int shmem_id;
    int shmem_offset;
    int width;
    int height;
    int srcx;
    int srcy;
    char *bmpdata;

    in_sint16_le(s, x);
    in_sint16_le(s, y);
    in_uint16_le(s, cx);
    in_uint16_le(s, cy);
    in_uint32_le(s, flags);
    in_uint32_le(s, frame_id);
    in_uint32_le(s, shmem_id);
    in_uint32_le(s, shmem_offset);
    in_uint16_le(s, width);
    in_uint16_le(s, height);
    in_sint16_le(s, srcx);
    in_sint16_le(s, srcy);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] PAINT_RECT_SHMEM "
                "x %d, y %d, cx %d, cy %d, flags 0x%8.8x, frame_id %d, "
                "shmem_id %d, shmem_offset %d, width %d, height %d, "
                "srcx %d, srcy %d", 
                x, y, cx, cy, flags, frame_id, shmem_id, shmem_offset, 
                width, height, srcx, srcy);
                                     
    bmpdata = 0;
    rv = 0;
    if (amod->screen_shmem_id_mapped == 0)
    {
        amod->screen_shmem_id = shmem_id;
        amod->screen_shmem_pixels = (char *) g_shmat(amod->screen_shmem_id);
        if (amod->screen_shmem_pixels == (void *) -1)
        {
            /* failed */
            amod->screen_shmem_id = 0;
            amod->screen_shmem_pixels = 0;
            amod->screen_shmem_id_mapped = 0;
        }
        else
        {
            amod->screen_shmem_id_mapped = 1;
        }
    }
    else if (amod->screen_shmem_id != shmem_id)
    {
        amod->screen_shmem_id = shmem_id;
        g_shmdt(amod->screen_shmem_pixels);
        amod->screen_shmem_pixels = (char *) g_shmat(amod->screen_shmem_id);
        if (amod->screen_shmem_pixels == (void *) -1)
        {
            /* failed */
            amod->screen_shmem_id = 0;
            amod->screen_shmem_pixels = 0;
            amod->screen_shmem_id_mapped = 0;
        }
    }
    if (amod->screen_shmem_pixels != 0)
    {
        bmpdata = amod->screen_shmem_pixels + shmem_offset;
    }
    if (bmpdata != 0)
    {
        rv = amod->server_paint_rect(amod, x, y, cx, cy,
                                     bmpdata, width, height,
                                     srcx, srcy);
    }
    send_paint_rect_ack(amod, flags, x, y, cx, cy, frame_id);
    return rv;
}

/******************************************************************************/
/* return error */
static int
send_paint_rect_ex_ack(struct mod *mod, int flags, int frame_id)
{
    int len;
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, XORGXRDP_PAINT_RECT_ACK_EX);
    out_uint32_le(s, flags);
    out_uint32_le(s, frame_id);
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [xorgxrdp] PAINT_RECT_ACK_EX "
                "flags 0x%8.8x, frame_id %d", flags, frame_id);

    len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    LOG_DEVEL(LOG_LEVEL_TRACE, 
              "Adding header [xorgxrdp] XORGXRDP_MSG_HDR "
              "msg_length %d, msg_type PAINT_RECT_ACK_EX", len);
    lib_send_copy(mod, s);
    free_stream(s);
    return 0;
}

/******************************************************************************/
/* return error */
static int
send_suppress_output(struct mod *mod, int suppress,
                     int left, int top, int right, int bottom)
{
    int len;
    struct stream *s;

    make_stream(s);
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, XORGXRDP_SUPRESS_OUTPUT);
    out_uint32_le(s, suppress);
    out_uint32_le(s, left);
    out_uint32_le(s, top);
    out_uint32_le(s, right);
    out_uint32_le(s, bottom);
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [xorgxrdp] SUPRESS_OUTPUT "
                "suppress %d, x1 %d, y1 %d, x2 %d, y2 %d", 
                suppress, left, top, right, bottom);

    len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    LOG_DEVEL(LOG_LEVEL_TRACE, 
              "Adding header [xorgxrdp] XORGXRDP_MSG_HDR "
              "msg_length %d, msg_type SUPRESS_OUTPUT", len);
    lib_send_copy(mod, s);
    free_stream(s);
    return 0;
}

/******************************************************************************/
/* return error */
static int
process_server_paint_rect_shmem_ex(struct mod *amod, struct stream *s)
{
    int num_drects;
    int num_crects;
    int flags;
    int frame_id;
    int shmem_id;
    int shmem_offset;
    int width;
    int height;
    int index;
    int rv;
    tsi16 *ldrects;
    tsi16 *ldrects1;
    tsi16 *lcrects;
    tsi16 *lcrects1;
    char *bmpdata;

    /* dirty pixels */
    in_uint16_le(s, num_drects);
    ldrects = (tsi16 *) g_malloc(2 * 4 * num_drects, 0);
    ldrects1 = ldrects;
    for (index = 0; index < num_drects; index++)
    {
        in_sint16_le(s, ldrects1[0]);
        in_sint16_le(s, ldrects1[1]);
        in_sint16_le(s, ldrects1[2]);
        in_sint16_le(s, ldrects1[3]);
        ldrects1 += 4;
    }

    /* copied pixels */
    in_uint16_le(s, num_crects);
    lcrects = (tsi16 *) g_malloc(2 * 4 * num_crects, 0);
    lcrects1 = lcrects;
    for (index = 0; index < num_crects; index++)
    {
        in_sint16_le(s, lcrects1[0]);
        in_sint16_le(s, lcrects1[1]);
        in_sint16_le(s, lcrects1[2]);
        in_sint16_le(s, lcrects1[3]);
        lcrects1 += 4;
    }

    in_uint32_le(s, flags);
    in_uint32_le(s, frame_id);
    in_uint32_le(s, shmem_id);
    in_uint32_le(s, shmem_offset);

    in_uint16_le(s, width);
    in_uint16_le(s, height);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] PAINT_RECT_SHMEM_EX "
                "-- TBD");
                
    bmpdata = 0;
    if (flags == 0) /* screen */
    {
        if (amod->screen_shmem_id_mapped == 0)
        {
            amod->screen_shmem_id = shmem_id;
            amod->screen_shmem_pixels = (char *) g_shmat(amod->screen_shmem_id);
            if (amod->screen_shmem_pixels == (void *) -1)
            {
                /* failed */
                amod->screen_shmem_id = 0;
                amod->screen_shmem_pixels = 0;
                amod->screen_shmem_id_mapped = 0;
            }
            else
            {
                amod->screen_shmem_id_mapped = 1;
            }
        }
        else if (amod->screen_shmem_id != shmem_id)
        {
            amod->screen_shmem_id = shmem_id;
            g_shmdt(amod->screen_shmem_pixels);
            amod->screen_shmem_pixels = (char *) g_shmat(amod->screen_shmem_id);
            if (amod->screen_shmem_pixels == (void *) -1)
            {
                /* failed */
                amod->screen_shmem_id = 0;
                amod->screen_shmem_pixels = 0;
                amod->screen_shmem_id_mapped = 0;
            }
        }
        if (amod->screen_shmem_pixels != 0)
        {
            bmpdata = amod->screen_shmem_pixels + shmem_offset;
        }
    }
    if (bmpdata != 0)
    {

        rv = amod->server_paint_rects(amod, num_drects, ldrects,
                                      num_crects, lcrects,
                                      bmpdata, width, height,
                                      flags, frame_id);
    }
    else
    {
        rv = 1;
    }

    //LOG_DEVEL(LOG_LEVEL_TRACE, "frame_id %d", frame_id);
    //send_paint_rect_ex_ack(amod, flags, frame_id);

    g_free(lcrects);
    g_free(ldrects);

    return rv;
}

/******************************************************************************/
/* return error */
static int
send_server_version_message(struct mod *mod, struct stream *s)
{
    int len;
    
    /* send version message */
    /* see nutrinolabs/xorgxrdp rdpClientCon.c rdpClientConProcessMsg() */
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, 103); /* msg type = client input */
    out_uint32_le(s, 301); /* client input msg type = version */
    out_uint32_le(s, 0);   /* version param1 */
    out_uint32_le(s, 0);   /* version param2 */
    out_uint32_le(s, 0);   /* version param3 */
    out_uint32_le(s, 1);   /* version param4 */
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [xorgxrdp] CLIENT_INPUT "
              "msg_type VERSION, param1 0, param2 0, param3 0, param4 1");
    
    len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len); /* msg length */
    LOG_DEVEL(LOG_LEVEL_TRACE, 
              "Adding header [xorgxrdp] XORGXRDP_MSG_HDR "
              "msg_length %d, msg_type CLIENT_INPUT", len);
    
    int rv = lib_send_copy(mod, s);
    return rv;
}

/******************************************************************************/
/* return error */
static int
send_server_monitor_resize(struct mod *mod, struct stream *s, int width, int height, int bpp)
{
    /* send screen size message */
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, 103);
    out_uint32_le(s, 300);
    out_uint32_le(s, width);
    out_uint32_le(s, height);
    /*
        TODO: The bpp here is only necessary for initial creation. We should modify XUP to require this
        only on server initialization, but not on resize. Microsoft's RDP protocol does not support changing
        the bpp on resize.
    */
    out_uint32_le(s, bpp);
    out_uint32_le(s, 0);  /* padding */
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [xorgxrdp] CLIENT_INPUT "
              "msg_type RESIZE, width %d, height %d, bpp %d, <padding 4 bytes>",
              width, height, bpp);

    int len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    LOG_DEVEL(LOG_LEVEL_TRACE, 
              "Adding header [xorgxrdp] XORGXRDP_MSG_HDR "
              "msg_length %d, msg_type CLIENT_INPUT", len);

    return lib_send_copy(mod, s);
}

static int
send_server_monitor_full_invalidate(struct mod *mod, struct stream *s, int width, int height)
{
    /* send invalidate message */
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, 103);
    out_uint32_le(s, 200); /* client_input msg_type */
    /* x and y */
    int i = 0;
    out_uint32_le(s, i); /* x and y */
    i = ((width & 0xffff) << 16) | height;
    out_uint32_le(s, i); /* width and height */
    out_uint32_le(s, 0); /* padding */
    out_uint32_le(s, 0); /* padding */
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE, "Sending [xorgxrdp] CLIENT_INPUT "
              "msg_type INVALIDATE, x 0, y 0, width %d, height %d, "
              "<padding 8 bytes>", width, height);
    
    int len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    LOG_DEVEL(LOG_LEVEL_TRACE, 
              "Adding header [xorgxrdp] XORGXRDP_MSG_HDR "
              "msg_length %d, msg_type CLIENT_INPUT", len);

    return lib_send_copy(mod, s);
}

/******************************************************************************/
/* return error */
static int
lib_send_server_version_message(struct mod *mod)
{
    /* send server version message */
    struct stream *s;
    make_stream(s);
    int rv = send_server_version_message(mod, s);
    free_stream(s);
    return rv;
}

/******************************************************************************/
/* return error */
static int
lib_send_server_monitor_resize(struct mod *mod, int width, int height)
{
    /* send screen size message */
    struct stream *s;
    make_stream(s);
    int rv = send_server_monitor_resize(mod, s, width, height, mod->bpp);
    free_stream(s);
    return rv;
}

/******************************************************************************/
/* return error */
static int
lib_send_server_monitor_full_invalidate(struct mod *mod, int width, int height)
{
    /* send invalidate message */
    struct stream *s;
    make_stream(s);
    int rv = send_server_monitor_full_invalidate(mod, s, width, height);
    free_stream(s);
    return rv;
}

/******************************************************************************/
/* return error */
static int
lib_mod_process_orders(struct mod *mod, int type, struct stream *s)
{
    int rv;

    rv = 0;
    switch (type)
    {
        case 1: /* server_begin_update */
            LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] BEGIN_UPDATE");
            rv = mod->server_begin_update(mod);
            break;
        case 2: /* server_end_update */
            LOG_DEVEL(LOG_LEVEL_TRACE, "Received [xorgxrdp] END_UPDATE");
            rv = mod->server_end_update(mod);
            break;
        case 3: /* server_fill_rect */
            rv = process_server_fill_rect(mod, s);
            break;
        case 4: /* server_screen_blt */
            rv = process_server_screen_blt(mod, s);
            break;
        case 5: /* server_paint_rect */
            rv = process_server_paint_rect(mod, s);
            break;
        case 10: /* server_set_clip */
            rv = process_server_set_clip(mod, s);
            break;
        case 11: /* server_reset_clip */
            rv = process_server_reset_clip(mod, s);
            break;
        case 12: /* server_set_fgcolor */
            rv = process_server_set_fgcolor(mod, s);
            break;
        case 13: /* server_set_bgcolor */
            rv = process_server_set_bgcolor(mod, s);
            break;
        case 14: /* server_set_opcode */
            rv =  process_server_set_opcode(mod, s);
            break;
        case 17: /* server_set_pen */
            rv = process_server_set_pen(mod, s);
            break;
        case 18: /* server_draw_line */
            rv = process_server_draw_line(mod, s);
            break;
        case 19: /* server_set_cursor */
            rv = process_server_set_cursor(mod, s);
            break;
        case 20: /* server_create_os_surface */
            rv = process_server_create_os_surface(mod, s);
            break;
        case 21: /* server_switch_os_surface */
            rv = process_server_switch_os_surface(mod, s);
            break;
        case 22: /* server_delete_os_surface */
            rv = process_server_delete_os_surface(mod, s);
            break;
        case 23: /* server_paint_rect_os */
            rv = process_server_paint_rect_os(mod, s);
            break;
        case 24: /* server_set_hints */
            rv = process_server_set_hints(mod, s);
            break;
        case 25: /* server_window_new_update */
            rv = process_server_window_new_update(mod, s);
            break;
        case 26: /* server_window_delete */
            rv = process_server_window_delete(mod, s);
            break;
        case 27: /* server_window_new_update - show */
            rv = process_server_window_show(mod, s);
            break;
        case 28: /* server_add_char */
            rv = process_server_add_char(mod, s);
            break;
        case 29: /* server_add_char_alpha */
            rv = process_server_add_char_alpha(mod, s);
            break;
        case 30: /* server_draw_text */
            rv = process_server_draw_text(mod, s);
            break;
        case 31: /* server_create_os_surface_bpp */
            rv = process_server_create_os_surface_bpp(mod, s);
            break;
        case 32: /* server_paint_rect_bpp */
            rv = process_server_paint_rect_bpp(mod, s);
            break;
        case 33: /* server_composite */
            rv = process_server_composite(mod, s);
            break;
        case 51: /* server_set_pointer_ex */
            rv = process_server_set_pointer_ex(mod, s);
            break;
        case 60: /* server_paint_rect_shmem */
            rv = process_server_paint_rect_shmem(mod, s);
            break;
        case 61: /* server_paint_rect_shmem_ex */
            rv = process_server_paint_rect_shmem_ex(mod, s);
            break;
        default:
            LOG_DEVEL(LOG_LEVEL_WARNING, "Received header [xorgxrdp] "
                    "unknown order_type %d", type);
            rv = 0;
            break;
    }
    return rv;
}

/******************************************************************************/
/* return error */
static int
lib_send_client_info(struct mod *mod)
{
    struct stream *s;
    int len;

    make_stream(s);
    init_stream(s, 8192);
    s_push_layer(s, iso_hdr, 4);
    out_uint16_le(s, XORGXRDP_CLIENT_INFO);
    g_memcpy(s->p, &(mod->client_info), sizeof(mod->client_info));
    s->p += sizeof(mod->client_info);
    s_mark_end(s);
    LOG_DEVEL(LOG_LEVEL_TRACE,
              "Sending [xorgxrdp] CLIENT_INFO client_info <struct client_info>");
    
    len = (int)(s->end - s->data);
    s_pop_layer(s, iso_hdr);
    out_uint32_le(s, len);
    LOG_DEVEL(LOG_LEVEL_TRACE, 
              "Adding header [xorgxrdp] XORGXRDP_MSG_HDR "
              "msg_length %d, msg_type CLIENT_INFO", len);

    lib_send_copy(mod, s);
    free_stream(s);
    return 0;
}

/******************************************************************************/
/* return error */
static int
lib_mod_process_message(struct mod *mod, struct stream *s)
{
    int num_orders;
    int index;
    int rv;
    int len;
    int type;
    char *phold;

    rv = 0;
    if (rv == 0)
    {
        in_uint16_le(s, type);        
        in_uint16_le(s, num_orders);
        in_uint32_le(s, len);
        LOG_DEVEL(LOG_LEVEL_TRACE, 
                  "Received header [xorgxrdp] XORGXRDP_ORDERS_HDR "
                  "msg_type %d, num_orders %d, msg_length %d",
                  type, num_orders, len);

        if (type == 1) /* original order list */
        {
            for (index = 0; index < num_orders; index++)
            {
                in_uint16_le(s, type);
                LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [xorgxrdp] XORGXRDP_ORDER_HDR_V1 "
                          "order_type %d", type);
                rv = lib_mod_process_orders(mod, type, s);

                if (rv != 0)
                {
                    break;
                }
            }
        }
        else if (type == 2) /* caps */
        {
            for (index = 0; index < num_orders; index++)
            {
                phold = s->p;
                in_uint16_le(s, type);
                in_uint16_le(s, len);
                LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [xorgxrdp] XORGXRDP_ORDER_HDR_V2 "
                          "order_type %d, length %d", type, len);

                switch (type)
                {
                    default:
                        LOG_DEVEL(LOG_LEVEL_WARNING, 
                                  "unknown capability type %d, len %d",
                                  type, len);
                        break;
                }

                s->p = phold + len;
            }

            lib_send_client_info(mod);
        }
        else if (type == 3) /* order list with len after type */
        {
            for (index = 0; index < num_orders; index++)
            {
                phold = s->p;
                in_uint16_le(s, type);
                in_uint16_le(s, len);
                LOG_DEVEL(LOG_LEVEL_TRACE, "Received header [xorgxrdp] XORGXRDP_ORDER_HDR_V2 "
                          "order_type %d, length %d", type, len);
    
                rv = lib_mod_process_orders(mod, type, s);

                if (rv != 0)
                {
                    break;
                }

                s->p = phold + len;
            }
        }
        else
        {
            LOG_DEVEL(LOG_LEVEL_TRACE, "unknown type %d", type);
        }
    }

    return rv;
}

/******************************************************************************/
/* return error */
int
lib_mod_signal(struct mod *mod)
{
    /*
     * Note: only signal safe code (eg. setting wait event) should be executed in
     * this funciton. For more details see `man signal-safety`
     */
    return 0;
}

/******************************************************************************/
/* return error */
int
lib_mod_end(struct mod *mod)
{
    LOG_DEVEL(LOG_LEVEL_TRACE, "Callback [xorgxrdp] MOD_END");
    if (mod->screen_shmem_pixels != 0)
    {
        g_shmdt(mod->screen_shmem_pixels);
        mod->screen_shmem_pixels = 0;
    }
    return 0;
}

/******************************************************************************/
/* return error */
int
lib_mod_set_param(struct mod *mod, const char *name, const char *value)
{
    if (g_strcasecmp(name, "username") == 0)
    {
        g_strncpy(mod->username, value, INFO_CLIENT_MAX_CB_LEN - 1);
    }
    else if (g_strcasecmp(name, "password") == 0)
    {
        g_strncpy(mod->password, value, INFO_CLIENT_MAX_CB_LEN - 1);
        value = "<omitted from the log>";
    }
    else if (g_strcasecmp(name, "ip") == 0)
    {
        g_strncpy(mod->ip, value, 255);
    }
    else if (g_strcasecmp(name, "port") == 0)
    {
        g_strncpy(mod->port, value, 255);
    }
    else if (g_strcasecmp(name, "client_info") == 0)
    {
        g_memcpy(&(mod->client_info), value, sizeof(mod->client_info));
        value = "<struct client_info>";
    }
    else
    {
        if (g_strcasecmp(name, "pampassword") == 0)
        {
            value = "<omitted from the log>";
        }
        LOG(LOG_LEVEL_WARNING, "Configuring [xorgxrdp] ignoring: %s = %s", name, value);
        value = NULL;
    }
    
    if (value != NULL)
    {
        LOG(LOG_LEVEL_INFO, "Configuring [xorgxrdp] setting: %s = %s", name, value);
    }
        
    return 0;
}

/******************************************************************************/
/* return error */
int
lib_mod_get_wait_objs(struct mod *mod, tbus *read_objs, int *rcount,
                      tbus *write_objs, int *wcount, int *timeout)
{
    if (mod != 0)
    {
        if (mod->trans != 0)
        {
            trans_get_wait_objs_rw(mod->trans, read_objs, rcount,
                                   write_objs, wcount, timeout);
        }
    }
    return 0;
}

/******************************************************************************/
/* return error */
int
lib_mod_check_wait_objs(struct mod *mod)
{
    int rv;

    rv = 0;
    if (mod != 0)
    {
        if (mod->trans != 0)
        {
            rv = trans_check_wait_objs(mod->trans);
        }
    }

    return rv;
}

/******************************************************************************/
/* return error */
int
lib_mod_frame_ack(struct mod *amod, int flags, int frame_id)
{
    send_paint_rect_ex_ack(amod, flags, frame_id);
    return 0;
}

/******************************************************************************/
/* return error */
int
lib_mod_suppress_output(struct mod *amod, int suppress,
                        int left, int top, int right, int bottom)
{
    send_suppress_output(amod, suppress, left, top, right, bottom);
    return 0;
}

/******************************************************************************/
tintptr EXPORT_CC
mod_init(void)
{
    struct mod *mod;

    LOG_DEVEL(LOG_LEVEL_TRACE, "Callback [xorgxrdp] MOD_INIT");
    
    mod = (struct mod *)g_malloc(sizeof(struct mod), 1);
    mod->size = sizeof(struct mod);
    mod->version = CURRENT_MOD_VER;
    mod->handle = (tintptr) mod;
    mod->mod_connect = lib_mod_connect;
    mod->mod_start = lib_mod_start;
    mod->mod_event = lib_mod_event;
    mod->mod_signal = lib_mod_signal;
    mod->mod_end = lib_mod_end;
    mod->mod_set_param = lib_mod_set_param;
    mod->mod_get_wait_objs = lib_mod_get_wait_objs;
    mod->mod_check_wait_objs = lib_mod_check_wait_objs;
    mod->mod_frame_ack = lib_mod_frame_ack;
    mod->mod_suppress_output = lib_mod_suppress_output;
    mod->mod_server_monitor_resize = lib_send_server_monitor_resize;
    mod->mod_server_monitor_full_invalidate = lib_send_server_monitor_full_invalidate;
    mod->mod_server_version_message = lib_send_server_version_message;
    return (tintptr) mod;
}

/******************************************************************************/
int EXPORT_CC
mod_exit(tintptr handle)
{
    struct mod *mod = (struct mod *) handle;

    if (mod == 0)
    {
        return 0;
    }
    
    LOG_DEVEL(LOG_LEVEL_TRACE, "Callback [xorgxrdp] MOD_EXIT");
    trans_delete(mod->trans);
    g_free(mod);
    return 0;
}
