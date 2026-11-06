/* Physical keys: W3C code names ("KeyQ", "ControlLeft"), evdev numbers and
 * fixed labels; layout keys take their label from the current keyboard
 * layout (BK_key_label). From Awra 1 (awra-key-codes.c).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "awra-private.h"

typedef struct KeyCode {
  const char *code;
  int evdev;
  const char *label;
  bool layout;
} KeyCode;

static const KeyCode g_keys[] = {
    {"Escape", 1, "Esc", false},
    {"Digit1", 2, "1", true},
    {"Digit2", 3, "2", true},
    {"Digit3", 4, "3", true},
    {"Digit4", 5, "4", true},
    {"Digit5", 6, "5", true},
    {"Digit6", 7, "6", true},
    {"Digit7", 8, "7", true},
    {"Digit8", 9, "8", true},
    {"Digit9", 10, "9", true},
    {"Digit0", 11, "0", true},
    {"Minus", 12, "-", true},
    {"Equal", 13, "=", true},
    {"Backspace", 14, "Backspace", false},
    {"Tab", 15, "Tab", false},
    {"KeyQ", 16, "Q", true},
    {"KeyW", 17, "W", true},
    {"KeyE", 18, "E", true},
    {"KeyR", 19, "R", true},
    {"KeyT", 20, "T", true},
    {"KeyY", 21, "Y", true},
    {"KeyU", 22, "U", true},
    {"KeyI", 23, "I", true},
    {"KeyO", 24, "O", true},
    {"KeyP", 25, "P", true},
    {"BracketLeft", 26, "[", true},
    {"BracketRight", 27, "]", true},
    {"Enter", 28, "Enter", false},
    {"ControlLeft", 29, "Ctrl", false},
    {"KeyA", 30, "A", true},
    {"KeyS", 31, "S", true},
    {"KeyD", 32, "D", true},
    {"KeyF", 33, "F", true},
    {"KeyG", 34, "G", true},
    {"KeyH", 35, "H", true},
    {"KeyJ", 36, "J", true},
    {"KeyK", 37, "K", true},
    {"KeyL", 38, "L", true},
    {"Semicolon", 39, ";", true},
    {"Quote", 40, "'", true},
    {"Backquote", 41, "`", true},
    {"ShiftLeft", 42, "Shift", false},
    {"Backslash", 43, "\\", true},
    {"KeyZ", 44, "Z", true},
    {"KeyX", 45, "X", true},
    {"KeyC", 46, "C", true},
    {"KeyV", 47, "V", true},
    {"KeyB", 48, "B", true},
    {"KeyN", 49, "N", true},
    {"KeyM", 50, "M", true},
    {"Comma", 51, ",", true},
    {"Period", 52, ".", true},
    {"Slash", 53, "/", true},
    {"ShiftRight", 54, "Shift", false},
    {"NumpadMultiply", 55, "*", false},
    {"AltLeft", 56, "Alt", false},
    {"Space", 57, "Space", false},
    {"CapsLock", 58, "Caps Lock", false},
    {"F1", 59, "F1", false},
    {"F2", 60, "F2", false},
    {"F3", 61, "F3", false},
    {"F4", 62, "F4", false},
    {"F5", 63, "F5", false},
    {"F6", 64, "F6", false},
    {"F7", 65, "F7", false},
    {"F8", 66, "F8", false},
    {"F9", 67, "F9", false},
    {"F10", 68, "F10", false},
    {"NumLock", 69, "Num", false},
    {"ScrollLock", 70, "ScrLk", false},
    {"Numpad7", 71, "7", false},
    {"Numpad8", 72, "8", false},
    {"Numpad9", 73, "9", false},
    {"NumpadSubtract", 74, "−", false},
    {"Numpad4", 75, "4", false},
    {"Numpad5", 76, "5", false},
    {"Numpad6", 77, "6", false},
    {"NumpadAdd", 78, "+", false},
    {"Numpad1", 79, "1", false},
    {"Numpad2", 80, "2", false},
    {"Numpad3", 81, "3", false},
    {"Numpad0", 82, "0", false},
    {"NumpadDecimal", 83, ".", false},
    {"IntlBackslash", 86, "<", true},
    {"F11", 87, "F11", false},
    {"F12", 88, "F12", false},
    {"NumpadEnter", 96, "Enter", false},
    {"ControlRight", 97, "Ctrl", false},
    {"NumpadDivide", 98, "/", false},
    {"PrintScreen", 99, "PrtSc", false},
    {"Home", 102, "Home", false},
    {"ArrowUp", 103, "↑", false},
    {"PageUp", 104, "PgUp", false},
    {"ArrowLeft", 105, "←", false},
    {"ArrowRight", 106, "→", false},
    {"End", 107, "End", false},
    {"ArrowDown", 108, "↓", false},
    {"PageDown", 109, "PgDn", false},
    {"Insert", 110, "Ins", false},
    {"Delete", 111, "Del", false},
    {"Pause", 119, "Pause", false},
    {"MetaLeft", 125, "Super", false},
    {"MetaRight", 126, "Super", false},
    {"ContextMenu", 127, "Menu", false},
    {"AltRight", 100, "Alt", false},
};

#define N_KEYS ((int)(sizeof(g_keys) / sizeof(g_keys[0])))

const char *awra_key_code_name(int evdev);
const char *awra_key_code_name(int evdev)
{
  for (int i = 0; i < N_KEYS; i++) {
    if (g_keys[i].evdev == evdev) {
      return g_keys[i].code;
    }
  }
  return NULL;
}

int awra_key_code_evdev(const char *code);
int awra_key_code_evdev(const char *code)
{
  for (int i = 0; code && i < N_KEYS; i++) {
    if (!strcmp(g_keys[i].code, code)) {
      return g_keys[i].evdev;
    }
  }
  return 0;
}

/* The label of a physical key: its character in the current layout for
 * layout keys ("A" on QWERTY is "Q" on AZERTY), else the fixed name. */
const char *awra_key_code_label(const char *code);
const char *awra_key_code_label(const char *code)
{
  for (int i = 0; code && i < N_KEYS; i++) {
    if (!strcmp(g_keys[i].code, code)) {
      if (g_keys[i].layout) {
        const char *l = BK_key_label(g_keys[i].evdev);
        if (l && l[0]) {
          return l;
        }
      }
      return g_keys[i].label;
    }
  }
  return code;
}
