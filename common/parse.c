/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
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
 * Parsing structs and macros
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "arch.h"
#include "os_calls.h"
#include "string_calls.h"
#include "log.h"
#include "parse.h"


#if 0
/*****************************************************************************
 * convert utf-16 encoded string from stream into utf-8 string.
 * note: src_bytes doesn't include the null-terminator char.
 * Copied From: xrdp_sec.c
 * see also: xrdp_orders_send_as_unicode()
 */
static int
unicode_utf16_in(struct stream *s, int src_bytes, char *dst, int dst_len)
{
    twchar *src;
    int num_chars;
    int i;
    int bytes;

    LOG_DEVEL(LOG_LEVEL_TRACE, "unicode_utf16_in: uni_len %d, dst_len %d", src_bytes, dst_len);
    if (src_bytes == 0)
    {
        if (!s_check_rem_and_log(s, 2, "Parsing UTF-16"))
        {
            return 1;
        }
        LOG_DEVEL(LOG_LEVEL_TRACE, "unicode_utf16_in: num_chars 0, dst '' (empty string)");
        in_uint8s(s, 2); /* null terminator */
        return 0;
    }

    bytes = src_bytes + 2; /* include the null terminator */
    src = g_new0(twchar, bytes);
    for (i = 0; i < bytes / 2; ++i)
    {
        if (!s_check_rem_and_log(s, 2, "Parsing UTF-16"))
        {
            g_free(src);
            return 1;
        }
        in_uint16_le(s, src[i]);
    }
    num_chars = g_wcstombs(dst, src, dst_len);
    if (num_chars < 0)
    {
        g_memset(dst, '\0', dst_len);
    }
    LOG_DEVEL(LOG_LEVEL_TRACE, "unicode_utf16_in: num_chars %d, dst '%s'", num_chars, dst);
    g_free(src);

    return 0;
}
#endif


/*****************************************************************************/
/** Read a UTF-16-LE encoded unicode string.
 * 
 * [MS-RDPBCGR] defines all "unicode characters and string" to be encoded 
 * using UTF-16 LE unless otherwise noted.
 * https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpbcgr/ab35aee7-1cf7-42dc-ac74-d0d7f4ca64f7#gt_34715e6f-1612-4b2d-a4bb-3305c56e96f5
 * 
 * Note: this function only handles 16-bit UTF-16 encoding since Microsoft 
 * Windows only supports 16-bit UTF-16 encoding.
 * 
 * Internally xrdp represents string using the standard multi byte strings 
 * approch. For details see:
 * https://www.gnu.org/software/libc/manual/html_node/Character-Set-Handling.html
 * 
 * See also: xrdp_orders_get_unicode_bytes()
 * 
 * @return 0 on success, else 1
 */
int
in_utf16_le(struct stream *s, char *dest, int num_src_bytes, int num_dest_bytes)
{
    int num_chars;
    twchar *wchars;
    int rv;
    int index;
    int mbs_length;

    wchars = 0;
    rv = 0;

    if (num_src_bytes == 0)
    {
        LOG(LOG_LEVEL_DEBUG,
            "No-op parsing a zero length [UTF-16-LE] encoded string");
        return 0;
    }
    if (num_src_bytes % 2 != 0 || num_src_bytes < 0)
    {
        LOG(LOG_LEVEL_ERROR, "Error parsing a [UTF-16-LE] encoded string "
            "which requires 2 bytes per character, but there is %d bytes to "
            "parse", num_src_bytes);
        return 1;
    }
    num_chars = num_src_bytes / 2;
    
    if (s_rem(s) < num_src_bytes)
    {
        LOG(LOG_LEVEL_ERROR, "Not enough bytes in the stream. "
            "Expected %d, Actual %d", num_src_bytes, s_rem(s));
        return 1;
    }
    LOG_DEVEL_HEXDUMP(LOG_LEVEL_DEBUG, "parsing utf-16", s->p, num_src_bytes);
    wchars = (twchar *)g_malloc((num_chars + 1) * sizeof(twchar), 0);
    for (index = 0; index < num_chars; index++)
    {
        in_uint16_le(s, wchars[index]);
    }
    wchars[num_chars] = 0;

    mbs_length = g_wcstombs(0, wchars, 0);
    if (mbs_length + 1 <= num_dest_bytes)
    {
        g_wcstombs(dest, wchars, mbs_length);
        dest[mbs_length] = 0;
    }
    else
    {
        LOG(LOG_LEVEL_ERROR, 
            "Not enough space available in the destination buffer. "
            "Expected %d, Actual %d", mbs_length, num_dest_bytes);
        rv = 1;
    }
    LOG_DEVEL(LOG_LEVEL_DEBUG, "parsed utf-16: %s", dest);

    g_free(wchars);
    return rv;
}

/*****************************************************************************/
/** 
 * Write a UTF-16-LE encoded unicode string to the stream.
 * 
 * @param[out] s
 * @param src - the string to encode and copy into the stream.
 * @param src_length - number of bytes to encode and copy to the stream. 
 *      No NULL terminator is added to the output. If a NULL terminator is 
 *      desired, then it must be included in the src string and the src_length.
 * @return number of bytes written to the stream
 */
int
out_utf16_le(struct stream *s, const char *src, int src_length)
{
    int wcs_length;
    int byte_length;
    int index;
    int i32;
    twchar *wdst;
    char *src_cpy;

    /* g_mbstowcs requires that the src string is null terminated */
    src_cpy = (char *) g_malloc(sizeof(char) * src_length + 1, 1);
    if (src_cpy == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Out of memmory ");
        return 0;
    }
    g_memcpy(src_cpy, src, src_length);
    src_cpy[src_length] = 0;
    
    wcs_length = g_mbstowcs(0, src_cpy, 0);
    byte_length = (wcs_length + 1) * 2;
    if (s_rem_out(s) < byte_length)
    {
        LOG(LOG_LEVEL_ERROR, "Not enough bytes in the stream. "
            "Expected %d, Actual %d", byte_length, s_rem_out(s));
        g_free(src_cpy);
        return 0;
    }
    
    wdst = (twchar *) g_malloc(sizeof(twchar) * (wcs_length + 1), 1);
    if (wdst == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "Out of memmory ");
        g_free(src_cpy);
        return 0;
    }
    
    g_mbstowcs(wdst, src_cpy, wcs_length);
    for (index = 0; index < wcs_length; index++)
    {
        i32 = wdst[index];
        out_uint16_le(s, i32);
    }
    LOG_DEVEL(LOG_LEVEL_DEBUG, "outputing utf-16: %s", src_cpy);
    LOG_DEVEL_HEXDUMP(LOG_LEVEL_DEBUG, "outputing utf-16", (s->p) - (2 * wcs_length), (2 * wcs_length));
    g_free(wdst);
    g_free(src_cpy);
    return wcs_length;
}