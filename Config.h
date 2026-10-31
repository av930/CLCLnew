#ifndef _INC_CM_CONFIG_H
#define _INC_CM_CONFIG_H
#include "Ini.h"
void cm_config_defaults(OPTION_INFO *settings);
BOOL cm_config_startup(BOOL enabled);
BOOL cm_config_new_boot(HWND main_window);
void cm_runtime_initialize(HWND main_window);
BOOL cm_runtime_message(HWND main_window, UINT message, WPARAM parameter, LPARAM data, LRESULT *result);
void cm_runtime_relaunch(void);
void cm_runtime_show_requested(const TCHAR *command);
BOOL cm_runtime_internal_command(void);
#endif