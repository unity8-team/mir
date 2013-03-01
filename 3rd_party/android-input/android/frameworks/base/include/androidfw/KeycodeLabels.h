/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _ANDROIDFW_KEYCODE_LABELS_H
#define _ANDROIDFW_KEYCODE_LABELS_H

#include <androidfw/Platform.h>

#include <android/keycodes.h>

struct KeycodeLabel {
    const char *literal;
    int value;
};

static const KeycodeLabel KEYCODES[] = {
    { "SOFT_LEFT", 1 },
    { "SOFT_RIGHT", 2 },
    { "HOME", 3 },
    { "BACK", 4 },
    { "CALL", 5 },
    { "ENDCALL", 6 },
    { "0", 7 },
    { "1", 8 },
    { "2", 9 },
    { "3", 10 },
    { "4", 11 },
    { "5", 12 },
    { "6", 13 },
    { "7", 14 },
    { "8", 15 },
    { "9", 16 },
    { "STAR", 17 },
    { "POUND", 18 },
    { "DPAD_UP", 19 },
    { "DPAD_DOWN", 20 },
    { "DPAD_LEFT", 21 },
    { "DPAD_RIGHT", 22 },
    { "DPAD_CENTER", 23 },
    { "VOLUME_UP", 24 },
    { "VOLUME_DOWN", 25 },
    { "POWER", 26 },
    { "CAMERA", 27 },
    { "CLEAR", 28 },
    { "A", 29 },
    { "B", 30 },
    { "C", 31 },
    { "D", 32 },
    { "E", 33 },
    { "F", 34 },
    { "G", 35 },
    { "H", 36 },
    { "I", 37 },
    { "J", 38 },
    { "K", 39 },
    { "L", 40 },
    { "M", 41 },
    { "N", 42 },
    { "O", 43 },
    { "P", 44 },
    { "Q", 45 },
    { "R", 46 },
    { "S", 47 },
    { "T", 48 },
    { "U", 49 },
    { "V", 50 },
    { "W", 51 },
    { "X", 52 },
    { "Y", 53 },
    { "Z", 54 },
    { "COMMA", 55 },
    { "PERIOD", 56 },
    { "ALT_LEFT", 57 },
    { "ALT_RIGHT", 58 },
    { "SHIFT_LEFT", 59 },
    { "SHIFT_RIGHT", 60 },
    { "TAB", 61 },
    { "SPACE", 62 },
    { "SYM", 63 },
    { "EXPLORER", 64 },
    { "ENVELOPE", 65 },
    { "ENTER", 66 },
    { "DEL", 67 },
    { "GRAVE", 68 },
    { "MINUS", 69 },
    { "EQUALS", 70 },
    { "LEFT_BRACKET", 71 },
    { "RIGHT_BRACKET", 72 },
    { "BACKSLASH", 73 },
    { "SEMICOLON", 74 },
    { "APOSTROPHE", 75 },
    { "SLASH", 76 },
    { "AT", 77 },
    { "NUM", 78 },
    { "HEADSETHOOK", 79 },
    { "FOCUS", 80 },
    { "PLUS", 81 },
    { "MENU", 82 },
    { "NOTIFICATION", 83 },
    { "SEARCH", 84 },
    { "MEDIA_PLAY_PAUSE", 85 },
    { "MEDIA_STOP", 86 },
    { "MEDIA_NEXT", 87 },
    { "MEDIA_PREVIOUS", 88 },
    { "MEDIA_REWIND", 89 },
    { "MEDIA_FAST_FORWARD", 90 },
    { "MUTE", 91 },
    { "PAGE_UP", 92 },
    { "PAGE_DOWN", 93 },
    { "PICTSYMBOLS", 94 },
    { "SWITCH_CHARSET", 95 },
    { "BUTTON_A", 96 },
    { "BUTTON_B", 97 },
    { "BUTTON_C", 98 },
    { "BUTTON_X", 99 },
    { "BUTTON_Y", 100 },
    { "BUTTON_Z", 101 },
    { "BUTTON_L1", 102 },
    { "BUTTON_R1", 103 },
    { "BUTTON_L2", 104 },
    { "BUTTON_R2", 105 },
    { "BUTTON_THUMBL", 106 },
    { "BUTTON_THUMBR", 107 },
    { "BUTTON_START", 108 },
    { "BUTTON_SELECT", 109 },
    { "BUTTON_MODE", 110 },
    { "ESCAPE", 111 },
    { "FORWARD_DEL", 112 },
    { "CTRL_LEFT", 113 },
    { "CTRL_RIGHT", 114 },
    { "CAPS_LOCK", 115 },
    { "SCROLL_LOCK", 116 },
    { "META_LEFT", 117 },
    { "META_RIGHT", 118 },
    { "FUNCTION", 119 },
    { "SYSRQ", 120 },
    { "BREAK", 121 },
    { "MOVE_HOME", 122 },
    { "MOVE_END", 123 },
    { "INSERT", 124 },
    { "FORWARD", 125 },
    { "MEDIA_PLAY", 126 },
    { "MEDIA_PAUSE", 127 },
    { "MEDIA_CLOSE", 128 },
    { "MEDIA_EJECT", 129 },
    { "MEDIA_RECORD", 130 },
    { "F1", 131 },
    { "F2", 132 },
    { "F3", 133 },
    { "F4", 134 },
    { "F5", 135 },
    { "F6", 136 },
    { "F7", 137 },
    { "F8", 138 },
    { "F9", 139 },
    { "F10", 140 },
    { "F11", 141 },
    { "F12", 142 },
    { "NUM_LOCK", 143 },
    { "NUMPAD_0", 144 },
    { "NUMPAD_1", 145 },
    { "NUMPAD_2", 146 },
    { "NUMPAD_3", 147 },
    { "NUMPAD_4", 148 },
    { "NUMPAD_5", 149 },
    { "NUMPAD_6", 150 },
    { "NUMPAD_7", 151 },
    { "NUMPAD_8", 152 },
    { "NUMPAD_9", 153 },
    { "NUMPAD_DIVIDE", 154 },
    { "NUMPAD_MULTIPLY", 155 },
    { "NUMPAD_SUBTRACT", 156 },
    { "NUMPAD_ADD", 157 },
    { "NUMPAD_DOT", 158 },
    { "NUMPAD_COMMA", 159 },
    { "NUMPAD_ENTER", 160 },
    { "NUMPAD_EQUALS", 161 },
    { "NUMPAD_LEFT_PAREN", 162 },
    { "NUMPAD_RIGHT_PAREN", 163 },
    { "VOLUME_MUTE", 164 },
    { "INFO", 165 },
    { "CHANNEL_UP", 166 },
    { "CHANNEL_DOWN", 167 },
    { "ZOOM_IN", 168 },
    { "ZOOM_OUT", 169 },
    { "TV", 170 },
    { "WINDOW", 171 },
    { "GUIDE", 172 },
    { "DVR", 173 },
    { "BOOKMARK", 174 },
    { "CAPTIONS", 175 },
    { "SETTINGS", 176 },
    { "TV_POWER", 177 },
    { "TV_INPUT", 178 },
    { "STB_POWER", 179 },
    { "STB_INPUT", 180 },
    { "AVR_POWER", 181 },
    { "AVR_INPUT", 182 },
    { "PROG_RED", 183 },
    { "PROG_GREEN", 184 },
    { "PROG_YELLOW", 185 },
    { "PROG_BLUE", 186 },
    { "APP_SWITCH", 187 },
    { "BUTTON_1", 188 },
    { "BUTTON_2", 189 },
    { "BUTTON_3", 190 },
    { "BUTTON_4", 191 },
    { "BUTTON_5", 192 },
    { "BUTTON_6", 193 },
    { "BUTTON_7", 194 },
    { "BUTTON_8", 195 },
    { "BUTTON_9", 196 },
    { "BUTTON_10", 197 },
    { "BUTTON_11", 198 },
    { "BUTTON_12", 199 },
    { "BUTTON_13", 200 },
    { "BUTTON_14", 201 },
    { "BUTTON_15", 202 },
    { "BUTTON_16", 203 },
    { "LANGUAGE_SWITCH", 204 },
    { "MANNER_MODE", 205 },
    { "3D_MODE", 206 },
    { "CONTACTS", 207 },
    { "CALENDAR", 208 },
    { "MUSIC", 209 },
    { "CALCULATOR", 210 },
    { "ZENKAKU_HANKAKU", 211 },
    { "EISU", 212 },
    { "MUHENKAN", 213 },
    { "HENKAN", 214 },
    { "KATAKANA_HIRAGANA", 215 },
    { "YEN", 216 },
    { "RO", 217 },
    { "KANA", 218 },
    { "ASSIST", 219 },

    // NOTE: If you add a new keycode here you must also add it to several other files.
    //       Refer to frameworks/base/core/java/android/view/KeyEvent.java for the full list.

    { NULL, 0 }
};

// NOTE: If you edit these flags, also edit policy flags in Input.h.
static const KeycodeLabel FLAGS[] = {
    { "WAKE", 0x00000001 },
    { "WAKE_DROPPED", 0x00000002 },
    { "SHIFT", 0x00000004 },
    { "CAPS_LOCK", 0x00000008 },
    { "ALT", 0x00000010 },
    { "ALT_GR", 0x00000020 },
    { "MENU", 0x00000040 },
    { "LAUNCHER", 0x00000080 },
    { "VIRTUAL", 0x00000100 },
    { "FUNCTION", 0x00000200 },
    { NULL, 0 }
};

static const KeycodeLabel AXES[] = {
    { "X", 0 },
    { "Y", 1 },
    { "PRESSURE", 2 },
    { "SIZE", 3 },
    { "TOUCH_MAJOR", 4 },
    { "TOUCH_MINOR", 5 },
    { "TOOL_MAJOR", 6 },
    { "TOOL_MINOR", 7 },
    { "ORIENTATION", 8 },
    { "VSCROLL", 9 },
    { "HSCROLL", 10 },
    { "Z", 11 },
    { "RX", 12 },
    { "RY", 13 },
    { "RZ", 14 },
    { "HAT_X", 15 },
    { "HAT_Y", 16 },
    { "LTRIGGER", 17 },
    { "RTRIGGER", 18 },
    { "THROTTLE", 19 },
    { "RUDDER", 20 },
    { "WHEEL", 21 },
    { "GAS", 22 },
    { "BRAKE", 23 },
    { "DISTANCE", 24 },
    { "TILT", 25 },
    { "GENERIC_1", 32 },
    { "GENERIC_2", 33 },
    { "GENERIC_3", 34 },
    { "GENERIC_4", 35 },
    { "GENERIC_5", 36 },
    { "GENERIC_6", 37 },
    { "GENERIC_7", 38 },
    { "GENERIC_8", 39 },
    { "GENERIC_9", 40 },
    { "GENERIC_10", 41 },
    { "GENERIC_11", 42 },
    { "GENERIC_12", 43 },
    { "GENERIC_13", 44 },
    { "GENERIC_14", 45 },
    { "GENERIC_15", 46 },
    { "GENERIC_16", 47 },

    // NOTE: If you add a new axis here you must also add it to several other files.
    //       Refer to frameworks/base/core/java/android/view/MotionEvent.java for the full list.

    { NULL, -1 }
};

#endif // _ANDROIDFW_KEYCODE_LABELS_H
