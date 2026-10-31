#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <tchar.h>
#include "General.h"
#include "Ini.h"
#include "Config.h"
#include "History.h"
#include "Viewer.h"

extern OPTION_INFO option;
static BOOL restart_requested;

void cm_config_defaults(OPTION_INFO *settings)
{
    settings->main_clipboard_watch = 1;
    settings->main_show_trayicon = 1;
    settings->config_width_percent = 70;
    settings->config_height_percent = 90;
    settings->config_alignment = 1;
    settings->config_remember_layout = 1;
    settings->config_saved_geometry = 0;
    settings->config_last_tab = 0;
    settings->config_history_limit_enabled = 0;
    settings->config_history_days_enabled = 1;
    settings->config_history_days = 3;
    settings->history_max = 300;
    settings->history_save = 1;
    settings->history_always_save = 1;
    settings->config_favorite_max = 20;
    settings->config_clear_history_boot = 0;
    settings->config_clear_clipboard_startup = 0;
    settings->config_duplicate_policy = 2;
    settings->config_skip_passwords = 1;
    lstrcpy(settings->config_hotkey_clipmanager, TEXT("Alt+SPACE"));
    lstrcpy(settings->config_hotkey_favorites, TEXT("CapsLock+F"));
    lstrcpy(settings->config_hotkey_pasteplus, TEXT("CapsLock+V"));
    lstrcpy(settings->config_hotkey_paste, TEXT("Ctrl+V"));
    settings->config_exclude_text = 0;
    settings->config_exclude_richtext = 0;
    settings->config_exclude_image = 0;
    settings->config_exclude_file = 0;
    settings->config_exclude_audio = 1;
    settings->config_exclude_device = 1;
    settings->config_force_unicode = 1;
    settings->config_start_with_windows = 1;
    settings->config_encrypt_history = 0;
    settings->config_hide_tray_on_switch = 1;
    settings->config_handler_count = 3;
    ZeroMemory(settings->config_handlers, sizeof(settings->config_handlers));
    settings->config_handler_special[0] = 0;
    settings->config_handler_image[0] = 0;
    settings->config_handler_unicode[0] = 0;
}

BOOL cm_config_startup(BOOL enabled)
{
    HKEY key;
    TCHAR executable[MAX_PATH], command[MAX_PATH + 4], current[MAX_PATH + 4];
    DWORD size = sizeof(current), type = 0;
    LONG result;
    GetModuleFileName(NULL, executable, ARRAYSIZE(executable));
    wsprintf(command, TEXT("\"%s\""), executable);
    result = RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        0, NULL, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &key, NULL);
    if (result != ERROR_SUCCESS) return FALSE;
    if (enabled) result = RegSetValueEx(key, TEXT("ClipMaster"), 0, REG_SZ,
        (BYTE *)command, (lstrlen(command) + 1) * sizeof(TCHAR));
    else {
        result = RegQueryValueEx(key, TEXT("ClipMaster"), NULL, &type, (BYTE *)current, &size);
        if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
        else if (result == ERROR_SUCCESS && type == REG_SZ) {
            current[ARRAYSIZE(current) - 1] = 0;
            if (!lstrcmpi(current, command)) result = RegDeleteValue(key, TEXT("ClipMaster"));
        }
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

BOOL cm_config_new_boot(HWND main_window)
{
    typedef LONG (WINAPI *QUERY_SYSTEM)(ULONG, PVOID, ULONG, PULONG);
    struct { GUID identifier; ULONG firmware; ULONGLONG flags; } information;
    QUERY_SYSTEM query = (QUERY_SYSTEM)GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "NtQuerySystemInformation");
    TCHAR directory[MAX_PATH], path[MAX_PATH], temporary[MAX_PATH];
    HANDLE file;
    GUID previous;
    DWORD size;
    BOOL changed = TRUE, saved;
    if (query == NULL || query(90, &information, sizeof(information), NULL) < 0) return FALSE;
    SendMessage(main_window, WM_GET_WORKPATH, 0, (LPARAM)directory);
    if (lstrlen(directory) + 20 >= MAX_PATH) return FALSE;
    wsprintf(path, TEXT("%s\\clipmaster.boot"), directory);
    wsprintf(temporary, TEXT("%s.tmp"), path);
    file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        if (ReadFile(file, &previous, sizeof(previous), &size, NULL) && size == sizeof(previous))
            changed = memcmp(&previous, &information.identifier, sizeof(previous)) != 0;
        CloseHandle(file);
    }
    if (!changed) return FALSE;
    file = CreateFile(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    saved = WriteFile(file, &information.identifier, sizeof(GUID), &size, NULL) && size == sizeof(GUID) && FlushFileBuffers(file);
    CloseHandle(file);
    if (saved) saved = MoveFileEx(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!saved) DeleteFile(temporary);
    return saved;
}

void cm_runtime_initialize(HWND main_window)
{
    if (cm_config_new_boot(main_window) && option.config_clear_history_boot)
        SendMessage(main_window, WM_HISTORY_CLEAR, 0, 0);
    if (option.config_start_with_windows) cm_config_startup(TRUE);
    SetTimer(main_window, 8, 60000, NULL);
}

BOOL cm_runtime_message(HWND main_window, UINT message, WPARAM parameter, LPARAM data, LRESULT *result)
{
    HWND viewer = FindWindow(TEXT("CLCLViewer"), TEXT("ClipMaster"));
    *result = 0;
    if (message == WM_CONFIG_APPLY) {
        OPTION_INFO *previous = (OPTION_INFO *)data;
        BOOL success;
        if (previous == NULL) return TRUE;
        SendMessage(main_window, WM_UNREGIST_HOTKEY, 0, 0);
        success = (BOOL)SendMessage(main_window, WM_REGIST_HOTKEY, 0, 0);
        success = success && cm_config_startup(option.config_start_with_windows) && ini_put_option() &&
            SendMessage(main_window, WM_HISTORY_SAVE, 1, 0);
        if (!success) {
            SendMessage(main_window, WM_UNREGIST_HOTKEY, 0, 0);
            option = *previous;
            cm_config_startup(option.config_start_with_windows);
            ini_put_option();
            SendMessage(main_window, WM_REGIST_HOTKEY, 0, 0);
        }
        *result = success;
        return TRUE;
    }
    if (message == WM_CONFIG_RESET) {
        OPTION_INFO *previous = HeapAlloc(GetProcessHeap(), 0, sizeof(OPTION_INFO));
        BOOL success;
        if (previous == NULL) return TRUE;
        if (viewer != NULL) SendMessage(viewer, WM_CLOSE, 0, 0);
        *previous = option;
        cm_config_defaults(&option);
        success = (BOOL)SendMessage(main_window, WM_CONFIG_APPLY, 0, (LPARAM)previous);
        HeapFree(GetProcessHeap(), 0, previous);
        if (!success) return TRUE;
        message = WM_CONFIG_RESTART;
    }
    if (message == WM_CONFIG_RESTART) {
        restart_requested = TRUE;
        PostMessage(main_window, WM_CLOSE, 0, 0);
        *result = TRUE;
        return TRUE;
    }
    if (message == WM_TIMER && parameter == 8) {
        DATA_INFO *root = (DATA_INFO *)SendMessage(main_window, WM_HISTORY_GET_ROOT, 0, 0);
        DATA_INFO *item;
        int before = 0, after = 0;
        if (root == NULL) return TRUE;
        for (item = root->child; item != NULL; item = item->next) before++;
        if (viewer != NULL) SendMessage(viewer, WM_VIEWER_PREPARE_HISTORY, 0, 0);
        history_trim(&root->child);
        for (item = root->child; item != NULL; item = item->next) after++;
        if (before != after) {
            SendMessage(main_window, WM_HISTORY_CHANGED, 0, 0);
            SendMessage(main_window, WM_HISTORY_SAVE, 1, 0);
        }
        return TRUE;
    }
    return FALSE;
}

void cm_runtime_relaunch(void)
{
    if (restart_requested) {
        TCHAR executable[MAX_PATH];
        GetModuleFileName(NULL, executable, ARRAYSIZE(executable));
        ShellExecute(NULL, TEXT("open"), executable, TEXT("/cmshow"), NULL, SW_SHOWNORMAL);
    }
}

static int cm_runtime_requested_tab(void)
{
    int count, index, tab = -1;
    LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments == NULL) return -1;
    for (index = 1; index < count; index++) {
        if (!lstrcmpiW(arguments[index], L"/cmsettings")) tab = 3;
        else if (!lstrcmpiW(arguments[index], L"/cmshow") && tab < 0) tab = 0;
    }
    LocalFree(arguments);
    return tab;
}

BOOL cm_runtime_internal_command(void)
{
    return cm_runtime_requested_tab() >= 0;
}

void cm_runtime_show_requested(const TCHAR *command)
{
    int tab = cm_runtime_requested_tab();
    UNREFERENCED_PARAMETER(command);
    if (tab == 3) {
        SendMessage(FindWindow(MAIN_WND_CLASS, MAIN_WINDOW_TITLE), WM_OPTION_SHOW, 0, 0);
        return;
    }
    if (tab == 0) {
        HWND viewer = FindWindow(TEXT("CLCLViewer"), TEXT("ClipMaster"));
        if (viewer != NULL) ShowWindow(viewer, SW_SHOW);
        else SendMessage(FindWindow(MAIN_WND_CLASS, MAIN_WINDOW_TITLE), WM_COMMAND, 40002, 0);
    }
}