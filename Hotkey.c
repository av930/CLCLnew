#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include "General.h"
#include "Hotkey.h"

static HHOOK capture_hook;
static HWND capture_edit;
static UINT capture_modifiers;
static BOOL capture_caps;

BOOL cm_hotkey_parse(const TCHAR *text, CM_HOTKEY *hotkey)
{
    TCHAR copy[64], *part, *next;
    const TCHAR *names[] = { TEXT("SPACE"), TEXT("TAB"), TEXT("ENTER"), TEXT("ESC"), TEXT("INSERT"),
        TEXT("DELETE"), TEXT("HOME"), TEXT("END"), TEXT("PGUP"), TEXT("PGDN"), TEXT("LEFT"), TEXT("RIGHT"), TEXT("UP"), TEXT("DOWN") };
    UINT keys[] = { VK_SPACE, VK_TAB, VK_RETURN, VK_ESCAPE, VK_INSERT, VK_DELETE, VK_HOME, VK_END,
        VK_PRIOR, VK_NEXT, VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN };
    int index;
    ZeroMemory(hotkey, sizeof(*hotkey));
    lstrcpyn(copy, text, ARRAYSIZE(copy));
    CharUpperBuff(copy, lstrlen(copy));
    for (part = copy; part != NULL; part = next) {
        next = _tcschr(part, TEXT('+'));
        if (next != NULL) *next++ = 0;
        if (!*part) return FALSE;
        if (!lstrcmp(part, TEXT("CTRL")) || !lstrcmp(part, TEXT("CONTROL"))) hotkey->modifiers |= MOD_CONTROL;
        else if (!lstrcmp(part, TEXT("ALT"))) hotkey->modifiers |= MOD_ALT;
        else if (!lstrcmp(part, TEXT("SHIFT"))) hotkey->modifiers |= MOD_SHIFT;
        else if (!lstrcmp(part, TEXT("WIN")) || !lstrcmp(part, TEXT("WINDOWS"))) hotkey->modifiers |= MOD_WIN;
        else if (!lstrcmp(part, TEXT("CAPS")) || !lstrcmp(part, TEXT("CAPSLOCK"))) hotkey->caps = TRUE;
        else {
            if (hotkey->key) return FALSE;
            if (lstrlen(part) == 1 && ((part[0] >= TEXT('A') && part[0] <= TEXT('Z')) ||
                (part[0] >= TEXT('0') && part[0] <= TEXT('9')))) hotkey->key = part[0];
            else if (part[0] == TEXT('F') && _ttoi(part + 1) >= 1 && _ttoi(part + 1) <= 24) {
                int function = _ttoi(part + 1);
                TCHAR expected[8];
                wsprintf(expected, TEXT("F%d"), function);
                if (lstrcmp(expected, part)) return FALSE;
                hotkey->key = VK_F1 + function - 1;
            } else if (_tcsncmp(part, TEXT("VK_"), 3) == 0) {
                unsigned int key = 0;
                TCHAR extra;
                if (_stscanf_s(part + 3, TEXT("%x%c"), &key, &extra, 1) != 1 || key < 1 || key > 254) return FALSE;
                hotkey->key = key;
            } else {
                for (index = 0; index < ARRAYSIZE(keys); index++) if (!lstrcmp(names[index], part)) { hotkey->key = keys[index]; break; }
                if (!hotkey->key) return FALSE;
            }
        }
    }
    return hotkey->key != 0 && (hotkey->modifiers != 0 || hotkey->caps);
}

static void capture_key(UINT key)
{
    TCHAR text[64] = TEXT(""), name[32];
    if (capture_modifiers & MOD_CONTROL) lstrcat(text, TEXT("Ctrl+"));
    if (capture_modifiers & MOD_ALT) lstrcat(text, TEXT("Alt+"));
    if (capture_modifiers & MOD_SHIFT) lstrcat(text, TEXT("Shift+"));
    if (capture_modifiers & MOD_WIN) lstrcat(text, TEXT("Win+"));
    if (capture_caps) lstrcat(text, TEXT("CapsLock+"));
    if (key == VK_SPACE) lstrcpy(name, TEXT("SPACE"));
    else if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) { name[0] = (TCHAR)key; name[1] = 0; }
    else if (key >= VK_F1 && key <= VK_F24) wsprintf(name, TEXT("F%d"), key - VK_F1 + 1);
    else wsprintf(name, TEXT("VK_%02X"), key);
    lstrcat(text, name);
    SetWindowText(capture_edit, text);
}

static LRESULT CALLBACK capture_proc(int code, WPARAM parameter, LPARAM data)
{
    KBDLLHOOKSTRUCT *key = (KBDLLHOOKSTRUCT *)data;
    BOOL down = parameter == WM_KEYDOWN || parameter == WM_SYSKEYDOWN;
    UINT modifier = 0;
    if (code < 0 || !IsWindow(capture_edit) || (key->flags & LLKHF_INJECTED))
        return CallNextHookEx(capture_hook, code, parameter, data);
    switch (key->vkCode) {
    case VK_LCONTROL: case VK_RCONTROL: case VK_CONTROL: modifier = MOD_CONTROL; break;
    case VK_LMENU: case VK_RMENU: case VK_MENU: modifier = MOD_ALT; break;
    case VK_LSHIFT: case VK_RSHIFT: case VK_SHIFT: modifier = MOD_SHIFT; break;
    case VK_LWIN: case VK_RWIN: modifier = MOD_WIN; break;
    }
    if (modifier) {
        if (down) capture_modifiers |= modifier;
        else capture_modifiers &= ~modifier;
    } else if (key->vkCode == VK_CAPITAL) capture_caps = down;
    else if (key->vkCode == VK_TAB && capture_modifiers == 0 && !capture_caps)
        return CallNextHookEx(capture_hook, code, parameter, data);
    else if (down && (capture_modifiers || capture_caps)) capture_key(key->vkCode);
    return 1;
}

static LRESULT CALLBACK hotkey_edit_proc(HWND window, UINT message, WPARAM parameter, LPARAM data,
    UINT_PTR identifier, DWORD_PTR main_window)
{
    UNREFERENCED_PARAMETER(identifier);
    if (message == WM_SETFOCUS) {
        SendMessage((HWND)main_window, WM_UNREGIST_HOTKEY, 0, 0);
        capture_edit = window;
        capture_caps = FALSE;
        capture_modifiers = 0;
        capture_hook = SetWindowsHookEx(WH_KEYBOARD_LL, capture_proc, GetModuleHandle(NULL), 0);
    } else if (message == WM_KILLFOCUS || message == WM_NCDESTROY) {
        if (capture_edit == window) {
            if (capture_hook) UnhookWindowsHookEx(capture_hook);
            capture_hook = NULL;
            capture_edit = NULL;
            PostMessage((HWND)main_window, WM_REGIST_HOTKEY, 0, 0);
        }
    } else if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && !capture_hook) {
        capture_modifiers = (GetKeyState(VK_CONTROL) < 0 ? MOD_CONTROL : 0) |
            (GetKeyState(VK_MENU) < 0 ? MOD_ALT : 0) | (GetKeyState(VK_SHIFT) < 0 ? MOD_SHIFT : 0) |
            (GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0 ? MOD_WIN : 0);
        capture_caps = GetKeyState(VK_CAPITAL) < 0;
        if ((capture_modifiers || capture_caps) && parameter != VK_CONTROL && parameter != VK_MENU &&
            parameter != VK_SHIFT && parameter != VK_LWIN && parameter != VK_RWIN && parameter != VK_CAPITAL)
            capture_key((UINT)parameter);
    }
    return DefSubclassProc(window, message, parameter, data);
}

void cm_hotkey_edit(HWND edit, HWND main_window)
{
    SetWindowSubclass(edit, hotkey_edit_proc, 1, (DWORD_PTR)main_window);
}

BOOL cm_hotkey_capturing(void)
{
    return capture_edit != NULL && IsWindow(capture_edit);
}