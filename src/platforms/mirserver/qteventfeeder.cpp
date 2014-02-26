#include "qteventfeeder.h"

#include <qpa/qplatforminputcontext.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qwindowsysteminterface.h>
#include <QGuiApplication>
#include <private/qguiapplication_p.h>

#include <QDebug>

using namespace android;

static const QEvent::Type kEventType[] = {
  QEvent::KeyPress,    // AKEY_EVENT_ACTION_DOWN     = 0
  QEvent::KeyRelease,  // AKEY_EVENT_ACTION_UP       = 1
  QEvent::KeyPress     // AKEY_EVENT_ACTION_MULTIPLE = 2
};

// Lookup table for the key codes and unicode values.
static const struct {
  const quint32 keycode;
  const quint16 unicode[3];  // { no modifier, shift modifier, other modifiers }
} kKeyCode[] = {
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_UNKNOWN         = 0
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_SOFT_LEFT       = 1
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_SOFT_RIGHT      = 2
  { Qt::Key_Home, { 0xffff, 0xffff, 0xffff } },            // AKEYCODE_HOME            = 3
  { Qt::Key_Back, { 0xffff, 0xffff, 0xffff } },            // AKEYCODE_BACK            = 4
  { Qt::Key_Call, { 0xffff, 0xffff, 0xffff } },            // AKEYCODE_CALL            = 5
  { Qt::Key_Hangup, { 0xffff, 0xffff, 0xffff } },          // AKEYCODE_ENDCALL         = 6
  { Qt::Key_0, { 0x0030, 0x0029, 0xffff } },               // AKEYCODE_0               = 7
  { Qt::Key_1, { 0x0031, 0xffff, 0xffff } },               // AKEYCODE_1               = 8
  { Qt::Key_2, { 0x0032, 0xffff, 0xffff } },               // AKEYCODE_2               = 9
  { Qt::Key_3, { 0x0033, 0xffff, 0xffff } },               // AKEYCODE_3               = 10
  { Qt::Key_4, { 0x0034, 0xffff, 0xffff } },               // AKEYCODE_4               = 11
  { Qt::Key_5, { 0x0035, 0xffff, 0xffff } },               // AKEYCODE_5               = 12
  { Qt::Key_6, { 0x0036, 0xffff, 0xffff } },               // AKEYCODE_6               = 13
  { Qt::Key_7, { 0x0037, 0xffff, 0xffff } },               // AKEYCODE_7               = 14
  { Qt::Key_8, { 0x0038, 0xffff, 0xffff } },               // AKEYCODE_8               = 15
  { Qt::Key_9, { 0x0039, 0x0028, 0xffff } },               // AKEYCODE_9               = 16
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_STAR            = 17
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_POUND           = 18
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_DPAD_UP         = 19
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_DPAD_DOWN       = 20
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_DPAD_LEFT       = 21
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_DPAD_RIGHT      = 22
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_DPAD_CENTER     = 23
  { Qt::Key_VolumeUp, { 0xffff, 0xffff, 0xffff } },        // AKEYCODE_VOLUME_UP       = 24
  { Qt::Key_VolumeDown, { 0xffff, 0xffff, 0xffff } },      // AKEYCODE_VOLUME_DOWN     = 25
  { Qt::Key_PowerOff, { 0xffff, 0xffff, 0xffff } },        // AKEYCODE_POWER           = 26
  { Qt::Key_Camera, { 0xffff, 0xffff, 0xffff } },          // AKEYCODE_CAMERA          = 27
  { Qt::Key_Clear, { 0xffff, 0xffff, 0xffff } },           // AKEYCODE_CLEAR           = 28
  { Qt::Key_A, { 0x0061, 0x0041, 0xffff } },               // AKEYCODE_A               = 29
  { Qt::Key_B, { 0x0062, 0x0042, 0xffff } },               // AKEYCODE_B               = 30
  { Qt::Key_C, { 0x0063, 0x0043, 0xffff } },               // AKEYCODE_C               = 31
  { Qt::Key_D, { 0x0064, 0x0044, 0xffff } },               // AKEYCODE_D               = 32
  { Qt::Key_E, { 0x0065, 0x0045, 0xffff } },               // AKEYCODE_E               = 33
  { Qt::Key_F, { 0x0066, 0x0046, 0xffff } },               // AKEYCODE_F               = 34
  { Qt::Key_G, { 0x0067, 0x0047, 0xffff } },               // AKEYCODE_G               = 35
  { Qt::Key_H, { 0x0068, 0x0048, 0xffff } },               // AKEYCODE_H               = 36
  { Qt::Key_I, { 0x0069, 0x0049, 0xffff } },               // AKEYCODE_I               = 37
  { Qt::Key_J, { 0x006a, 0x004a, 0xffff } },               // AKEYCODE_J               = 38
  { Qt::Key_K, { 0x006b, 0x004b, 0xffff } },               // AKEYCODE_K               = 39
  { Qt::Key_L, { 0x006c, 0x004c, 0xffff } },               // AKEYCODE_L               = 40
  { Qt::Key_M, { 0x006d, 0x004d, 0xffff } },               // AKEYCODE_M               = 41
  { Qt::Key_N, { 0x006e, 0x004e, 0xffff } },               // AKEYCODE_N               = 42
  { Qt::Key_O, { 0x006f, 0x004f, 0xffff } },               // AKEYCODE_O               = 43
  { Qt::Key_P, { 0x0070, 0x0050, 0xffff } },               // AKEYCODE_P               = 44
  { Qt::Key_Q, { 0x0071, 0x0051, 0xffff } },               // AKEYCODE_Q               = 45
  { Qt::Key_R, { 0x0072, 0x0052, 0xffff } },               // AKEYCODE_R               = 46
  { Qt::Key_S, { 0x0073, 0x0053, 0xffff } },               // AKEYCODE_S               = 47
  { Qt::Key_T, { 0x0074, 0x0054, 0xffff } },               // AKEYCODE_T               = 48
  { Qt::Key_U, { 0x0075, 0x0055, 0xffff } },               // AKEYCODE_U               = 49
  { Qt::Key_V, { 0x0076, 0x0056, 0xffff } },               // AKEYCODE_V               = 50
  { Qt::Key_W, { 0x0077, 0x0057, 0xffff } },               // AKEYCODE_W               = 51
  { Qt::Key_X, { 0x0078, 0x0058, 0xffff } },               // AKEYCODE_X               = 52
  { Qt::Key_Y, { 0x0079, 0x0059, 0xffff } },               // AKEYCODE_Y               = 53
  { Qt::Key_Z, { 0x007a, 0x005a, 0xffff } },               // AKEYCODE_Z               = 54
  { Qt::Key_Comma, { 0x002c, 0xffff, 0xffff } },           // AKEYCODE_COMMA           = 55
  { Qt::Key_Period, { 0x002e, 0xffff, 0xffff } },          // AKEYCODE_PERIOD          = 56
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_ALT_LEFT        = 57
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_ALT_RIGHT       = 58
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_SHIFT_LEFT      = 59
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_SHIFT_RIGHT     = 60
  { Qt::Key_Tab, { 0xffff, 0xffff, 0xffff } },             // AKEYCODE_TAB             = 61
  { Qt::Key_Space, { 0x0020, 0xffff, 0xffff } },           // AKEYCODE_SPACE           = 62
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_SYM             = 63
  { Qt::Key_Explorer, { 0xffff, 0xffff, 0xffff } },        // AKEYCODE_EXPLORER        = 64
  { Qt::Key_LaunchMail, { 0xffff, 0xffff, 0xffff } },      // AKEYCODE_ENVELOPE        = 65
  { Qt::Key_Enter, { 0xffff, 0xffff, 0xffff } },           // AKEYCODE_ENTER           = 66
  { Qt::Key_Delete, { 0xffff, 0xffff, 0xffff } },          // AKEYCODE_DEL             = 67
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_GRAVE           = 68
  { Qt::Key_Minus, { 0x002d, 0x005f, 0xffff } },           // AKEYCODE_MINUS           = 69
  { Qt::Key_Equal, { 0x003d, 0xffff, 0xffff } },           // AKEYCODE_EQUALS          = 70
  { Qt::Key_BracketLeft, { 0x005b, 0xffff, 0xffff } },     // AKEYCODE_LEFT_BRACKET    = 71
  { Qt::Key_BracketRight, { 0x005d, 0xffff, 0xffff } },    // AKEYCODE_RIGHT_BRACKET   = 72
  { Qt::Key_Backslash, { 0x005c, 0xffff, 0xffff } },       // AKEYCODE_BACKSLASH       = 73
  { Qt::Key_Semicolon, { 0x003b, 0x003a, 0xffff } },       // AKEYCODE_SEMICOLON       = 74
  { Qt::Key_Apostrophe, { 0x0027, 0xffff, 0xffff } },      // AKEYCODE_APOSTROPHE      = 75
  { Qt::Key_Slash, { 0x002f, 0xffff, 0xffff } },           // AKEYCODE_SLASH           = 76
  { Qt::Key_At, { 0x0040, 0xffff, 0xffff } },              // AKEYCODE_AT              = 77
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_NUM             = 78
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_HEADSETHOOK     = 79
  { Qt::Key_CameraFocus, { 0xffff, 0xffff, 0xffff } },     // AKEYCODE_FOCUS           = 80  // *Camera* focus
  { Qt::Key_Plus, { 0x002b, 0xffff, 0xffff } },            // AKEYCODE_PLUS            = 81
  { Qt::Key_Menu, { 0xffff, 0xffff, 0xffff } },            // AKEYCODE_MENU            = 82
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_NOTIFICATION    = 83
  { Qt::Key_Search, { 0xffff, 0xffff, 0xffff } },          // AKEYCODE_SEARCH          = 84
  { Qt::Key_MediaTogglePlayPause, { 0xffff, 0xffff, 0xffff } },  // AKEYCODE_MEDIA_PLAY_PAUSE= 85
  { Qt::Key_MediaStop, { 0xffff, 0xffff, 0xffff } },       // AKEYCODE_MEDIA_STOP      = 86
  { Qt::Key_MediaNext, { 0xffff, 0xffff, 0xffff } },       // AKEYCODE_MEDIA_NEXT      = 87
  { Qt::Key_MediaPrevious, { 0xffff, 0xffff, 0xffff } },   // AKEYCODE_MEDIA_PREVIOUS  = 88
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_MEDIA_REWIND    = 89
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_MEDIA_FAST_FORWARD = 90
  { Qt::Key_VolumeMute, { 0xffff, 0xffff, 0xffff } },      // AKEYCODE_MUTE            = 91
  { Qt::Key_PageUp, { 0xffff, 0xffff, 0xffff } },          // AKEYCODE_PAGE_UP         = 92
  { Qt::Key_PageDown, { 0xffff, 0xffff, 0xffff } },        // AKEYCODE_PAGE_DOWN       = 93
  { Qt::Key_Pictures, { 0xffff, 0xffff, 0xffff } },        // AKEYCODE_PICTSYMBOLS     = 94
  { Qt::Key_Mode_switch, { 0xffff, 0xffff, 0xffff } },     // AKEYCODE_SWITCH_CHARSET  = 95
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_A        = 96
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_B        = 97
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_C        = 98
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_X        = 99
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_Y        = 100
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_Z        = 101
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_L1       = 102
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_R1       = 103
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_L2       = 104
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_R2       = 105
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_THUMBL   = 106
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_THUMBR   = 107
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_START    = 108
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_SELECT   = 109
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_MODE     = 110
  { Qt::Key_Escape, { 0xffff, 0xffff, 0xffff } },          // AKEYCODE_ESCAPE          = 111
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_FORWARD_DEL     = 112
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_CTRL_LEFT       = 113
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_CTRL_RIGHT      = 114
  { Qt::Key_CapsLock, { 0xffff, 0xffff, 0xffff } },        // AKEYCODE_CAPS_LOCK       = 115
  { Qt::Key_ScrollLock, { 0xffff, 0xffff, 0xffff } },      // AKEYCODE_SCROLL_LOCK     = 116
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_META_LEFT       = 117
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_META_RIGHT      = 118
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_FUNCTION        = 119
  { Qt::Key_SysReq, { 0xffff, 0xffff, 0xffff } },          // AKEYCODE_SYSRQ           = 120
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BREAK           = 121
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_MOVE_HOME       = 122
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_MOVE_END        = 123
  { Qt::Key_Insert, { 0xffff, 0xffff, 0xffff } },          // AKEYCODE_INSERT          = 124
  { Qt::Key_Forward, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_FORWARD         = 125
  { Qt::Key_MediaPlay, { 0xffff, 0xffff, 0xffff } },       // AKEYCODE_MEDIA_PLAY      = 126
  { Qt::Key_MediaPause, { 0xffff, 0xffff, 0xffff } },      // AKEYCODE_MEDIA_PAUSE     = 127
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_MEDIA_CLOSE     = 128
  { Qt::Key_Eject, { 0xffff, 0xffff, 0xffff } },           // AKEYCODE_MEDIA_EJECT     = 129
  { Qt::Key_MediaRecord, { 0xffff, 0xffff, 0xffff } },     // AKEYCODE_MEDIA_RECORD    = 130
  { Qt::Key_F1, { 0xffff, 0xffff, 0xffff } },              // AKEYCODE_F1              = 131
  { Qt::Key_F2, { 0xffff, 0xffff, 0xffff } },              // AKEYCODE_F2              = 132
  { Qt::Key_F3, { 0xffff, 0xffff, 0xffff } },              // AKEYCODE_F3              = 133
  { Qt::Key_F4, { 0xffff, 0xffff, 0xffff } },              // AKEYCODE_F4              = 134
  { Qt::Key_F5, { 0xffff, 0xffff, 0xffff } },              // AKEYCODE_F5              = 135
  { Qt::Key_F6, { 0xffff, 0xffff, 0xffff } },              // AKEYCODE_F6              = 136
  { Qt::Key_F7, { 0xffff, 0xffff, 0xffff } },              // AKEYCODE_F7              = 137
  { Qt::Key_F8, { 0xffff, 0xffff, 0xffff } },              // AKEYCODE_F8              = 138
  { Qt::Key_F9, { 0xffff, 0xffff, 0xffff } },              // AKEYCODE_F9              = 139
  { Qt::Key_F10, { 0xffff, 0xffff, 0xffff } },             // AKEYCODE_F10             = 140
  { Qt::Key_F11, { 0xffff, 0xffff, 0xffff } },             // AKEYCODE_F11             = 141
  { Qt::Key_F12, { 0xffff, 0xffff, 0xffff } },             // AKEYCODE_F12             = 142
  { Qt::Key_NumLock, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_NUM_LOCK        = 143
  { Qt::Key_0, { 0x0030, 0xffff, 0xffff } },               // AKEYCODE_NUMPAD_0        = 144
  { Qt::Key_1, { 0x0031, 0xffff, 0xffff } },               // AKEYCODE_NUMPAD_1        = 145
  { Qt::Key_2, { 0x0032, 0xffff, 0xffff } },               // AKEYCODE_NUMPAD_2        = 146
  { Qt::Key_3, { 0x0033, 0xffff, 0xffff } },               // AKEYCODE_NUMPAD_3        = 147
  { Qt::Key_4, { 0x0034, 0xffff, 0xffff } },               // AKEYCODE_NUMPAD_4        = 148
  { Qt::Key_5, { 0x0035, 0xffff, 0xffff } },               // AKEYCODE_NUMPAD_5        = 149
  { Qt::Key_6, { 0x0036, 0xffff, 0xffff } },               // AKEYCODE_NUMPAD_6        = 150
  { Qt::Key_7, { 0x0037, 0xffff, 0xffff } },               // AKEYCODE_NUMPAD_7        = 151
  { Qt::Key_8, { 0x0038, 0xffff, 0xffff } },               // AKEYCODE_NUMPAD_8        = 152
  { Qt::Key_9, { 0x0039, 0xffff, 0xffff } },               // AKEYCODE_NUMPAD_9        = 153
  { Qt::Key_Slash, { 0x002f, 0xffff, 0xffff } },           // AKEYCODE_NUMPAD_DIVIDE   = 154
  { Qt::Key_Asterisk, { 0x002a, 0xffff, 0xffff } },        // AKEYCODE_NUMPAD_MULTIPLY = 155
  { Qt::Key_Minus, { 0x002d, 0xffff, 0xffff } },           // AKEYCODE_NUMPAD_SUBTRACT = 156
  { Qt::Key_Plus, { 0x002b, 0xffff, 0xffff } },            // AKEYCODE_NUMPAD_ADD      = 157
  { Qt::Key_Period, { 0x002e, 0xffff, 0xffff } },          // AKEYCODE_NUMPAD_DOT      = 158
  { Qt::Key_Comma, { 0x002c, 0xffff, 0xffff } },           // AKEYCODE_NUMPAD_COMMA    = 159
  { Qt::Key_Enter, { 0xffff, 0xffff, 0xffff } },           // AKEYCODE_NUMPAD_ENTER    = 160
  { Qt::Key_Equal, { 0x003d, 0xffff, 0xffff } },           // AKEYCODE_NUMPAD_EQUALS   = 161
  { Qt::Key_ParenLeft, { 0x0028, 0xffff, 0xffff } },       // AKEYCODE_NUMPAD_LEFT_PAREN = 162
  { Qt::Key_ParenRight, { 0x0029, 0xffff, 0xffff } },      // AKEYCODE_NUMPAD_RIGHT_PAREN = 163
  { Qt::Key_VolumeMute, { 0xffff, 0xffff, 0xffff } },      // AKEYCODE_VOLUME_MUTE     = 164
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_INFO            = 165
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_CHANNEL_UP      = 166
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_CHANNEL_DOWN    = 167
  { Qt::Key_ZoomIn, { 0xffff, 0xffff, 0xffff } },          // AKEYCODE_ZOOM_IN         = 168
  { Qt::Key_ZoomOut, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_ZOOM_OUT        = 169
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_TV              = 170
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_WINDOW          = 171
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_GUIDE           = 172
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_DVR             = 173
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BOOKMARK        = 174
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_CAPTIONS        = 175
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_SETTINGS        = 176
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_TV_POWER        = 177
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_TV_INPUT        = 178
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_STB_POWER       = 179
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_STB_INPUT       = 180
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_AVR_POWER       = 181
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_AVR_INPUT       = 182
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_PROG_RED        = 183
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_PROG_GREEN      = 184
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_PROG_YELLOW     = 185
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_PROG_BLUE       = 186
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_APP_SWITCH      = 187
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_1        = 188
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_2        = 189
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_3        = 190
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_4        = 191
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_5        = 192
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_6        = 193
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_7        = 194
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_8        = 195
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_9        = 196
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_10       = 197
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_11       = 198
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_12       = 199
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_13       = 200
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_14       = 201
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_15       = 202
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_BUTTON_16       = 203
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_LANGUAGE_SWITCH = 204
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_MANNER_MODE     = 205
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_3D_MODE         = 206
  { Qt::Key_unknown, { 0xffff, 0xffff, 0xffff } },         // AKEYCODE_CONTACTS        = 207
  { Qt::Key_Calendar, { 0xffff, 0xffff, 0xffff } },        // AKEYCODE_CALENDAR        = 208
  { Qt::Key_Music, { 0xffff, 0xffff, 0xffff } },           // AKEYCODE_MUSIC           = 209
  { Qt::Key_Calculator, { 0xffff, 0xffff, 0xffff } }       // AKEYCODE_CALCULATOR      = 210
};

QtEventFeeder::QtEventFeeder()
{
  // Initialize touch device. Hardcoded just like in qtubuntu
  mTouchDevice = new QTouchDevice();
  mTouchDevice->setType(QTouchDevice::TouchScreen);
  mTouchDevice->setCapabilities(
      QTouchDevice::Position | QTouchDevice::Area | QTouchDevice::Pressure |
      QTouchDevice::NormalizedPosition);
  QWindowSystemInterface::registerTouchDevice(mTouchDevice);
}

void QtEventFeeder::notifyConfigurationChanged(const NotifyConfigurationChangedArgs* args)
{
    (void)args;
}

void QtEventFeeder::notifyKey(const NotifyKeyArgs* args)
{
    if (QGuiApplication::topLevelWindows().isEmpty())
        return;

    QWindow *window = QGuiApplication::topLevelWindows().first();

    // Key modifier and unicode index mapping.
    const int kMetaState = args->metaState;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    int unicodeIndex = 0;
    if (kMetaState & AMETA_SHIFT_ON) {
        modifiers |= Qt::ShiftModifier;
        unicodeIndex = 1;
    }
    if (kMetaState & AMETA_CTRL_ON) {
        modifiers |= Qt::ControlModifier;
        unicodeIndex = 2;
    }
    if (kMetaState & AMETA_ALT_ON) {
        modifiers |= Qt::AltModifier;
        unicodeIndex = 2;
    }
    if (kMetaState & AMETA_META_ON) {
        modifiers |= Qt::MetaModifier;
        unicodeIndex = 2;
    }

    // Key event propagation.
    QEvent::Type keyType = kEventType[args->action];
    quint32 keyCode = kKeyCode[args->keyCode].keycode;
    QString text(kKeyCode[args->keyCode].unicode[unicodeIndex]);
    ulong timestamp = args->eventTime / 1000000;
    QPlatformInputContext* context = QGuiApplicationPrivate::platformIntegration()->inputContext();
    if (context) {
        QKeyEvent qKeyEvent(keyType, keyCode, modifiers, text);
        qKeyEvent.setTimestamp(timestamp);
        if (context->filterEvent(&qKeyEvent)) {
            // key event filtered out by input context
            return;
        }
    }

    QWindowSystemInterface::handleKeyEvent(window, timestamp, keyType, keyCode, modifiers, text);
}

void QtEventFeeder::notifyMotion(const NotifyMotionArgs* args)
{
    if (QGuiApplication::topLevelWindows().isEmpty())
        return;

    QWindow *window = QGuiApplication::topLevelWindows().first();

    // FIXME(loicm) Max pressure is device specific. That one is for the Samsung Galaxy Nexus. That
    //     needs to be fixed as soon as the compat input lib adds query support.
    const float kMaxPressure = 1.28;
    const QRect kWindowGeometry = window->geometry();
    QList<QWindowSystemInterface::TouchPoint> touchPoints;

    // TODO: Is it worth setting the Qt::TouchPointStationary ones? Currently they are left
    //       as Qt::TouchPointMoved
    const int kPointerCount = (int) args->pointerCount;
    for (int i = 0; i < kPointerCount; ++i) {
        QWindowSystemInterface::TouchPoint touchPoint;

        const float kX = args->pointerCoords[i].getX();
        const float kY = args->pointerCoords[i].getY();
        const float kW = args->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR);
        const float kH = args->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR);
        const float kP = args->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_PRESSURE);
        touchPoint.id = args->pointerProperties[i].id;
        touchPoint.normalPosition = QPointF(kX / kWindowGeometry.width(), kY / kWindowGeometry.height());
        touchPoint.area = QRectF(kX - (kW / 2.0), kY - (kH / 2.0), kW, kH);
        touchPoint.pressure = kP / kMaxPressure;
        touchPoint.state = Qt::TouchPointMoved;

        touchPoints.append(touchPoint);
    }

    switch (args->action & AMOTION_EVENT_ACTION_MASK) {
    case AMOTION_EVENT_ACTION_MOVE:
        // No extra work needed.
        break;

    case AMOTION_EVENT_ACTION_DOWN:
        // NB: hardcoded index 0 because there's only a single touch point in this case
        touchPoints[0].state = Qt::TouchPointPressed;
        break;

    case AMOTION_EVENT_ACTION_UP:
        touchPoints[0].state = Qt::TouchPointReleased;
        break;

    case AMOTION_EVENT_ACTION_POINTER_DOWN: {
        const int index = (args->action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
            AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        touchPoints[index].state = Qt::TouchPointPressed;
        break;
        }

    case AMOTION_EVENT_ACTION_CANCEL:
    case AMOTION_EVENT_ACTION_POINTER_UP: {
        const int index = (args->action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
            AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        touchPoints[index].state = Qt::TouchPointReleased;
        break;
        }

    case AMOTION_EVENT_ACTION_OUTSIDE:
    case AMOTION_EVENT_ACTION_HOVER_MOVE:
    case AMOTION_EVENT_ACTION_SCROLL:
    case AMOTION_EVENT_ACTION_HOVER_ENTER:
    case AMOTION_EVENT_ACTION_HOVER_EXIT:
        default:
        qWarning() << "unhandled motion event action" << (int)(args->action & AMOTION_EVENT_ACTION_MASK);
    }

    // Touch event propagation.
    QWindowSystemInterface::handleTouchEvent(
            window,
            args->eventTime / 1000000,
            mTouchDevice,
            touchPoints);
}

void QtEventFeeder::notifySwitch(const NotifySwitchArgs* args)
{
    (void)args;
}

void QtEventFeeder::notifyDeviceReset(const NotifyDeviceResetArgs* args)
{
    (void)args;
}
