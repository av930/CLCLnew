#ifndef _INC_TILEVIEW_H
#define _INC_TILEVIEW_H

#include "Data.h"

#define WM_TILE_PASTE (WM_APP + 1140)
#define WM_TILE_FAVORITE (WM_APP + 1141)
#define WM_TILE_SEARCH (WM_APP + 1142)
#define WM_TILE_NEW (WM_APP + 1143)
#define WM_TILE_EDIT (WM_APP + 1144)
#define WM_TILE_DELETE (WM_APP + 1145)

HWND tileview_create(HINSTANCE instance, HWND parent, int identifier);
void tileview_refresh(HWND window, DATA_INFO *items);
void tileview_search(HWND window, const TCHAR *keyword);
void tileview_favorites(HWND window, BOOL enabled);
BOOL tileview_portrait_image(HWND window, DATA_INFO *item);
BOOL tileview_has_image(HWND window, DATA_INFO *item);

#endif