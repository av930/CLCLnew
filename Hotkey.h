#ifndef _INC_CM_HOTKEY_H
#define _INC_CM_HOTKEY_H
typedef struct {
    UINT modifiers;
    UINT key;
    BOOL caps;
} CM_HOTKEY;
BOOL cm_hotkey_parse(const TCHAR *text, CM_HOTKEY *hotkey);
void cm_hotkey_edit(HWND edit, HWND main_window);
BOOL cm_hotkey_capturing(void);
#endif