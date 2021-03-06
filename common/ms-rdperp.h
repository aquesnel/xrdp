/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * MS-RDPERP : Definitions from [MS-RDPERP]
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
 * References to MS-RDPERP are currently correct for v20190923 of that
 * document
 */

#if !defined(MS_RDPERP_H)
#define MS_RDPERP_H

/* Window List Capability Set: WndSupportLevel (2.2.1.1.2) */
#define TS_WINDOW_LEVEL_NOT_SUPPORTED  0x00000000
#define TS_WINDOW_LEVEL_SUPPORTED      0x00000001
#define TS_WINDOW_LEVEL_SUPPORTED_EX   0x00000002

#define TS_WINDOW_LEVEL_TO_STR(level) \
    ((level) == TS_WINDOW_LEVEL_NOT_SUPPORTED ? "TS_WINDOW_LEVEL_NOT_SUPPORTED" : \
     (level) == TS_WINDOW_LEVEL_SUPPORTED ? "TS_WINDOW_LEVEL_SUPPORTED" : \
     (level) == TS_WINDOW_LEVEL_SUPPORTED_EX ? "TS_WINDOW_LEVEL_SUPPORTED_EX" : \
     "unknown" \
     )
     
/*
  ORDER_TYPE_WINDOW
    WINDOW_ORDER_TYPE_WINDOW
      WINDOW_ORDER_ICON
      WINDOW_ORDER_CACHED_ICON
      WINDOW_ORDER_STATE_DELETED
      WINDOW_ORDER_STATE_NEW on
      WINDOW_ORDER_STATE_NEW off
    WINDOW_ORDER_TYPE_NOTIFY
      WINDOW_ORDER_STATE_DELETED
      WINDOW_ORDER_STATE_NEW on
      WINDOW_ORDER_STATE_NEW off
    WINDOW_ORDER_TYPE_DESKTOP
      WINDOW_ORDER_FIELD_DESKTOP_NONE on
      WINDOW_ORDER_FIELD_DESKTOP_NONE off
*/

/* Window Order Header Flags */
#define WINDOW_ORDER_TYPE_WINDOW                        0x01000000
#define WINDOW_ORDER_TYPE_NOTIFY                        0x02000000
#define WINDOW_ORDER_TYPE_DESKTOP                       0x04000000

#define WINDOW_ORDER_STATE_NEW                          0x10000000
#define WINDOW_ORDER_STATE_DELETED                      0x20000000
#define WINDOW_ORDER_ICON                               0x40000000
#define WINDOW_ORDER_CACHED_ICON                        0x80000000

#define WINDOW_ORDER_FIELD_OWNER                        0x00000002
#define WINDOW_ORDER_FIELD_TITLE                        0x00000004
#define WINDOW_ORDER_FIELD_STYLE                        0x00000008
#define WINDOW_ORDER_FIELD_SHOW                         0x00000010

#define WINDOW_ORDER_FIELD_WND_RECTS                    0x00000100
#define WINDOW_ORDER_FIELD_VISIBILITY                   0x00000200
#define WINDOW_ORDER_FIELD_WND_SIZE                     0x00000400
#define WINDOW_ORDER_FIELD_WND_OFFSET                   0x00000800
#define WINDOW_ORDER_FIELD_VIS_OFFSET                   0x00001000
#define WINDOW_ORDER_FIELD_ICON_BIG                     0x00002000
#define WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET           0x00004000
#define WINDOW_ORDER_FIELD_WND_CLIENT_DELTA             0x00008000
#define WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE             0x00010000
#define WINDOW_ORDER_FIELD_RP_CONTENT                   0x00020000
#define WINDOW_ORDER_FIELD_ROOT_PARENT                  0x00040000

#define WINDOW_ORDER_FIELD_NOTIFY_TIP                   0x00000001
#define WINDOW_ORDER_FIELD_NOTIFY_INFO_TIP              0x00000002
#define WINDOW_ORDER_FIELD_NOTIFY_STATE                 0x00000004
#define WINDOW_ORDER_FIELD_NOTIFY_VERSION               0x00000008

#define WINDOW_ORDER_FIELD_DESKTOP_NONE                 0x00000001
#define WINDOW_ORDER_FIELD_DESKTOP_HOOKED               0x00000002
#define WINDOW_ORDER_FIELD_DESKTOP_ARC_COMPLETED        0x00000004
#define WINDOW_ORDER_FIELD_DESKTOP_ARC_BEGAN            0x00000008
#define WINDOW_ORDER_FIELD_DESKTOP_ZORDER               0x00000010
#define WINDOW_ORDER_FIELD_DESKTOP_ACTIVE_WND           0x00000020

/* New or Existing Window: ShowState (2.2.1.3.1.2.1) */
#define WINDOW_ORDER_VISIBILITY_HIDE         0x00
#define WINDOW_ORDER_VISIBILITY_MINIMIZED    0x02
#define WINDOW_ORDER_VISIBILITY_MAXIMIZED    0x03
#define WINDOW_ORDER_VISIBILITY_SHOW         0x05

#define WINDOW_ORDER_VISIBILITY_TO_STR(showState) \
    ((showState) == WINDOW_ORDER_VISIBILITY_HIDE ? "WINDOW_ORDER_VISIBILITY_HIDE" : \
     (showState) == WINDOW_ORDER_VISIBILITY_MINIMIZED ? "WINDOW_ORDER_VISIBILITY_MINIMIZED" : \
     (showState) == WINDOW_ORDER_VISIBILITY_MAXIMIZED ? "WINDOW_ORDER_VISIBILITY_MAXIMIZED" : \
     (showState) == WINDOW_ORDER_VISIBILITY_SHOW ? "WINDOW_ORDER_VISIBILITY_SHOW" : \
     "unknown" \
     )



#endif /* MS_RDPERP_H */
