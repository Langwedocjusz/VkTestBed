#pragma once

// Copied and renamed from glfw3.h in order not to include glfw functionality
// unnecessarily into non-window related code

#define VKTB_KEY_SPACE 32
#define VKTB_KEY_APOSTROPHE 39 /* ' */
#define VKTB_KEY_COMMA 44      /* , */
#define VKTB_KEY_MINUS 45      /* - */
#define VKTB_KEY_PERIOD 46     /* . */
#define VKTB_KEY_SLASH 47      /* / */
#define VKTB_KEY_0 48
#define VKTB_KEY_1 49
#define VKTB_KEY_2 50
#define VKTB_KEY_3 51
#define VKTB_KEY_4 52
#define VKTB_KEY_5 53
#define VKTB_KEY_6 54
#define VKTB_KEY_7 55
#define VKTB_KEY_8 56
#define VKTB_KEY_9 57
#define VKTB_KEY_SEMICOLON 59 /* ; */
#define VKTB_KEY_EQUAL 61     /* = */
#define VKTB_KEY_A 65
#define VKTB_KEY_B 66
#define VKTB_KEY_C 67
#define VKTB_KEY_D 68
#define VKTB_KEY_E 69
#define VKTB_KEY_F 70
#define VKTB_KEY_G 71
#define VKTB_KEY_H 72
#define VKTB_KEY_I 73
#define VKTB_KEY_J 74
#define VKTB_KEY_K 75
#define VKTB_KEY_L 76
#define VKTB_KEY_M 77
#define VKTB_KEY_N 78
#define VKTB_KEY_O 79
#define VKTB_KEY_P 80
#define VKTB_KEY_Q 81
#define VKTB_KEY_R 82
#define VKTB_KEY_S 83
#define VKTB_KEY_T 84
#define VKTB_KEY_U 85
#define VKTB_KEY_V 86
#define VKTB_KEY_W 87
#define VKTB_KEY_X 88
#define VKTB_KEY_Y 89
#define VKTB_KEY_Z 90
#define VKTB_KEY_LEFT_BRACKET 91  /* [ */
#define VKTB_KEY_BACKSLASH 92     /* \ */
#define VKTB_KEY_RIGHT_BRACKET 93 /* ] */
#define VKTB_KEY_GRAVE_ACCENT 96  /* ` */
#define VKTB_KEY_WORLD_1 161      /* non-US #1 */
#define VKTB_KEY_WORLD_2 162      /* non-US #2 */

/* Function keys */
#define VKTB_KEY_ESCAPE 256
#define VKTB_KEY_ENTER 257
#define VKTB_KEY_TAB 258
#define VKTB_KEY_BACKSPACE 259
#define VKTB_KEY_INSERT 260
#define VKTB_KEY_DELETE 261
#define VKTB_KEY_RIGHT 262
#define VKTB_KEY_LEFT 263
#define VKTB_KEY_DOWN 264
#define VKTB_KEY_UP 265
#define VKTB_KEY_PAGE_UP 266
#define VKTB_KEY_PAGE_DOWN 267
#define VKTB_KEY_HOME 268
#define VKTB_KEY_END 269
#define VKTB_KEY_CAPS_LOCK 280
#define VKTB_KEY_SCROLL_LOCK 281
#define VKTB_KEY_NUM_LOCK 282
#define VKTB_KEY_PRINT_SCREEN 283
#define VKTB_KEY_PAUSE 284
#define VKTB_KEY_F1 290
#define VKTB_KEY_F2 291
#define VKTB_KEY_F3 292
#define VKTB_KEY_F4 293
#define VKTB_KEY_F5 294
#define VKTB_KEY_F6 295
#define VKTB_KEY_F7 296
#define VKTB_KEY_F8 297
#define VKTB_KEY_F9 298
#define VKTB_KEY_F10 299
#define VKTB_KEY_F11 300
#define VKTB_KEY_F12 301
#define VKTB_KEY_F13 302
#define VKTB_KEY_F14 303
#define VKTB_KEY_F15 304
#define VKTB_KEY_F16 305
#define VKTB_KEY_F17 306
#define VKTB_KEY_F18 307
#define VKTB_KEY_F19 308
#define VKTB_KEY_F20 309
#define VKTB_KEY_F21 310
#define VKTB_KEY_F22 311
#define VKTB_KEY_F23 312
#define VKTB_KEY_F24 313
#define VKTB_KEY_F25 314
#define VKTB_KEY_KP_0 320
#define VKTB_KEY_KP_1 321
#define VKTB_KEY_KP_2 322
#define VKTB_KEY_KP_3 323
#define VKTB_KEY_KP_4 324
#define VKTB_KEY_KP_5 325
#define VKTB_KEY_KP_6 326
#define VKTB_KEY_KP_7 327
#define VKTB_KEY_KP_8 328
#define VKTB_KEY_KP_9 329
#define VKTB_KEY_KP_DECIMAL 330
#define VKTB_KEY_KP_DIVIDE 331
#define VKTB_KEY_KP_MULTIPLY 332
#define VKTB_KEY_KP_SUBTRACT 333
#define VKTB_KEY_KP_ADD 334
#define VKTB_KEY_KP_ENTER 335
#define VKTB_KEY_KP_EQUAL 336
#define VKTB_KEY_LEFT_SHIFT 340
#define VKTB_KEY_LEFT_CONTROL 341
#define VKTB_KEY_LEFT_ALT 342
#define VKTB_KEY_LEFT_SUPER 343
#define VKTB_KEY_RIGHT_SHIFT 344
#define VKTB_KEY_RIGHT_CONTROL 345
#define VKTB_KEY_RIGHT_ALT 346
#define VKTB_KEY_RIGHT_SUPER 347
#define VKTB_KEY_MENU 348

#define VKTB_MOUSE_BUTTON_1 0
#define VKTB_MOUSE_BUTTON_2 1
#define VKTB_MOUSE_BUTTON_3 2
#define VKTB_MOUSE_BUTTON_4 3
#define VKTB_MOUSE_BUTTON_5 4
#define VKTB_MOUSE_BUTTON_6 5
#define VKTB_MOUSE_BUTTON_7 6
#define VKTB_MOUSE_BUTTON_8 7
#define VKTB_MOUSE_BUTTON_LAST LOFI_MOUSE_BUTTON_8
#define VKTB_MOUSE_BUTTON_LEFT LOFI_MOUSE_BUTTON_1
#define VKTB_MOUSE_BUTTON_RIGHT LOFI_MOUSE_BUTTON_2
#define VKTB_MOUSE_BUTTON_MIDDLE LOFI_MOUSE_BUTTON_3