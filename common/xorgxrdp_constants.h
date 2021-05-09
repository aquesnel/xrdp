/**
 * xrdp: A Remote Desktop Protocol server.
 * Miscellaneous protocol constants
 *
 * Copyright (C) 2020
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
 
#if !defined(XORGXRDP_CONSTANTS_H)
#define XORGXRDP_CONSTANTS_H

#define XORGXRDP_CLIENT_INPUT           103
#define XORGXRDP_CLIENT_INFO            104
#define XORGXRDP_PAINT_RECT_ACK         105
#define XORGXRDP_PAINT_RECT_ACK_EX      106
#define XORGXRDP_SUPRESS_OUTPUT         108

// #define XORGXRDP_CLIENT_INPUT_KEY_DOWN          15 /* WM_KEYDOWN */
// #define XORGXRDP_CLIENT_INPUT_KEY_UP            16 /* WM_KEYUP */
#define XORGXRDP_CLIENT_INPUT_KEY_SYNC          17
#define XORGXRDP_CLIENT_INPUT_KEY_LOAD_LAYOUT   18
// #define XORGXRDP_CLIENT_INPUT_INVALIDATE        200 /* WM_INVALIDATE */
#define XORGXRDP_CLIENT_INPUT_RESIZE            300
#define XORGXRDP_CLIENT_INPUT_VERSION           301


#endif /* XORGXRDP_CONSTANTS_H */
