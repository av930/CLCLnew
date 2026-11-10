#ifndef _INC_TILEVIEW_H
#define _INC_TILEVIEW_H

#include "Data.h"

#define WM_TILE_PASTE (WM_APP + 1140)
#define WM_TILE_FAVORITE (WM_APP + 1141)
#define WM_TILE_SEARCH (WM_APP + 1142)
#define WM_TILE_NEW (WM_APP + 1143)
#define WM_TILE_EDIT (WM_APP + 1144)
#define WM_TILE_DELETE (WM_APP + 1145)
#define WM_TILE_SAMPLE (WM_APP + 1146)
#define WM_TILE_DEBUG_NEW (WM_APP + 1147)
#define WM_TILE_DEBUG_DELETE (WM_APP + 1148)

/*
 * Clip panel types. The number in the name orders the panels by height (Y) ascending, then width (X) ascending.
 * The value is the stored "kind" (favorite metadata "ClipMaster:tile:N") and is also the order of the size list
 * in the favorite editor, so changing it breaks saved favorites. Sizes are in pixels at 96 DPI;
 * kind 0 is the automatic size (clip history).
 */
#define CLIP_PANEL_TYPE1_W408_H24   1
#define CLIP_PANEL_TYPE2_W200_H48   2
#define CLIP_PANEL_TYPE3_W200_H96   3
#define CLIP_PANEL_TYPE4_W408_H96   4
#define CLIP_PANEL_TYPE5_W96_H200   5
#define CLIP_PANEL_TYPE6_W200_H200  6
#define CLIP_PANEL_TYPE_COUNT       6
/* Panels that hold an image only. */
#define CLIP_PANEL_IMAGE_ONLY(kind) ((kind) == CLIP_PANEL_TYPE5_W96_H200 || (kind) == CLIP_PANEL_TYPE6_W200_H200)

HWND tileview_create(HINSTANCE instance, HWND parent, int identifier);
void tileview_refresh(HWND window, DATA_INFO *items);
void tileview_search(HWND window, const TCHAR *keyword);
void tileview_favorites(HWND window, BOOL enabled);
void tileview_samples(HWND window, BOOL enabled);
void tileview_layout(HWND window, BOOL vertical);
BOOL tileview_portrait_image(HWND window, DATA_INFO *item);
BOOL tileview_has_image(HWND window, DATA_INFO *item);

#endif