#define _INC_OLE
#include <windows.h>
#undef _INC_OLE
#include <commctrl.h>
#include <tchar.h>
#include "General.h"
#include "Memory.h"
#include "String.h"
#include "Data.h"
#include "ClipBoard.h"
#include "TileView.h"
#include "DarkMode.h"
#include "ClipEdit.h"

typedef struct {
    DATA_INFO *working;
    BOOL changed;
    BOOL image;
    BOOL accepted;
    BOOL done;
    TCHAR *initial_text;
} CLIP_EDITOR;

static HWND editor_control(HWND owner, const TCHAR *type, const TCHAR *text, DWORD style, int id)
{
    HWND control = CreateWindowEx(lstrcmp(type, TEXT("EDIT")) == 0 ? WS_EX_CLIENTEDGE : 0,
        type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, owner, (HMENU)(INT_PTR)id,
        (HINSTANCE)GetWindowLongPtr(owner, GWLP_HINSTANCE), NULL);
    SendMessage(control, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    return control;
}

static BOOL editor_has_image(void)
{
    return IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_BITMAP) ||
        IsClipboardFormatAvailable(CF_TIFF) || IsClipboardFormatAvailable(RegisterClipboardFormat(TEXT("PNG"))) ||
        IsClipboardFormatAvailable(RegisterClipboardFormat(TEXT("JFIF"))) ||
        IsClipboardFormatAvailable(RegisterClipboardFormat(TEXT("image/png"))) ||
        IsClipboardFormatAvailable(RegisterClipboardFormat(TEXT("image/jpeg")));
}

static void editor_image(HWND window, CLIP_EDITOR *editor)
{
    DATA_INFO *item;
    TCHAR error[BUF_SIZE] = TEXT("");
    if (!OpenClipboard(window)) return;
    item = clipboard_to_item(error);
    CloseClipboard();
    if (item == NULL) return;
    data_free(editor->working);
    editor->working = item;
    editor->image = TRUE;
    editor->changed = FALSE;
    ShowWindow(GetDlgItem(window, 301), SW_HIDE);
    ShowWindow(GetDlgItem(window, 307), SW_SHOW);
    tileview_refresh(GetDlgItem(window, 307), item);
}

static LRESULT CALLBACK editor_text_proc(HWND window, UINT message, WPARAM parameter, LPARAM data,
    UINT_PTR identifier, DWORD_PTR reference)
{
    UNREFERENCED_PARAMETER(identifier);
    UNREFERENCED_PARAMETER(reference);
    if (message == WM_PASTE && editor_has_image()) {
        SendMessage(GetParent(window), WM_COMMAND, 302, 0);
        return 0;
    }
    return DefSubclassProc(window, message, parameter, data);
}

static LRESULT CALLBACK editor_proc(HWND window, UINT message, WPARAM parameter, LPARAM data)
{
    CLIP_EDITOR *editor = (CLIP_EDITOR *)GetWindowLongPtr(window, GWLP_USERDATA);
    RECT client;
    int scale = GetDpiForWindow(window);
    if (message == WM_NCCREATE) {
        editor = (CLIP_EDITOR *)((CREATESTRUCT *)data)->lpCreateParams;
        SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)editor);
    }
    if (editor == NULL) return DefWindowProc(window, message, parameter, data);
    switch (message) {
    case WM_SIZE:
        GetClientRect(window, &client);
        MoveWindow(GetDlgItem(window, 301), 12, 12, max(1, client.right - 24), max(1, client.bottom - 128), TRUE);
        MoveWindow(GetDlgItem(window, 307), 12, 12, max(1, client.right - 24), max(1, client.bottom - 128), TRUE);
        MoveWindow(GetDlgItem(window, 303), 12, max(12, client.bottom - 104), 144, 200, TRUE);
        MoveWindow(GetDlgItem(window, 302), 168, max(12, client.bottom - 104), 108, 28, TRUE);
        MoveWindow(GetDlgItem(window, 308), 284, max(12, client.bottom - 104), 76, 28, TRUE);
        MoveWindow(GetDlgItem(window, IDOK), max(12, client.right - 184), max(12, client.bottom - 40), 80, 28, TRUE);
        MoveWindow(GetDlgItem(window, IDCANCEL), max(100, client.right - 96), max(12, client.bottom - 40), 80, 28, TRUE);
        MoveWindow(GetDlgItem(window, 304), 12, max(12, client.bottom - 72), max(1, client.right - 24), 24, TRUE);
        return 0;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)data)->ptMinTrackSize.x = MulDiv(420, scale, 96);
        ((MINMAXINFO *)data)->ptMinTrackSize.y = MulDiv(320, scale, 96);
        return 0;
    case WM_COMMAND:
        if (LOWORD(parameter) == 301 && HIWORD(parameter) == EN_CHANGE) editor->changed = TRUE;
        if (LOWORD(parameter) == 302) {
            if (editor_has_image()) editor_image(window, editor);
            else SetDlgItemText(window, 304, TEXT("The clipboard does not contain an image."));
        }
        if (LOWORD(parameter) == 308) {
            editor->image = FALSE;
            editor->changed = TRUE;
            ShowWindow(GetDlgItem(window, 307), SW_HIDE);
            ShowWindow(GetDlgItem(window, 301), SW_SHOW);
            SetFocus(GetDlgItem(window, 301));
        }
        if (LOWORD(parameter) == IDOK) {
            if (!editor->image) {
                int length = GetWindowTextLength(GetDlgItem(window, 301));
                TCHAR *current = mem_alloc((length + 1) * sizeof(TCHAR));
                if (current == NULL) return 0;
                GetDlgItemText(window, 301, current, length + 1);
                editor->changed = editor->initial_text == NULL || lstrcmp(current, editor->initial_text) != 0;
                mem_free(&current);
            }
            if (!editor->image && (editor->changed || editor->working->child == NULL)) {
                int length = GetWindowTextLength(GetDlgItem(window, 301));
                HGLOBAL memory;
                TCHAR *text;
                DATA_INFO *format;
                if (length == 0) {
                    SetDlgItemText(window, 304, TEXT("Enter text or paste an image."));
                    return 0;
                }
                memory = GlobalAlloc(GMEM_MOVEABLE, (length + 1) * sizeof(TCHAR));
                text = memory ? GlobalLock(memory) : NULL;
                if (text == NULL) { if (memory) GlobalFree(memory); return 0; }
                GetDlgItemText(window, 301, text, length + 1);
                GlobalUnlock(memory);
                format = data_create_data(CF_UNICODETEXT, TEXT("UNICODE TEXT"), memory,
                    (length + 1) * sizeof(TCHAR), FALSE, NULL);
                if (format == NULL) { GlobalFree(memory); return 0; }
                data_free(editor->working->child);
                editor->working->child = format;
                mem_free(&editor->working->title);
            }
            {
                TCHAR metadata[32];
                int kind = (int)SendDlgItemMessage(window, 303, CB_GETCURSEL, 0, 0) + 1;
                wsprintf(metadata, TEXT("ClipMaster:tile:%d"), kind);
                if (editor->working->plugin_string == NULL ||
                    _tcsncmp(editor->working->plugin_string, TEXT("ClipMaster:tile:"), 16) == 0) {
                    mem_free(&editor->working->plugin_string);
                    editor->working->plugin_string = alloc_copy(metadata);
                }
            }
            data_set_modified(editor->working);
            editor->accepted = TRUE;
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(parameter) == IDCANCEL) DestroyWindow(window);
        return 0;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        editor->done = TRUE;
        PostThreadMessage(GetCurrentThreadId(), WM_NULL, 0, 0);
        return 0;
    }
    return DefWindowProc(window, message, parameter, data);
}

DATA_INFO *clipedit_show(HINSTANCE instance, HWND owner, DATA_INFO *original, int kind)
{
    CLIP_EDITOR editor;
    WNDCLASS descriptor;
    HWND window, text;
    DATA_INFO *format;
    MSG message;
    const TCHAR *sizes[] = { TEXT("96 x 96"), TEXT("192 x 96"), TEXT("384 x 96"), TEXT("192 x 192") };
    int index;
    ZeroMemory(&editor, sizeof(editor));
    editor.working = original ? data_item_copy(original, FALSE, FALSE, NULL) : data_create_item(NULL, TRUE, NULL);
    if (editor.working == NULL) return NULL;
    ZeroMemory(&descriptor, sizeof(descriptor));
    descriptor.hInstance = instance;
    descriptor.lpszClassName = TEXT("ClipMasterEditor");
    descriptor.lpfnWndProc = editor_proc;
    descriptor.hCursor = LoadCursor(NULL, IDC_ARROW);
    descriptor.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClass(&descriptor);
    window = CreateWindowEx(WS_EX_CONTROLPARENT | WS_EX_DLGMODALFRAME, descriptor.lpszClassName,
        TEXT("Favorite Clip"), WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX),
        CW_USEDEFAULT, CW_USEDEFAULT, 560, 440, owner, NULL, instance, &editor);
    if (window == NULL) { data_free(editor.working); return NULL; }
    text = editor_control(window, TEXT("EDIT"), TEXT(""),
        ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_TABSTOP, 301);
    SendMessage(text, EM_SETLIMITTEXT, 1048576, 0);
    SetWindowSubclass(text, editor_text_proc, 1, 0);
    editor_control(window, TEXT("BUTTON"), TEXT("Paste Image"), BS_PUSHBUTTON | WS_TABSTOP, 302);
    editor_control(window, TEXT("BUTTON"), TEXT("Text"), BS_PUSHBUTTON | WS_TABSTOP, 308);
    editor_control(window, WC_COMBOBOX, TEXT(""), CBS_DROPDOWNLIST | WS_TABSTOP, 303);
    editor_control(window, TEXT("STATIC"), TEXT(""), 0, 304);
    editor_control(window, TEXT("BUTTON"), TEXT("Save"), BS_DEFPUSHBUTTON | WS_TABSTOP, IDOK);
    editor_control(window, TEXT("BUTTON"), TEXT("Cancel"), BS_PUSHBUTTON | WS_TABSTOP, IDCANCEL);
    tileview_create(instance, window, 307);
    for (index = 0; index < 4; index++) SendDlgItemMessage(window, 303, CB_ADDSTRING, 0, (LPARAM)sizes[index]);
    SendDlgItemMessage(window, 303, CB_SETCURSEL, kind >= 1 && kind <= 4 ? kind - 1 : 0, 0);
    editor.image = original != NULL;
    for (format = editor.working->child; format != NULL; format = format->next) {
        if (format->format == CF_UNICODETEXT) {
            TCHAR *contents = GlobalLock(format->data);
            if (contents != NULL) { SetWindowText(text, contents); GlobalUnlock(format->data); }
            editor.image = FALSE;
            break;
        }
    }
    editor.changed = FALSE;
    {
        int length = GetWindowTextLength(text);
        editor.initial_text = mem_alloc((length + 1) * sizeof(TCHAR));
        if (editor.initial_text != NULL) GetWindowText(text, editor.initial_text, length + 1);
    }
    ShowWindow(text, editor.image ? SW_HIDE : SW_SHOW);
    ShowWindow(GetDlgItem(window, 307), editor.image ? SW_SHOW : SW_HIDE);
    if (editor.image) tileview_refresh(GetDlgItem(window, 307), editor.working);
    dark_mode_set_window(window);
    EnableWindow(owner, FALSE);
    ShowWindow(window, SW_SHOW);
    SendMessage(window, WM_SIZE, 0, 0);
    SetFocus(text);
    while (!editor.done) {
        int result = GetMessage(&message, NULL, 0, 0);
        if (result <= 0) { if (result == 0) PostQuitMessage((int)message.wParam); break; }
        if (!IsDialogMessage(window, &message)) { TranslateMessage(&message); DispatchMessage(&message); }
    }
    if (IsWindow(window)) DestroyWindow(window);
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    mem_free(&editor.initial_text);
    if (!editor.accepted) { data_free(editor.working); return NULL; }
    return editor.working;
}