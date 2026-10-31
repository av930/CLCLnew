#ifndef _INC_PASTEPLUS_H
#define _INC_PASTEPLUS_H
#include "Data.h"
#define WM_PASTEPLUS_PASTE (WM_APP + 1150)
#ifdef __cplusplus
extern "C" {
#endif
HWND pasteplus_create(HINSTANCE instance, HWND owner, int identifier);
void pasteplus_refresh(HWND window, DATA_INFO *item);
#ifdef __cplusplus
}
#endif
#endif