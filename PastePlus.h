#ifndef _INC_PASTEPLUS_H
#define _INC_PASTEPLUS_H
#include "Data.h"
#define WM_PASTEPLUS_PASTE (WM_APP + 1150)
#define WM_PASTEPLUS_COPY (WM_APP + 1151)
#define WM_PASTEPLUS_OCR_DONE (WM_APP + 1152)
#ifdef __cplusplus
extern "C" {
#endif
HWND pasteplus_create(HINSTANCE instance, HWND owner, int identifier);
HWND pasteplus_symbols_create(HINSTANCE instance, HWND owner, int identifier);
void pasteplus_refresh(HWND window, DATA_INFO *item);
#ifdef __cplusplus
}
#endif
#endif