/*
 * CLCL
 *
 * Viewer.c
 *
 * Copyright (C) 1996-2022 by Ohno Tomoaki. All rights reserved.
 *		https://www.nakka.com/
 *		nakka@nakka.com
 */

/* Include Files */
#define _INC_OLE
#include <windows.h>
#undef  _INC_OLE
#include <commctrl.h>
#include <commdlg.h>
#include <tchar.h>

#pragma comment(lib, "Version.lib")

#include "General.h"
#include "Memory.h"
#include "String.h"
#include "Data.h"
#include "Ini.h"
#include "Message.h"
#include "Menu.h"
#include "File.h"
#include "ClipBoard.h"
#include "History.h"
#include "Regist.h"
#include "Format.h"
#include "Filter.h"
#include "Tool.h"
#include "Viewer.h"
#include "Frame.h"
#include "ImageList.h"
#include "TreeView.h"
#include "Container.h"
#include "ListView.h"
#include "BinView.h"
#include "ToolBar.h"
#include "StatusBar.h"
#include "SelectFolder.h"
#include "SelectFormat.h"
#include "SetHotkey.h"
#include "OleDragDrop.h"
#include "dpi.h"
#include "DarkMode.h"
#include "ViewerOLEDnD.h"
#include "ViewerDnD.h"
#include "TileView.h"
#include "SendKey.h"
#include "ClipEdit.h"
#include "PastePlus.h"
#include "Hotkey.h"
#include "Config.h"

#include "resource.h"

/* Define */
#define WINDOW_CLASS					TEXT("CLCLViewer")
#define WINDOW_TITLE					TEXT("ClipMaster")
#define ID_CLIPMASTER_TABS 120
#define ID_CLIPMASTER_SETTINGS 121
#define ID_CLIPMASTER_PANEL 122
#define ID_CLIPMASTER_SAVE 123
#define ID_CLIPMASTER_STATUS 124
#define ID_CLIPMASTER_TILES 125
#define ID_CLIPMASTER_SEARCH 126
#define ID_CLIPMASTER_FIND 127
#define ID_CLIPMASTER_FAVORITES 128
#define ID_CLIPMASTER_PASTEPLUS 129
#define ID_CLIPMASTER_SEARCH_LABEL 130
#define ID_CLIPMASTER_FIELD 5100
#define CLIPMASTER_SETTINGS_HEIGHT settings_height
#define ERROR_TITLE						TEXT("CLCL - Error")

// タイマーID
#define TIMER_SEP						1
#define TIMER_DRAG						2
#define TIMER_SET_MENU					3
#define TIMER_REFLECT_DATA				4
#define TIMER_LV_REFRESH				5
#define TIMER_TV_ACTION					6

#define POPUPMENU_CLIPBOARD				0
#define POPUPMENU_HISTORY				1
#define POPUPMENU_HISTORY_LV			2
#define POPUPMENU_REGIST				3
#define POPUPMENU_REGIST_LV				4
#define POPUPMENU_DRAGDROP				5

#define WINDOW_MENU_FILE				0
#define WINDOW_MENU_EDIT				1
#define WINDOW_MENU_VIEW				2
#define WINDOW_MENU_TOOL				3

#define ID_MENUITEM_TOOL				(WM_APP + 1000)

/* Global Variables */
static HWND main_wnd;
static HWND current_wnd;
static HWND paste_target;
static HWND paste_focus;
static int settings_height = 1600;
static int settings_handler_top;
static int settings_handler_count;
HMENU h_popup_menu;

HTREEITEM clip_treeitem;
HTREEITEM history_treeitem;
HTREEITEM regist_treeitem;
DATA_INFO clip_di;

static BOOL save_flag = TRUE;
static BOOL DnD_mode;

// extern
extern HINSTANCE hInst;
extern DATA_INFO history_data;
extern DATA_INFO regist_data;

// オプション
extern OPTION_INFO option;

/* Local Function Prototypes */
static void set_cursor(const BOOL wiat_flag);
static void set_enable_window_menu(const HWND hWnd);
static void set_enadle_popup_menu(const HWND hWnd, const HMENU hMenu, const int index, const HTREEITEM hItem);
static void viewer_show_menu(const HWND hWnd);
static void viewer_show_item(const HWND hWnd);
static void viewer_item_activate(const HWND hWnd);
static void treeview_to_clipboard(const HWND hWnd, const HTREEITEM sel_item);
static void viewer_regist_add(const HWND hWnd, const HTREEITEM sel_item, const BOOL regist_move);
static void viewer_favorites_trim(const HWND hWnd);
static BOOL viewer_move_up(const HWND hWnd, const HTREEITEM sel_item);
static BOOL viewer_move_down(const HWND hWnd, const HTREEITEM sel_item);
static void viewer_create_item(const HWND hWnd, const HTREEITEM sel_item);
static void viewer_create_folder(const HWND hWnd, const HTREEITEM sel_item);
static void viewer_data_save(const HWND hWnd, const HTREEITEM sel_item);
static BOOL viewer_import_item(const HWND hWnd);
static BOOL viewer_export_item(const HWND hWnd);
static BOOL viewer_rename(const HWND hWnd, const HTREEITEM sel_item, TCHAR *title);
static BOOL viewer_clear_name(const HWND hWnd, const HTREEITEM sel_item);
static BOOL viewer_set_hotkey(const HWND hWnd, const HTREEITEM sel_item);
static void viewer_delete_item(const HWND hWnd, const HTREEITEM sel_item);
static BOOL viewer_format_to_option(const HWND hWnd, const HTREEITEM sel_item, const TCHAR *mode);
static void viewer_tool_execute(const HWND hWnd, const HTREEITEM sel_item, const int index);
static void viewer_set_datetime(const HWND hWnd, const HTREEITEM hItem);
static DATA_INFO *clipboard_to_datainfo(const HWND hWnd);
static void viewer_save_data(const HWND hWnd, const HTREEITEM hItem);
static void viewer_set_list_column(const HWND hTreeView, const HWND hListView, const HTREEITEM hItem);
static BOOL viewer_sel_cheange(const HWND hWnd, const HTREEITEM old_item, const HTREEITEM new_item);
static BOOL viewer_initialize(const HWND hWnd);
static void viewer_set_controls(const HWND hWnd);
static void viewer_select_tab(const HWND hWnd, int index);
static void viewer_create_settings(const HWND hWnd);
static BOOL viewer_save_settings(const HWND hWnd);
static void viewer_reset_dpi(const HWND hWnd);
static BOOL viewer_close(const HWND hWnd);
static LRESULT CALLBACK viewer_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/*
 * set_cursor - カーソルを設定
 */
static void set_cursor(const BOOL wiat_flag)
{
	static HCURSOR old_cursor;

	if (wiat_flag == TRUE) {
		// 砂時計カーソルにする
		old_cursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
	} else {
		if (old_cursor == NULL) {
			SetCursor(LoadCursor(NULL, IDC_ARROW));
		} else {
			// 元のカーソルに戻す
			SetCursor(old_cursor);
			old_cursor = NULL;
		}
	}
}

/*
 * set_enable_window_menu - メニュー項目の使用可能､ 使用不能を設定
 */
static void set_enable_window_menu(const HWND hWnd)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM hItem;
	HTREEITEM wk_item;
	DATA_INFO *di;
	UINT enable;
	BOOL tb_enable;
	BOOL sel_flag;
	BOOL folder_flag;
	int i;

	if (GetFocus() == hListView) {
		sel_flag = (ListView_GetSelectedCount(hListView) != 0) ? TRUE : FALSE;
		folder_flag = FALSE;
		i = -1;
		while ((i = ListView_GetNextItem(hListView, i, LVNI_SELECTED)) != -1) {
			wk_item = (HTREEITEM)listview_get_lparam(hListView, i);
			if (wk_item != NULL &&
				(di = (DATA_INFO *)treeview_get_lparam(hTreeView, wk_item)) != NULL &&
				di->type == TYPE_FOLDER) {
				folder_flag = TRUE;
				break;
			}
		}
		// menu
		enable = (sel_flag == FALSE || folder_flag == TRUE) ? MF_GRAYED : MF_ENABLED;
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_FILE), ID_MENUITEM_DATA_SAVE, enable);
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_EDIT), ID_MENUITEM_CLIPBOARD, enable);

		enable = (regist_treeitem == NULL) ? MF_GRAYED : MF_ENABLED;
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_FILE), ID_MENUITEM_IMPORT, enable);
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_FILE), ID_MENUITEM_EXPORT, enable);

		enable = (sel_flag == FALSE || regist_treeitem == NULL) ? MF_GRAYED : MF_ENABLED;
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_EDIT), ID_MENUITEM_REGIST_ADD, enable);

		enable = (sel_flag == FALSE) ? MF_GRAYED : MF_ENABLED;
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_EDIT), ID_MENUITEM_UP, enable);
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_EDIT), ID_MENUITEM_DOWN, enable);
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_EDIT), ID_MENUITEM_DELETE, enable);

		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_EDIT), ID_MENUITEM_PASTE, MF_ENABLED);
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_FILE), ID_MENUITEM_NEW_ITEM, MF_ENABLED);
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_EDIT), ID_MENUITEM_SELECT_ALL, MF_ENABLED);

		// tool bar
		tb_enable = (sel_flag == FALSE || folder_flag == TRUE) ? FALSE : TRUE;
		SendDlgItemMessage(hWnd, ID_TOOLBAR, TB_ENABLEBUTTON, ID_MENUITEM_CLIPBOARD_TB, (LPARAM)MAKELONG(tb_enable, 0));

		tb_enable = (sel_flag == FALSE || regist_treeitem == NULL) ? FALSE : TRUE;
		SendDlgItemMessage(hWnd, ID_TOOLBAR, TB_ENABLEBUTTON, ID_MENUITEM_REGIST_ADD_TB, (LPARAM)MAKELONG(tb_enable, 0));

		tb_enable = (sel_flag == FALSE) ? FALSE : TRUE;
		SendDlgItemMessage(hWnd, ID_TOOLBAR, TB_ENABLEBUTTON, ID_MENUITEM_DELETE_TB, (LPARAM)MAKELONG(tb_enable, 0));
		SendDlgItemMessage(hWnd, ID_TOOLBAR, TB_ENABLEBUTTON, ID_MENUITEM_UP_TB, (LPARAM)MAKELONG(tb_enable, 0));
		SendDlgItemMessage(hWnd, ID_TOOLBAR, TB_ENABLEBUTTON, ID_MENUITEM_DOWN_TB, (LPARAM)MAKELONG(tb_enable, 0));

		SendDlgItemMessage(hWnd, ID_TOOLBAR, TB_ENABLEBUTTON, ID_MENUITEM_PASTE_TB, (LPARAM)MAKELONG(TRUE, 0));
		SendDlgItemMessage(hWnd, ID_TOOLBAR, TB_ENABLEBUTTON, ID_MENUITEM_NEW_ITEM_TB, (LPARAM)MAKELONG(TRUE, 0));

	} else {
		hItem = TreeView_GetSelection(hTreeView);
		folder_flag = TRUE;
		if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) != NULL) {
			folder_flag = (di->type == TYPE_FOLDER) ? TRUE : FALSE;
		}
		// menu
		enable = (treeview_get_rootitem(hTreeView, hItem) == clip_treeitem) ? MF_GRAYED : MF_ENABLED;
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_FILE), ID_MENUITEM_NEW_ITEM, enable);
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_EDIT), ID_MENUITEM_SELECT_ALL, enable);

		enable = (regist_treeitem == NULL) ? MF_GRAYED : MF_ENABLED;
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_FILE), ID_MENUITEM_IMPORT, enable);
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_FILE), ID_MENUITEM_EXPORT, enable);

		enable = (hItem == history_treeitem || hItem == regist_treeitem || folder_flag == TRUE) ? MF_GRAYED : MF_ENABLED;
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_FILE), ID_MENUITEM_DATA_SAVE, enable);

		enable = (treeview_get_rootitem(hTreeView, hItem) == clip_treeitem ||
			hItem == history_treeitem || hItem == regist_treeitem || folder_flag == TRUE) ? MF_GRAYED : MF_ENABLED;
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_EDIT), ID_MENUITEM_CLIPBOARD, enable);

		enable = (treeview_get_rootitem(hTreeView, hItem) == clip_treeitem || folder_flag == FALSE) ? MF_GRAYED : MF_ENABLED;
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_EDIT), ID_MENUITEM_PASTE, enable);

		enable = (hItem == history_treeitem || hItem == regist_treeitem || regist_treeitem == NULL) ? MF_GRAYED : MF_ENABLED;
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_EDIT), ID_MENUITEM_REGIST_ADD, enable);

		enable = (treeview_get_rootitem(hTreeView, hItem) == clip_treeitem ||
			hItem == history_treeitem || hItem == regist_treeitem) ? MF_GRAYED : MF_ENABLED;
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_EDIT), ID_MENUITEM_UP, enable);
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_EDIT), ID_MENUITEM_DOWN, enable);
		EnableMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_EDIT), ID_MENUITEM_DELETE, enable);

		// tool bar
		tb_enable = (treeview_get_rootitem(hTreeView, hItem) == clip_treeitem ||
			hItem == history_treeitem || hItem == regist_treeitem || folder_flag == TRUE) ? FALSE : TRUE;
		SendDlgItemMessage(hWnd, ID_TOOLBAR, TB_ENABLEBUTTON, ID_MENUITEM_CLIPBOARD_TB, (LPARAM)MAKELONG(tb_enable, 0));

		tb_enable = (treeview_get_rootitem(hTreeView, hItem) == clip_treeitem || folder_flag == FALSE) ? FALSE : TRUE;
		SendDlgItemMessage(hWnd, ID_TOOLBAR, TB_ENABLEBUTTON, ID_MENUITEM_PASTE_TB, (LPARAM)MAKELONG(tb_enable, 0));

		tb_enable = (hItem == history_treeitem || hItem == regist_treeitem || regist_treeitem == NULL) ? FALSE : TRUE;
		SendDlgItemMessage(hWnd, ID_TOOLBAR, TB_ENABLEBUTTON, ID_MENUITEM_REGIST_ADD_TB, (LPARAM)MAKELONG(tb_enable, 0));

		tb_enable = (treeview_get_rootitem(hTreeView, hItem) == clip_treeitem) ? FALSE : TRUE;
		SendDlgItemMessage(hWnd, ID_TOOLBAR, TB_ENABLEBUTTON, ID_MENUITEM_NEW_ITEM_TB, (LPARAM)MAKELONG(tb_enable, 0));

		tb_enable = (treeview_get_rootitem(hTreeView, hItem) == clip_treeitem ||
			hItem == history_treeitem || hItem == regist_treeitem) ? FALSE : TRUE;
		SendDlgItemMessage(hWnd, ID_TOOLBAR, TB_ENABLEBUTTON, ID_MENUITEM_DELETE_TB, (LPARAM)MAKELONG(tb_enable, 0));
		SendDlgItemMessage(hWnd, ID_TOOLBAR, TB_ENABLEBUTTON, ID_MENUITEM_UP_TB, (LPARAM)MAKELONG(tb_enable, 0));
		SendDlgItemMessage(hWnd, ID_TOOLBAR, TB_ENABLEBUTTON, ID_MENUITEM_DOWN_TB, (LPARAM)MAKELONG(tb_enable, 0));
	}
}

/*
 * set_enadle_popup_menu - メニュー項目の使用可能､ 使用不能を設定
 */
static void set_enadle_popup_menu(const HWND hWnd, const HMENU hMenu, const int index, const HTREEITEM hItem)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM menu_item = NULL;
	HTREEITEM wk_item;
	DATA_INFO *di;
	UINT enable;
	BOOL sel_flag;
	BOOL folder_flag;
	int i;
	int def_menu = -1;

	switch (index) {
	case POPUPMENU_CLIPBOARD:
		// clipboard
		enable = (regist_treeitem != NULL) ? MF_ENABLED : MF_GRAYED;
		EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_REGIST_ADD, enable);

		enable = (clip_treeitem != hItem) ? MF_ENABLED : MF_GRAYED;
		EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_SET_FORMAT, enable);
		EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_SET_FILTER, enable);
		return;

	case POPUPMENU_HISTORY_LV:
		def_menu = option.list_default_action;
	case POPUPMENU_HISTORY:
		// history
		menu_item = history_treeitem;
		break;

	case POPUPMENU_REGIST_LV:
		def_menu = option.list_default_action;
	case POPUPMENU_REGIST:
		// regist
		menu_item = regist_treeitem;
		break;
	}
	// デフォルトメニュー設定
	switch (def_menu) {
	case -1:
		break;
	case 0:
	default:
		SetMenuDefaultItem(GetSubMenu(hMenu, index), ID_MENUITEM_OPEN, 0);
		break;
	case 1:
		SetMenuDefaultItem(GetSubMenu(hMenu, index), ID_MENUITEM_CLIPBOARD, 0);
		break;
	case 2:
		SetMenuDefaultItem(GetSubMenu(hMenu, index), ID_MENUITEM_DATA_SAVE, 0);
		break;
	}
	if (GetFocus() == hTreeView) {
		sel_flag = (hItem != menu_item) ? TRUE : FALSE;
		enable = MF_GRAYED;
		if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) != NULL) {
			folder_flag = (di->type == TYPE_FOLDER) ? TRUE : FALSE;
			enable = (di->type == TYPE_DATA) ? MF_ENABLED : MF_GRAYED;
		}
		EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_SET_FORMAT, enable);
		EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_SET_FILTER, enable);

		enable = MF_ENABLED;
		if (hItem != menu_item && (di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) != NULL) {
			enable = (di->type == TYPE_FOLDER) ? MF_ENABLED : MF_GRAYED;
		}
		EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_PASTE, enable);

	} else if (GetFocus() == hListView) {
		sel_flag = (ListView_GetSelectedCount(hListView) != 0) ? TRUE : FALSE;
		folder_flag = FALSE;
		i = -1;
		while ((i = ListView_GetNextItem(hListView, i, LVNI_SELECTED)) != -1) {
			if ((wk_item = (HTREEITEM)listview_get_lparam(hListView, i)) != NULL &&
				(di = (DATA_INFO *)treeview_get_lparam(hTreeView, wk_item)) != NULL &&
				di->type == TYPE_FOLDER) {
				folder_flag = TRUE;
				break;
			}
		}
		EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_PASTE, MF_ENABLED);
	}
	enable = (sel_flag == TRUE && folder_flag == FALSE) ? MF_ENABLED : MF_GRAYED;
	EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_CLIPBOARD, enable);
	EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_CLEAR_NAME, enable);
	EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_SET_HOTKEY, enable);
	EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_DATA_SAVE, enable);

	enable = (sel_flag == TRUE) ? MF_ENABLED : MF_GRAYED;
	EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_OPEN, enable);
	EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_RENAME, enable);
	EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_DELETE, enable);
	EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_UP, enable);
	EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_DOWN, enable);
	EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_REGIST_ADD, enable);
	EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_REGIST_MOVE, enable);

	enable = (sel_flag == TRUE && regist_treeitem != NULL) ? MF_ENABLED : MF_GRAYED;
	EnableMenuItem(GetSubMenu(hMenu, index), ID_MENUITEM_REGIST_ADD, enable);
}

/*
 * viewer_show_menu - メニュー表示
 */
static void viewer_show_menu(const HWND hWnd)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HTREEITEM hItem;
	HTREEITEM sel_item;
	int offset = 0;
	int ret;

	if (GetFocus() == hTreeView) {
		// TreeView
		if ((sel_item = (HTREEITEM)SendMessage(hTreeView, TVM_GETNEXTITEM, TVGN_DROPHILITE, 0)) == NULL) {
			sel_item = TreeView_GetSelection(hTreeView);
		}
	} else if (GetFocus() == GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST)) {
		// ListView
		sel_item = TreeView_GetSelection(hTreeView);
		offset = 1;
	} else {
		return;
	}

	if ((hItem = treeview_get_rootitem(hTreeView, sel_item)) == clip_treeitem) {
		// clipboard
		set_enadle_popup_menu(hWnd, h_popup_menu, POPUPMENU_CLIPBOARD, sel_item);
		_SetForegroundWindow(hWnd);
		ret = menu_show(hWnd, GetSubMenu(h_popup_menu, POPUPMENU_CLIPBOARD), NULL);
	} else if (hItem == history_treeitem) {
		// history
		set_enadle_popup_menu(hWnd, h_popup_menu, POPUPMENU_HISTORY + offset, sel_item);
		_SetForegroundWindow(hWnd);
		ret = menu_show(hWnd, GetSubMenu(h_popup_menu, POPUPMENU_HISTORY + offset), NULL);
	} else if (hItem == regist_treeitem) {
		// regist
		set_enadle_popup_menu(hWnd, h_popup_menu, POPUPMENU_REGIST + offset, sel_item);
		_SetForegroundWindow(hWnd);
		ret = menu_show(hWnd, GetSubMenu(h_popup_menu, POPUPMENU_REGIST + offset), NULL);
	}
	if (ret <= 0 || ret == IDCANCEL) {
		return;
	}
	SendMessage(hWnd, WM_COMMAND, ret, (LPARAM)sel_item);
}

/*
 * viewer_show_item - データを表示
 */
static void viewer_show_item(const HWND hWnd)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM hItem;

	if (GetFocus() == hListView) {
		if (ListView_GetNextItem(hListView, -1, LVNI_FOCUSED | LVNI_SELECTED) == -1) {
			return;
		}
		hItem = (HTREEITEM)listview_get_lparam(hListView,
			ListView_GetNextItem(hListView, -1, LVNI_FOCUSED | LVNI_SELECTED));
		if (hItem == NULL) {
			return;
		}
	} else {
		return;
	}
	TreeView_SelectItem(hTreeView, hItem);
	SetFocus(GetDlgItem(hWnd, ID_CONTAINER));
}

/*
 * viewer_item_activate - リストビューのアイテム選択操作
 */
static void viewer_item_activate(const HWND hWnd)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM hItem;
	DATA_INFO *di;
	BOOL lv_focus = FALSE;

	if (GetFocus() == hListView) {
		if (ListView_GetNextItem(hListView, -1, LVNI_FOCUSED | LVNI_SELECTED) == -1) {
			return;
		}
		hItem = (HTREEITEM)listview_get_lparam(hListView,
			ListView_GetNextItem(hListView, -1, LVNI_FOCUSED | LVNI_SELECTED));
		if (hItem == NULL) {
			return;
		}
	} else {
		return;
	}

	if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) == NULL) {
		return;
	}
	if (di->type == TYPE_FOLDER) {
		viewer_show_item(hWnd);
		return;
	}
	switch (option.list_default_action) {
	case 0:
	default:
		// 表示
		viewer_show_item(hWnd);
		break;
	case 1:
		// クリップボードに送る
		treeview_to_clipboard(hWnd, NULL);
		break;
	case 2:
		// 名前を付けて保存
		viewer_data_save(hWnd, hItem);
		break;
	}
}

/*
 * treeview_to_clipboard - データをクリップボードに送信
 */
static void treeview_to_clipboard(const HWND hWnd, const HTREEITEM sel_item)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM hItem;
	DATA_INFO *di;
	BOOL lv_focus = FALSE;

	if (GetFocus() == hListView) {
		lv_focus = TRUE;
		if (ListView_GetNextItem(hListView, -1, LVNI_FOCUSED | LVNI_SELECTED) == -1) {
			return;
		}
		hItem = (HTREEITEM)listview_get_lparam(hListView,
			ListView_GetNextItem(hListView, -1, LVNI_FOCUSED | LVNI_SELECTED));
		if (hItem == NULL) {
			return;
		}
	} else {
		SetFocus(hTreeView);

		if ((hItem = sel_item) == NULL) {
			hItem = TreeView_GetSelection(hTreeView);
		}
		if (treeview_get_rootitem(hTreeView, hItem) == clip_treeitem) {
			return;
		}
	}
	if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) == NULL || di->type == TYPE_FOLDER) {
		return;
	}
	set_cursor(TRUE);
	SendMessage(hWnd, WM_ITEM_TO_CLIPBOARD, 0, (LPARAM)di);
	set_cursor(FALSE);
}

/*
 * viewer_item_copy - アイテムをコピー
 */
HTREEITEM viewer_item_copy(const HWND hWnd, HTREEITEM from_item, HTREEITEM to_item, const BOOL move_flag, TCHAR *err_str)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HTREEITEM hItem;
	DATA_INFO *di, *wk_di, *prev_di;
	DATA_INFO **pdi;
	TCHAR buf[BUF_SIZE];
	int i;

	if (from_item == to_item) {
		// 同一フォルダ
		return NULL;
	}
	hItem = to_item;
	while ((hItem = TreeView_GetParent(hTreeView, hItem)) != NULL) {
		if (hItem == from_item) {
			// コピー先がコピー元のサブフォルダ
			return NULL;
		}
	}

	// コピーの作成
	if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, from_item)) == NULL) {
		return NULL;
	}
	if (di->type == TYPE_DATA) {
		// コピー元データを含むアイテムを作成
		if ((wk_di = data_create_item(NULL, TRUE, err_str)) == NULL) {
			return NULL;
		}
		if ((wk_di->child = data_item_copy(di, FALSE, move_flag, err_str)) == NULL) {
			mem_free(&wk_di);
			return NULL;
		}
	} else {
		// コピー
		wk_di = data_item_copy(di, FALSE, move_flag, err_str);
	}
	if (wk_di == NULL) {
		return NULL;
	}

	pdi = NULL;
	if (to_item == clip_treeitem) {
		if (wk_di->type == TYPE_FOLDER) {
			data_free(wk_di);
			return NULL;
		}
		// データをクリップボードに設定
		SendMessage(hWnd, WM_ITEM_TO_CLIPBOARD, 1, (LPARAM)di);
		data_free(wk_di);
		return NULL;
	} else if (to_item == history_treeitem) {
		pdi = &history_data.child;
	} else if (to_item == regist_treeitem) {
		pdi = &regist_data.child;
	} else {
		di = (DATA_INFO *)treeview_get_lparam(hTreeView, to_item);
		pdi = &di->child;
	}
	if (pdi == NULL) {
		data_free(wk_di);
		return NULL;
	}

	if (treeview_get_rootitem(hTreeView, to_item) == history_treeitem) {
		if (wk_di->type == TYPE_FOLDER) {
			data_free(wk_di);
			return NULL;
		}
		// フィルタのチェック
		di = wk_di->child;
		prev_di = NULL;
		while (di != NULL) {
			if (filter_format_check(di->format_name) == FALSE ||
				filter_size_check(di->format_name, di->size) == FALSE) {
				if (prev_di == NULL) {
					wk_di->child = di->next;
					di->next = NULL;
					data_free(di);
					di = wk_di->child;
				} else {
					prev_di->next = di->next;
					di->next = NULL;
					data_free(di);
					di = prev_di->next;
				}
				continue;
			}
			prev_di = di;
			di = di->next;
		}
		if (wk_di->child == NULL) {
			data_free(wk_di);
			return NULL;
		}
		// タイトルの除去
		mem_free(&wk_di->title);
		// 履歴に追加
		if (history_add(pdi, wk_di, TRUE) == FALSE) {
			data_free(wk_di);
			return NULL;
		}
		// 履歴に追加された時に実行するツール
		tool_execute_all(hWnd, CALLTYPE_ADD_HISTORY, wk_di);
		SendMessage(hWnd, WM_HISTORY_CHANGED, 0, 0);
		return TreeView_GetChild(hTreeView, to_item);

	} else {
		if (wk_di->type == TYPE_FOLDER) {
			if (TreeView_GetParent(hTreeView, from_item) == to_item) {
				// フォルダ名の作成
				wsprintf(buf, message_get_res(IDS_VIEWER_COPYNAME), wk_di->title);
				i = 2;
				while (i < 100) {
					for (di = *pdi; di != NULL; di = di->next) {
						if (di->type == TYPE_FOLDER && lstrcmpi(di->title, buf) == 0) {
							break;
						}
					}
					if (di == NULL) {
						break;
					}
					wsprintf(buf, message_get_res(IDS_VIEWER_COPYNAME_NUM), i++, wk_di->title);
				}
				mem_free(&wk_di->title);
				wk_di->title = alloc_copy(buf);
			}
			// フォルダのマージ
			di = regist_merge_item(pdi, wk_di, move_flag, err_str);
			data_free(wk_di);
			if (di == NULL) {
				return NULL;
			}
		} else {
			// ウィンドウ名の除去
			mem_free(&wk_di->window_name);

			if (*pdi == NULL) {
				// 先頭に追加
				*pdi = wk_di;
			} else {
				// 末尾に追加
				for (di = *pdi; di->next != NULL; di = di->next)
					;
				di->next = wk_di;
			}
			di = wk_di;
		}
	}
	return treeview_datainfo_to_treeitem(hTreeView, to_item, di);
}

/*
 * viewer_regist_add - 登録アイテムへ追加
 */
static void viewer_regist_add(const HWND hWnd, const HTREEITEM sel_item, const BOOL regist_move)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM hItem;
	HTREEITEM to_item;
	HTREEITEM ret_item;
	HTREEITEM wk_Item;
	DATA_INFO *di;
	TCHAR *msg;
	TCHAR err_str[BUF_SIZE];
	int i;
	BOOL move = regist_move;

	if (GetFocus() == hListView) {
		if (ListView_GetSelectedCount(hListView) == 0) {
			return;
		}

		// 追加するフォルダの選択
		if (treeview_get_rootitem(hTreeView, TreeView_GetSelection(hTreeView)) == regist_treeitem) {
			if (move == TRUE) {
				msg = message_get_res(IDS_DIALOG_MOVEPOS);
			} else {
				msg = message_get_res(IDS_DIALOG_COPYPOS);
			}
		} else {
			msg = message_get_res(IDS_DIALOG_ADDPOS);
		}
		if ((to_item = select_folder(hInst, hWnd, hTreeView, regist_treeitem, msg)) == NULL) {
			return;
		}
		if (move == TRUE && TreeView_GetSelection(hTreeView) == to_item) {
			return;
		}
		if (treeview_get_rootitem(hTreeView, TreeView_GetSelection(hTreeView)) != regist_treeitem) {
			move = FALSE;
		}

		// フォルダにアイテムを追加
		set_cursor(TRUE);
		ret_item = NULL;
		i = -1;
		while ((i = ListView_GetNextItem(hListView, i, LVNI_SELECTED)) != -1) {
			if ((hItem = (HTREEITEM)listview_get_lparam(hListView, i)) != NULL) {
				*err_str = TEXT('\0');
				wk_Item = viewer_item_copy(hWnd, hItem, to_item, move, err_str);
				if (wk_Item == NULL && *err_str != TEXT('\0')) {
					set_cursor(FALSE);
					MessageBox(hWnd, err_str, ERROR_TITLE, MB_ICONERROR);
					return;
				}
				if (ret_item == NULL) {
					ret_item = wk_Item;
				}
				if (move == TRUE) {
					// 移動
					treeview_delete_item(hTreeView, hItem);
					ListView_DeleteItem(hListView, i);
					i = -1;
				}
			}
		}
		if (ret_item != NULL) {
			TreeView_SelectItem(hTreeView, ret_item);
		}
		viewer_favorites_trim(hWnd);
		// 登録アイテムの保存
		SendMessage(hWnd, WM_REGIST_SAVE, 0, 0);
		SendMessage(hWnd, WM_VIEWER_REFRESH_STATUS, 0, 0);
		set_cursor(FALSE);

	} else {
		SetFocus(hTreeView);

		if ((hItem = sel_item) == NULL) {
			hItem = TreeView_GetSelection(hTreeView);
		}
		if (hItem == history_treeitem || hItem == regist_treeitem) {
			return;
		}

		// クリップボードの内容取得
		viewer_get_clipboard_data(hWnd, hItem);

		// 追加するフォルダの選択
		if (treeview_get_rootitem(hTreeView, hItem) == regist_treeitem) {
			if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) != NULL && di->type == TYPE_DATA) {
				move = FALSE;
			}
			if (move == TRUE) {
				msg = message_get_res(IDS_DIALOG_MOVEPOS);
			} else {
				msg = message_get_res(IDS_DIALOG_COPYPOS);
			}
		} else {
			msg = message_get_res(IDS_DIALOG_ADDPOS);
		}
		if ((to_item = select_folder(hInst, hWnd, hTreeView, regist_treeitem, msg)) == NULL) {
			return;
		}
		if (move == TRUE && TreeView_GetParent(hTreeView, hItem) == to_item) {
			return;
		}

		// フォルダにアイテムを追加
		set_cursor(TRUE);
		*err_str = TEXT('\0');
		if ((ret_item = viewer_item_copy(hWnd, hItem, to_item, move, err_str)) == NULL) {
			set_cursor(FALSE);
			if (*err_str != TEXT('\0')) {
				MessageBox(hWnd, err_str, ERROR_TITLE, MB_ICONERROR);
			}
			return;
		}
		if (move == TRUE && treeview_get_rootitem(hTreeView, hItem) == regist_treeitem) {
			// 移動
			treeview_delete_item(hTreeView, hItem);
		}
		TreeView_SelectItem(hTreeView, ret_item);
		viewer_favorites_trim(hWnd);
		// 登録アイテムの保存
		SendMessage(hWnd, WM_REGIST_SAVE, 0, 0);
		SendMessage(hWnd, WM_VIEWER_REFRESH_STATUS, 0, 0);
		set_cursor(FALSE);
	}
}

static void viewer_favorites_trim(const HWND hWnd)
{
	DATA_INFO **link = &regist_data.child;
	DATA_INFO *item;
	int count = 0;

	while ((item = *link) != NULL) {
		if (item->type == TYPE_ITEM && ++count > option.config_favorite_max) {
			*link = item->next;
			item->next = NULL;
			data_free(item);
		} else {
			link = &item->next;
		}
	}
	treeview_sync_datainfo(GetDlgItem(hWnd, ID_TREE), regist_treeitem, regist_data.child);
}

/*
 * viewer_item_paste - アイテムを貼り付け
 */
static void viewer_item_paste(const HWND hWnd, const HTREEITEM sel_item)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM to_item = sel_item;
	HTREEITEM ret_item;
	DATA_INFO *di;
	TCHAR err_str[BUF_SIZE];
	BOOL free_item = FALSE;

	if (to_item == NULL) {
		to_item = TreeView_GetSelection(hTreeView);
	}
	if (treeview_get_rootitem(hTreeView, to_item) == clip_treeitem) {
		return;
	} else if (to_item != history_treeitem && to_item != regist_treeitem) {
		if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, to_item)) == NULL || di->type != TYPE_FOLDER) {
			return;
		}
	}

	// クリップボードの内容取得
	if (clip_treeitem == NULL) {
		clip_treeitem = treeview_set_item(hTreeView, message_get_res(IDS_TREEITEM_CLIPBOARD),
			(HTREEITEM)TVI_ROOT, (HTREEITEM)TVI_LAST, 0, 0, (LPARAM)&clip_di);
		free_item = TRUE;
	}
	treeview_delete_child(hTreeView, clip_treeitem);
	data_free(clip_di.child);
	clip_di.child = clipboard_to_datainfo(hWnd);
	treeview_datainfo_to_treeitem(hTreeView, clip_treeitem, clip_di.child);
	viewer_get_clipboard_data(hWnd, clip_treeitem);

	// フォルダにアイテムを追加
	set_cursor(TRUE);
	*err_str = TEXT('\0');
	if ((ret_item = viewer_item_copy(hWnd, clip_treeitem, to_item, FALSE, err_str)) == NULL) {
		if (free_item == TRUE) {
			TreeView_DeleteItem(hTreeView, clip_treeitem);
			clip_treeitem = NULL;
		}
		set_cursor(FALSE);
		if (*err_str != TEXT('\0')) {
			MessageBox(hWnd, err_str, ERROR_TITLE, MB_ICONERROR);
		}
		return;
	}

	if (GetFocus() == hListView) {
		// ツリービューとリストビューの同期
		treeview_to_listview(hTreeView, TreeView_GetSelection(hTreeView), hListView);
		listview_lparam_select(hListView, (LPARAM)ret_item);
	} else {
		TreeView_SelectItem(hTreeView, ret_item);
	}
	if (treeview_get_rootitem(hTreeView, to_item) == regist_treeitem) {
		// 登録アイテムの保存
		SendMessage(hWnd, WM_REGIST_SAVE, 0, 0);
	} else if (option.history_save == 1 && option.history_always_save == 1 &&
		treeview_get_rootitem(hTreeView, to_item) == history_treeitem) {
		// 履歴の保存
		SendMessage(hWnd, WM_HISTORY_SAVE, 0, 0);
	}
	SendMessage(hWnd, WM_VIEWER_REFRESH_STATUS, 0, 0);
	if (free_item == TRUE) {
		TreeView_DeleteItem(hTreeView, clip_treeitem);
		clip_treeitem = NULL;
	}
	set_cursor(FALSE);
}

/*
 * viewer_move_up - アイテムを上に移動
 */
static BOOL viewer_move_up(const HWND hWnd, const HTREEITEM sel_item)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM hItem;
	int i;

	if (GetFocus() == hListView) {
		if (ListView_GetSelectedCount(hListView) == 0) {
			return FALSE;
		}

		SendMessage(hTreeView, WM_SETREDRAW, (WPARAM)FALSE, 0);
		SendMessage(hListView, WM_SETREDRAW, (WPARAM)FALSE, 0);

		i = -1;
		while ((i = ListView_GetNextItem(hListView, i, LVNI_SELECTED)) != -1) {
			if ((hItem = (HTREEITEM)listview_get_lparam(hListView, i)) == NULL) {
				continue;
			}
			// アイテムの入れ替え
			if ((hItem = treeview_move_up(hTreeView, hItem)) == NULL) {
				break;
			}
			listview_set_lparam(hListView, i, (LPARAM)hItem);
			listview_move_item(hListView, i, -1);
		}
		SendMessage(hTreeView, WM_SETREDRAW, (WPARAM)TRUE, 0);
		UpdateWindow(hTreeView);
		SendMessage(hListView, WM_SETREDRAW, (WPARAM)TRUE, 0);
		UpdateWindow(hListView);

	} else {
		SetFocus(hTreeView);

		if ((hItem = sel_item) == NULL) {
			hItem = TreeView_GetSelection(hTreeView);
		}
		// アイテムの入れ替え
		if (treeview_move_up(hTreeView, hItem) == NULL) {
			SendMessage(hTreeView, WM_SETREDRAW, (WPARAM)TRUE, 0);
			UpdateWindow(hTreeView);
			return FALSE;
		}
		if (current_wnd == hListView) {
			// リストビューの更新
			treeview_to_listview(hTreeView, TreeView_GetSelection(hTreeView), hListView);
		}
	}
	return TRUE;
}

/*
 * viewer_move_down - アイテムを下に移動
 */
static BOOL viewer_move_down(const HWND hWnd, const HTREEITEM sel_item)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM hItem;
	int i;

	if (GetFocus() == hListView) {
		if (ListView_GetSelectedCount(hListView) == 0) {
			return FALSE;
		}

		SendMessage(hTreeView, WM_SETREDRAW, (WPARAM)FALSE, 0);
		SendMessage(hListView, WM_SETREDRAW, (WPARAM)FALSE, 0);

		for (i = ListView_GetItemCount(hListView) - 1; i >= 0; i--) {
			if (ListView_GetItemState(hListView, i, LVNI_SELECTED) == LVNI_SELECTED) {
				if ((hItem = (HTREEITEM)listview_get_lparam(hListView, i)) == NULL) {
					continue;
				}
				// アイテムの入れ替え
				if ((hItem = treeview_move_down(hTreeView, hItem)) == NULL) {
					break;
				}
				listview_set_lparam(hListView, i, (LPARAM)hItem);
				listview_move_item(hListView, i, 1);
			}
		}
		SendMessage(hTreeView, WM_SETREDRAW, (WPARAM)TRUE, 0);
		UpdateWindow(hTreeView);
		SendMessage(hListView, WM_SETREDRAW, (WPARAM)TRUE, 0);
		UpdateWindow(hListView);

	} else {
		SetFocus(hTreeView);

		if ((hItem = sel_item) == NULL) {
			hItem = TreeView_GetSelection(hTreeView);
		}
		// アイテムの入れ替え
		if (treeview_move_down(hTreeView, hItem) == NULL) {
			SendMessage(hTreeView, WM_SETREDRAW, (WPARAM)TRUE, 0);
			UpdateWindow(hTreeView);
			return FALSE;
		}
		if (current_wnd == hListView) {
			// リストビューの更新
			treeview_to_listview(hTreeView, TreeView_GetSelection(hTreeView), hListView);
		}
	}
	return TRUE;
}

/*
 * viewer_create_item - アイテムの作成
 */
static void viewer_create_item(const HWND hWnd, const HTREEITEM sel_item)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM hItem = sel_item;
	HTREEITEM cItem;
	HANDLE data = NULL;
	DATA_INFO *di;
	DATA_INFO *new_di;
	DATA_INFO *wk_di;
	TCHAR format_name[BUF_SIZE];
	TCHAR file_name[MAX_PATH];
	TCHAR err_str[BUF_SIZE];
	DWORD size = 0;

	if (GetFocus() == hListView) {
		// リストビューの選択アイテム取得
		if (ListView_GetNextItem(hListView, -1, LVNI_FOCUSED | LVNI_SELECTED) != -1) {
			hItem = (HTREEITEM)listview_get_lparam(hListView,
				ListView_GetNextItem(hListView, -1, LVNI_FOCUSED | LVNI_SELECTED));
		}
	}
	if (hItem == NULL) {
		hItem = TreeView_GetSelection(hTreeView);
	}
	if (treeview_get_rootitem(hTreeView, hItem) == clip_treeitem) {
		return;
	}

	// 形式とファイル名の選択
	if (select_format(hInst, hWnd, format_name, file_name) == FALSE || *format_name == TEXT('\0')) {
		return;
	}
	if (*file_name != TEXT('\0')) {
		// ファイルからデータを作成
		set_cursor(TRUE);
		*err_str = TEXT('\0');
		if ((data = format_file_to_data(file_name, format_name, &size, err_str)) == NULL) {
			if (*err_str != TEXT('\0')) {
				set_cursor(FALSE);
				MessageBox(hWnd, err_str, ERROR_TITLE, MB_ICONERROR);
				return;
			}
			if ((data = clipboard_file_to_data(file_name, format_name, &size, err_str)) == NULL && *err_str != TEXT('\0')) {
				set_cursor(FALSE);
				MessageBox(hWnd, err_str, ERROR_TITLE, MB_ICONERROR);
				return;
			}
		}
		set_cursor(FALSE);
	}

	if (hItem == history_treeitem) {
		di = &history_data;
	} else if (hItem == regist_treeitem) {
		di = &regist_data;
	} else {
		if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) != NULL && di->type == TYPE_DATA) {
			hItem = TreeView_GetParent(hTreeView, hItem);
			di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem);
		}
	}
	if (di == NULL) {
		if (data != NULL && format_free_data(format_name, data) == FALSE) {
			clipboard_free_data(format_name, data);
		}
		return;
	}

	if (hItem == history_treeitem ||
		(di->type == TYPE_FOLDER && treeview_get_rootitem(hTreeView, hItem) == history_treeitem)) {
		// 履歴にアイテムを追加
		if ((new_di = data_create_item(NULL, TRUE, err_str)) == NULL) {
			if (*err_str != TEXT('\0')) {
				MessageBox(hWnd, err_str, ERROR_TITLE, MB_ICONERROR);
			}
			if (data != NULL && format_free_data(format_name, data) == FALSE) {
				clipboard_free_data(format_name, data);
			}
			return;
		}
		if ((new_di->child = data_create_data(0, format_name, data, size, TRUE, err_str)) == NULL) {
			if (*err_str != TEXT('\0')) {
				MessageBox(hWnd, err_str, ERROR_TITLE, MB_ICONERROR);
			}
			if (data != NULL && format_free_data(format_name, data) == FALSE) {
				clipboard_free_data(format_name, data);
			}
			data_free(new_di);
			return;
		}

		// 履歴に追加
		if (history_add(&di->child, new_di, TRUE) == FALSE) {
			data_free(new_di);
			return;
		}
		// 履歴に追加された時に実行するツール
		tool_execute_all(hWnd, CALLTYPE_ADD_HISTORY, new_di);

		if (option.history_save == 1 && option.history_always_save == 1) {
			// 履歴の保存
			SendMessage(hWnd, WM_HISTORY_SAVE, 0, 0);
		}
		SendMessage(hWnd, WM_HISTORY_CHANGED, 0, 0);
		if ((cItem = TreeView_GetChild(hTreeView, hItem)) != NULL) {
			TreeView_SelectItem(hTreeView, cItem);
			SetFocus(GetDlgItem(hWnd, ID_CONTAINER));
		}
		return;
	}

	switch (di->type) {
	case TYPE_ROOT:
	case TYPE_FOLDER:
		// アイテムの作成
		if ((new_di = data_create_item(NULL, TRUE, err_str)) == NULL) {
			if (*err_str != TEXT('\0')) {
				MessageBox(hWnd, err_str, ERROR_TITLE, MB_ICONERROR);
			}
			if (data != NULL && format_free_data(format_name, data) == FALSE) {
				clipboard_free_data(format_name, data);
			}
			return;
		}
		if ((new_di->child = data_create_data(0, format_name, data, size, TRUE, err_str)) == NULL) {
			if (*err_str != TEXT('\0')) {
				MessageBox(hWnd, err_str, ERROR_TITLE, MB_ICONERROR);
			}
			if (data != NULL && format_free_data(format_name, data) == FALSE) {
				clipboard_free_data(format_name, data);
			}
			data_free(new_di);
			return;
		}
		break;

	case TYPE_ITEM:
		// アイテムに形式を追加
		// 同名の形式が存在するかチェック
		for (wk_di = di->child; wk_di != NULL; wk_di = wk_di->next) {
			if (lstrcmpi(format_name, wk_di->format_name) == 0) {
				if (MessageBox(hWnd, message_get_res(IDS_QUESTION_REPLACE), WINDOW_TITLE, MB_ICONQUESTION | MB_YESNO) == IDNO ||
					SendMessage(hWnd, WM_ITEM_CHECK, 0, (LPARAM)wk_di) == -1) {
					if (data != NULL && format_free_data(format_name, data) == FALSE) {
						clipboard_free_data(format_name, data);
					}
					return;
				}
				// ツリービューからアイテムを削除
				cItem = TreeView_GetChild(hTreeView, hItem);
				while (cItem != NULL) {
					if ((DATA_INFO *)treeview_get_lparam(hTreeView, cItem) == wk_di) {
						TreeView_DeleteItem(hTreeView, cItem);
						break;
					}
					cItem = TreeView_GetNextSibling(hTreeView, cItem);
				}
				data_delete(&di->child, wk_di, TRUE);
				break;
			}
		}
		// 形式の作成
		if ((new_di = data_create_data(0, format_name, data, size, TRUE, err_str)) == NULL) {
			if (*err_str != TEXT('\0')) {
				MessageBox(hWnd, err_str, ERROR_TITLE, MB_ICONERROR);
			}
			if (data != NULL && format_free_data(format_name, data) == FALSE) {
				clipboard_free_data(format_name, data);
			}
			return;
		}
		break;

	default:
		if (data != NULL && format_free_data(format_name, data) == FALSE) {
			clipboard_free_data(format_name, data);
		}
		return;
	}
	// アイテムの追加
	if (di->child == NULL) {
		di->child = new_di;
	} else {
		for (di = di->child; di->next != NULL; di = di->next)
			;
		di->next = new_di;
	}

	// ツリービューにアイテムを追加
	cItem = (new_di->type != TYPE_DATA || option.tree_show_format == 1) ?
		treeview_datainfo_to_treeitem(hTreeView, hItem, new_di) : NULL;
	TreeView_SelectItem(hTreeView, (cItem != NULL) ? cItem : hItem);
	SetFocus(GetDlgItem(hWnd, ID_CONTAINER));

	if (new_di->type == TYPE_DATA) {
		// タイトルの更新
		treeview_title_refresh(hTreeView, hItem);
		viewer_set_datetime(hWnd, hItem);
	}

	set_cursor(TRUE);
	if (treeview_get_rootitem(hTreeView, cItem) == regist_treeitem) {
		// 登録アイテムの保存
		SendMessage(hWnd, WM_REGIST_SAVE, 0, 0);
	} else if (option.history_save == 1 && option.history_always_save == 1 &&
		treeview_get_rootitem(hTreeView, cItem) == history_treeitem) {
		// 履歴の保存
		SendMessage(hWnd, WM_HISTORY_SAVE, 0, 0);
	}
	set_cursor(FALSE);
}

/*
 * viewer_create_folder - フォルダの作成
 */
static void viewer_create_folder(const HWND hWnd, const HTREEITEM sel_item)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM hItem = sel_item;
	HTREEITEM new_hItem;
	DATA_INFO *di;
	DATA_INFO **pdi;
	TCHAR title[BUF_SIZE];
	TCHAR err_str[BUF_SIZE];
	int i;

	if (hItem == NULL) {
		hItem = TreeView_GetSelection(hTreeView);
	}
	if (treeview_get_rootitem(hTreeView, hItem) != regist_treeitem) {
		return;
	}

	pdi = NULL;
	if (hItem == regist_treeitem) {
		pdi = &regist_data.child;
	} else {
		di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem);
		while (di != NULL && di->type != TYPE_FOLDER) {
			hItem = TreeView_GetParent(hTreeView, hItem);
			di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem);
		}
		if (di == NULL) {
			pdi = &regist_data.child;
		} else {
			pdi = &di->child;
		}
	}
	if (pdi == NULL) {
		return;
	}

	// フォルダパスの作成
	lstrcpy(title, message_get_res(IDS_TREEITEM_NEWFOLDER));
	i = 1;
	*err_str = TEXT('\0');
	while ((di = regist_create_folder(pdi, title, err_str)) == NULL) {
		if (*err_str != TEXT('\0')) {
			MessageBox(hWnd, err_str, ERROR_TITLE, MB_ICONERROR);
			return;
		}
		wsprintf(title, TEXT("%s (%d)"), message_get_res(IDS_TREEITEM_NEWFOLDER), ++i);
	}

	// ツリービューにアイテムを追加
	new_hItem = treeview_set_item(hTreeView, di->title, hItem, (HTREEITEM)TVI_LAST,
		ICON_FOLDER, ICON_FOLDER_OPEN, (LPARAM)di);
	if (new_hItem == NULL) {
		data_delete(pdi, di, TRUE);
		return;
	}

	if (current_wnd == hListView) {
		// ツリービューとリストビューの同期
		treeview_to_listview(hTreeView, TreeView_GetSelection(hTreeView), hListView);
	}
	// 登録アイテムの保存
	set_cursor(TRUE);
	SendMessage(hWnd, WM_REGIST_SAVE, 0, 0);
	set_cursor(FALSE);

	if (GetFocus() == hListView) {
		SetFocus(hTreeView);
	}
	// ラベルの編集
	TreeView_Expand(hTreeView, hItem, TVE_EXPAND);
	TreeView_EditLabel(hTreeView, new_hItem);
}

/*
 * viewer_data_save - データの保存
 */
static void viewer_data_save(const HWND hWnd, const HTREEITEM sel_item)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM hItem = sel_item;
	OPENFILENAME of;
	DATA_INFO *di;
	TCHAR file_name[MAX_PATH];
	TCHAR err_str[BUF_SIZE];

	if (GetFocus() == hListView) {
		// リストビューの選択アイテム取得
		if (ListView_GetNextItem(hListView, -1, LVNI_FOCUSED | LVNI_SELECTED) != -1) {
			hItem = (HTREEITEM)listview_get_lparam(hListView,
				ListView_GetNextItem(hListView, -1, LVNI_FOCUSED | LVNI_SELECTED));
		}
	}
	if (hItem == NULL) {
		hItem = TreeView_GetSelection(hTreeView);
	}

	if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) == NULL) {
		return;
	}
	switch (di->type) {
	case TYPE_FOLDER:
		return;

	case TYPE_ITEM:
		if ((di = format_get_priority_highest(di)) == NULL) {
			return;
		}
		break;
	}

	// ファイルの選択
	ZeroMemory(&of, sizeof(OPENFILENAME));
	of.lStructSize = sizeof(OPENFILENAME);
	of.hInstance = hInst;
	of.hwndOwner = hWnd;
	of.lpstrFilter = TEXT("*.*\0*.*\0\0");
	of.nFilterIndex = 1;
	*file_name = TEXT('\0');
	of.lpstrFile = file_name;
	of.nMaxFile = MAX_PATH - 1;
	of.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	if (format_get_file_info(di->format_name, di, &of, FALSE) >= 0 && GetSaveFileName(&of) == FALSE) {
		return;
	}

	set_cursor(TRUE);
	// クリップボードの内容取得
	viewer_get_clipboard_data(hWnd, hItem);
	// ファイルに保存
	*err_str = TEXT('\0');
	if (format_data_to_file(di, file_name, of.nFilterIndex, err_str) == FALSE) {
		if (*err_str != TEXT('\0')) {
			set_cursor(FALSE);
			MessageBox(hWnd, err_str, ERROR_TITLE, MB_ICONERROR);
			return;
		}
		if (clipboard_data_to_file(di, file_name, of.nFilterIndex, err_str) == FALSE && *err_str != TEXT('\0')) {
			set_cursor(FALSE);
			MessageBox(hWnd, err_str, ERROR_TITLE, MB_ICONERROR);
			return;
		}
	}
	set_cursor(FALSE);
}

/*
 * viewer_import_item - アイテムのインポート
 */
static BOOL viewer_import_item(const HWND hWnd)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	OPENFILENAME of;
	HTREEITEM to_item;
	DATA_INFO *load_di = NULL;
	DATA_INFO *to_di;
	DATA_INFO *di;
	TCHAR file_name[MAX_PATH];
	TCHAR err_str[BUF_SIZE];

	// インポートするファイルの選択
	ZeroMemory(&of, sizeof(OPENFILENAME));
	of.lStructSize = sizeof(OPENFILENAME);
	of.hInstance = hInst;
	of.hwndOwner = hWnd;
	of.lpstrFilter = TEXT("*.dat\0*.dat\0*.*\0*.*\0\0");
	of.nFilterIndex = 1;
	*file_name = TEXT('\0');
	of.lpstrFile = file_name;
	of.nMaxFile = MAX_PATH - 1;
	of.lpstrDefExt = TEXT("dat");
	of.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	if (GetOpenFileName(&of) == FALSE) {
		return FALSE;
	}

	// インポートしたアイテムを追加するフォルダの選択
	to_item = select_folder(hInst, hWnd, hTreeView, regist_treeitem, message_get_res(IDS_DIALOG_IMPORT));
	if (to_item == NULL) {
		return FALSE;
	}
	if (to_item == regist_treeitem) {
		to_di = &regist_data;
	} else {
		to_di = (DATA_INFO *)treeview_get_lparam(hTreeView, to_item);
	}
	if (to_di == NULL) {
		return FALSE;
	}

	set_cursor(TRUE);
	// ファイルからアイテムを作成
	*err_str = TEXT('\0');
	if (file_read_data(file_name, &load_di, err_str) == FALSE && *err_str != TEXT('\0')) {
		set_cursor(FALSE);
		MessageBox(hWnd, err_str, file_name, MB_ICONERROR);
		return FALSE;
	}

	// アイテムのコピー
	for (di = load_di; di != NULL; di = di->next) {
		if (regist_merge_item(&to_di->child, di, FALSE, err_str) == NULL) {
			break;
		}
	}
	data_free(load_di);

	treeview_sync_datainfo(hTreeView, regist_treeitem, regist_data.child);
	TreeView_Expand(hTreeView, to_item, TVE_EXPAND);
	// 登録アイテムの保存
	SendMessage(hWnd, WM_REGIST_SAVE, 0, 0);
	if (current_wnd == hListView) {
		// リストビューの更新
		treeview_to_listview(hTreeView, TreeView_GetSelection(hTreeView), hListView);
	}
	SendMessage(hWnd, WM_VIEWER_REFRESH_STATUS, 0, 0);
	set_cursor(FALSE);
	return TRUE;
}

/*
 * viewer_export_item - アイテムのエクスポート
 */
static BOOL viewer_export_item(const HWND hWnd)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	OPENFILENAME of;
	HTREEITEM from_item;
	DATA_INFO *from_di;
	TCHAR file_name[MAX_PATH];
	TCHAR err_str[BUF_SIZE];

	// エクスポートするフォルダの選択
	from_item = select_folder(hInst, hWnd, hTreeView, regist_treeitem, message_get_res(IDS_DIALOG_EXPORT));
	if (from_item == regist_treeitem) {
		from_di = &regist_data;
	} else {
		from_di = (DATA_INFO *)treeview_get_lparam(hTreeView, from_item);
	}
	if (from_di == NULL) {
		return FALSE;
	}

	// インポートするファイルの選択
	ZeroMemory(&of, sizeof(OPENFILENAME));
	of.lStructSize = sizeof(OPENFILENAME);
	of.hInstance = hInst;
	of.hwndOwner = hWnd;
	of.lpstrFilter = TEXT("*.dat\0*.dat\0*.*\0*.*\0\0");
	of.nFilterIndex = 1;
	*file_name = TEXT('\0');
	of.lpstrFile = file_name;
	of.nMaxFile = MAX_PATH - 1;
	of.lpstrDefExt = TEXT("dat");
	of.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	if (GetSaveFileName(&of) == FALSE) {
		return FALSE;
	}

	// ファイルへ出力
	*err_str = TEXT('\0');
	if (file_write_data(file_name, from_di->child, err_str) == FALSE) {
		if (*err_str != TEXT('\0')) {
			MessageBox(hWnd, err_str, file_name, MB_ICONERROR);
		}
		return FALSE;
	}
	return TRUE;
}

/*
 * viewer_rename - アイテムのタイトル変更
 */
static BOOL viewer_rename(const HWND hWnd, const HTREEITEM sel_item, TCHAR *title)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM hItem = sel_item;
	HTREEITEM pItem;
	DATA_INFO *di;
	DATA_INFO *wk_di;
	TCHAR *tmp;
	TCHAR err_str[BUF_SIZE];

	if (title == NULL || *title == TEXT('\0')) {
		return FALSE;
	}

	if (sel_item == NULL && current_wnd == hListView) {
		if (ListView_GetNextItem(hListView, -1, LVNI_FOCUSED) == -1) {
			return FALSE;
		}
		hItem = (HTREEITEM)listview_get_lparam(hListView,
			ListView_GetNextItem(hListView, -1, LVNI_FOCUSED));
		if (hItem == NULL) {
			return FALSE;
		}
	}

	// データの取得
	if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) == NULL) {
		return FALSE;
	}

	switch (di->type) {
	case TYPE_DATA:
		return FALSE;

	case TYPE_FOLDER:
		// ファイル名のチェック
		if (file_name_check(title) == FALSE) {
			MessageBox(hWnd, message_get_res(IDS_ERROR_FILENAME), ERROR_TITLE, MB_ICONERROR);
			return FALSE;
		}

		// 同名のフォルダが存在しないかチェック
		if ((pItem = TreeView_GetParent(hTreeView, hItem)) == history_treeitem) {
			wk_di = history_data.child;
		} else if (pItem == regist_treeitem) {
			wk_di = regist_data.child;
		} else {
			if ((wk_di = (DATA_INFO *)treeview_get_lparam(hTreeView, pItem)) == NULL) {
				return FALSE;
			}
			wk_di = wk_di->child;
		}
		for (; wk_di != NULL; wk_di = wk_di->next) {
			if (wk_di->type == TYPE_FOLDER && wk_di != di && lstrcmpi(wk_di->title, title) == 0) {
				MessageBox(hWnd, message_get_res(IDS_ERROR_FOLDERNAME), ERROR_TITLE, MB_ICONERROR);
				return FALSE;
			}
		}
	case TYPE_ITEM:
		// 新しいタイトルの設定
		if ((tmp = alloc_copy(title)) == NULL) {
			message_get_error(GetLastError(), err_str);
			if (*err_str != TEXT('\0')) {
				MessageBox(hWnd, err_str, ERROR_TITLE, MB_ICONERROR);
			}
			return FALSE;
		}
		mem_free(&di->title);
		di->title = tmp;
		break;
	}

	if (current_wnd == hListView) {
		if (sel_item != NULL) {
			// リストビューの更新
			SetTimer(hWnd, TIMER_LV_REFRESH, 1, NULL);
		} else {
			// ツリービューの更新
			treeview_set_text(hTreeView, hItem, title);
		}
	}
	if (treeview_get_rootitem(hTreeView, hItem) == regist_treeitem) {
		// 登録アイテムの保存
		set_cursor(TRUE);
		SendMessage(hWnd, WM_REGIST_SAVE, 0, 0);
		set_cursor(FALSE);
	}
	return TRUE;
}

/*
 * viewer_clear_name - アイテムのタイトルクリア
 */
static BOOL viewer_clear_name(const HWND hWnd, const HTREEITEM sel_item)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM hItem;
	DATA_INFO *di;
	int i;

	if (GetFocus() == hTreeView) {
		if ((hItem = sel_item) == NULL) {
			hItem = TreeView_GetSelection(hTreeView);
		}
		if (treeview_get_rootitem(hTreeView, hItem) == clip_treeitem) {
			return FALSE;
		}
		if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) == NULL || di->type != TYPE_ITEM) {
			return FALSE;
		}
		mem_free(&di->title);
		treeview_title_refresh(hTreeView, hItem);

		if (current_wnd == hListView) {
			// リストビューの更新
			treeview_to_listview(hTreeView, TreeView_GetSelection(hTreeView), hListView);
		}
	} else if (GetFocus() == hListView) {
		if (ListView_GetSelectedCount(hListView) == 0) {
			return FALSE;
		}

		set_cursor(TRUE);
		i = -1;
		while ((i = ListView_GetNextItem(hListView, i, LVNI_SELECTED)) != -1) {
			if ((hItem = (HTREEITEM)listview_get_lparam(hListView, i)) == NULL) {
				continue;
			}
			if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) == NULL || di->type != TYPE_ITEM) {
				continue;
			}
			mem_free(&di->title);
			treeview_title_refresh(hTreeView, hItem);
		}
		// リストビューの更新
		treeview_to_listview(hTreeView, TreeView_GetSelection(hTreeView), hListView);
		set_cursor(FALSE);

	} else {
		return FALSE;
	}
	return TRUE;
}

/*
 * viewer_set_hotkey - ホットキーの設定
 */
static BOOL viewer_set_hotkey(const HWND hWnd, const HTREEITEM sel_item)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM hItem = sel_item;
	DATA_INFO *di;
	BOOL ret;

	if (GetFocus() == hListView) {
		// リストビューの選択アイテム取得
		if (ListView_GetNextItem(hListView, -1, LVNI_FOCUSED | LVNI_SELECTED) != -1) {
			hItem = (HTREEITEM)listview_get_lparam(hListView,
				ListView_GetNextItem(hListView, -1, LVNI_FOCUSED | LVNI_SELECTED));
		}
	}
	if (hItem == NULL) {
		hItem = TreeView_GetSelection(hTreeView);
	}
	if (treeview_get_rootitem(hTreeView, hItem) != regist_treeitem) {
		return FALSE;
	}
	if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) == NULL || di->type != TYPE_ITEM) {
		return FALSE;
	}
	// ホットキー設定
	ret = set_hotkey(hInst, hWnd, di);
	if (current_wnd == hListView) {
		// リストビューの更新
		treeview_to_listview(hTreeView, TreeView_GetSelection(hTreeView), hListView);
	}
	if (ret == TRUE) {
		// 登録アイテムの保存
		set_cursor(TRUE);
		SendMessage(hWnd, WM_REGIST_SAVE, 0, 0);
		set_cursor(FALSE);
	}
	return ret;
}

/*
 * viewer_delete_item - アイテムの削除
 */
static void viewer_delete_item(const HWND hWnd, const HTREEITEM sel_item)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HTREEITEM hItem;
	DATA_INFO *di;
	int i;

	if (GetFocus() == hListView) {
		if (ListView_GetSelectedCount(hListView) == 0) {
			return;
		}
		// 確認メッセージ
		if (option.viewer_delete_confirm == 1 &&
			MessageBox(hWnd, message_get_res(IDS_QUESTION_DELETE), WINDOW_TITLE, MB_ICONQUESTION | MB_YESNO) == IDNO) {
			return;
		}
		set_cursor(TRUE);
		SendMessage(hTreeView, WM_SETREDRAW, (WPARAM)FALSE, 0);
		SendMessage(hListView, WM_SETREDRAW, (WPARAM)FALSE, 0);
		while ((i = ListView_GetNextItem(hListView, -1, LVNI_SELECTED)) != -1) {
			if ((hItem = (HTREEITEM)listview_get_lparam(hListView, i)) != NULL) {
				// ホットキーの解除
				if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) != NULL) {
					regist_unregist_hotkey(main_wnd, di->child);
					if (di->hkey_id != 0) {
						UnregisterHotKey(main_wnd, di->hkey_id);
						di->hkey_id = 0;
					}
				}
				// ツリービューからアイテムを削除
				treeview_delete_item(hTreeView, hItem);
			}
			// リストビューからアイテムを削除
			ListView_DeleteItem(hListView, i);
		}
		SendMessage(hTreeView, WM_SETREDRAW, (WPARAM)TRUE, 0);
		SendMessage(hListView, WM_SETREDRAW, (WPARAM)TRUE, 0);
		UpdateWindow(hTreeView);
		UpdateWindow(hListView);
		set_cursor(FALSE);
	} else {
		SetFocus(hTreeView);

		if ((hItem = sel_item) == NULL) {
			hItem = TreeView_GetSelection(hTreeView);
		}
		if (treeview_get_rootitem(hTreeView, hItem) == clip_treeitem) {
			return;
		}
		if (treeview_get_lparam(hTreeView, hItem) == 0) {
			return;
		}
		// 確認メッセージ
		if (option.viewer_delete_confirm == 1 &&
			MessageBox(hWnd, message_get_res(IDS_QUESTION_DELETE), WINDOW_TITLE, MB_ICONQUESTION | MB_YESNO) == IDNO) {
			return;
		}
		set_cursor(TRUE);
		// ホットキーの解除
		if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) != NULL) {
			regist_unregist_hotkey(main_wnd, di->child);
			if (di->hkey_id != 0) {
				UnregisterHotKey(main_wnd, di->hkey_id);
				di->hkey_id = 0;
			}
		}
		// ツリービューからアイテムを削除
		treeview_delete_item(hTreeView, hItem);
		if (current_wnd == hListView) {
			// ツリービューとリストビューの同期
			treeview_to_listview(hTreeView, TreeView_GetSelection(hTreeView), hListView);
		}
		set_cursor(FALSE);
	}
}

/*
 * viewer_format_to_option - 形式名をオプションに送る
 */
static BOOL viewer_format_to_option(const HWND hWnd, const HTREEITEM sel_item, const TCHAR *mode)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HTREEITEM hItem = sel_item;
	DATA_INFO *di;
	TCHAR buf[BUF_SIZE];

	if (hItem == NULL) {
		hItem = TreeView_GetSelection(hTreeView);
	}
	if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) == NULL || di->type != TYPE_DATA) {
		return FALSE;
	}
	wsprintf(buf, TEXT("%s%s"), mode, di->format_name);

	SendMessage(hWnd, WM_OPTION_SHOW, 0, (LPARAM)buf);
	return TRUE;
}

/*
 * viewer_tool_execute - ツールの実行
 */
static void viewer_tool_execute(const HWND hWnd, const HTREEITEM sel_item, const int index)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	HWND focus_wnd;
	HTREEITEM hItem;
	DATA_INFO *di;
	TOOL_DATA_INFO *root_tdi = NULL;
	TOOL_DATA_INFO *new_tdi;
	TOOL_DATA_INFO *tdi;
	int ret;
	int i;

	if (GetFocus() == hListView && ListView_GetSelectedCount(hListView) > 0) {
		// ツール用データの作成
		i = -1;
		while ((i = ListView_GetNextItem(hListView, i, LVNI_SELECTED)) != -1) {
			if ((hItem = (HTREEITEM)listview_get_lparam(hListView, i)) == NULL) {
				continue;
			}
			if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) == NULL) {
				continue;
			}
			new_tdi = tool_data_copy(di, FALSE);
			if (root_tdi == NULL) {
				root_tdi = new_tdi;
			} else {
				tdi->next = new_tdi;
			}
			tdi = new_tdi;
		}
		if (root_tdi != NULL) {
			// ツールの呼び出し
			ret = tool_execute(hWnd, option.tool_info + index, CALLTYPE_VIEWER, NULL, root_tdi);
			tool_data_free(root_tdi);
			if (ret & TOOL_DATA_MODIFIED) {
				set_cursor(TRUE);
				// アイテムの日時更新
				i = -1;
				while ((i = ListView_GetNextItem(hListView, i, LVNI_SELECTED)) != -1) {
					if ((hItem = (HTREEITEM)listview_get_lparam(hListView, i)) != NULL) {
						viewer_set_datetime(hWnd, hItem);
					}
				}
				// ツリーの同期
				if (treeview_get_rootitem(hTreeView, TreeView_GetSelection(hTreeView)) == history_treeitem) {
					data_adjust(&history_data.child);
					treeview_sync_datainfo(hTreeView, history_treeitem, history_data.child);
					if (option.history_save == 1 && option.history_always_save == 1) {
						// 履歴の保存
						SendMessage(hWnd, WM_HISTORY_SAVE, 0, 0);
					}
				} else {
					data_adjust(&regist_data.child);
					treeview_sync_datainfo(hTreeView, regist_treeitem, regist_data.child);
					// 登録アイテムの保存
					SendMessage(hWnd, WM_REGIST_SAVE, 0, 0);
				}
				// リストビューの更新
				treeview_to_listview(hTreeView, TreeView_GetSelection(hTreeView), hListView);
				SendMessage(hWnd, WM_VIEWER_REFRESH_STATUS, 0, 0);
				set_cursor(FALSE);
			}
		}
	} else {
		focus_wnd = (GetFocus() == hTreeView) ? hTreeView : GetDlgItem(hWnd, ID_CONTAINER);
		SetFocus(hTreeView);

		if ((hItem = sel_item) == NULL) {
			hItem = TreeView_GetSelection(hTreeView);
		}
		if (treeview_get_rootitem(hTreeView, hItem) == clip_treeitem) {
			di = NULL;
		} else if (hItem == history_treeitem) {
			di = &history_data;
		} else if (hItem == regist_treeitem) {
			di = &regist_data;
		} else {
			di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem);
		}

		// 自動保存解除
		viewer_save_data(hWnd, TreeView_GetSelection(hTreeView));
		save_flag = FALSE;

		// ツールの呼び出し
		ret = tool_execute(hWnd, option.tool_info + index, CALLTYPE_VIEWER, di, NULL);

		// 自動保存設定
		if (ret & TOOL_DATA_MODIFIED) {
			viewer_sel_cheange(hWnd, TreeView_GetSelection(hTreeView), TreeView_GetSelection(hTreeView));
		}
		save_flag = TRUE;

		set_cursor(TRUE);
		if ((ret & TOOL_DATA_MODIFIED) && treeview_get_rootitem(hTreeView, hItem) != clip_treeitem) {
			viewer_set_datetime(hWnd, hItem);
			// ツリーの同期
			if (treeview_get_rootitem(hTreeView, hItem) == history_treeitem) {
				data_adjust(&history_data.child);
				treeview_sync_datainfo(hTreeView, history_treeitem, history_data.child);
				if (option.history_save == 1 && option.history_always_save == 1) {
					// 履歴の保存
					SendMessage(hWnd, WM_HISTORY_SAVE, 0, 0);
				}
			} else {
				data_adjust(&regist_data.child);
				treeview_sync_datainfo(hTreeView, regist_treeitem, regist_data.child);
				// 登録アイテムの保存
				SendMessage(hWnd, WM_REGIST_SAVE, 0, 0);
			}
			if (current_wnd == hListView) {
				// リストビューの更新
				treeview_to_listview(hTreeView, TreeView_GetSelection(hTreeView), hListView);
			}
		}
		SendMessage(hWnd, WM_VIEWER_REFRESH_STATUS, 0, 0);
		set_cursor(FALSE);
		SetFocus(focus_wnd);
	}
}

/*
 * viewer_set_datetime - アイテムの更新日時を設定
 */
static void viewer_set_datetime(const HWND hWnd, const HTREEITEM hItem)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HTREEITEM parent_item;
	DATA_INFO *di;

	if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) == NULL) {
		return;
	}
	if (di->type == TYPE_DATA) {
		if ((parent_item = TreeView_GetParent(hTreeView, hItem)) == NULL) {
			return;
		}
		if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, parent_item)) == NULL) {
			return;
		}
	}
	data_set_modified(di);
}

/*
 * clipboard_to_datainfo - クリップボードの内容からデータリストを作成
 */
static DATA_INFO *clipboard_to_datainfo(const HWND hWnd)
{
	DATA_INFO *ret_di;
	TCHAR err_str[BUF_SIZE];

	// クリップボードの初期化
	if (OpenClipboard(hWnd) == FALSE) {
		return NULL;
	}
	// データを取得
	ret_di = clipboard_get_datainfo(FALSE, FALSE, err_str);
	CloseClipboard();
	return ret_di;
}

/*
 * viewer_get_clipboard_data - クリップボードの内容取得
 */
void viewer_get_clipboard_data(const HWND hWnd, const HTREEITEM hItem)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	DATA_INFO *di;
	HANDLE data;

	set_cursor(TRUE);
	if (hItem == clip_treeitem) {
		// クリップボードの全データ取得
		if (OpenClipboard(hWnd) == TRUE) {
			for (di = clip_di.child; di != NULL; di = di->next) {
				if (di->data == NULL && (data = GetClipboardData(di->format)) != NULL) {
					if ((di->data = format_copy_data(di->format_name, data, &di->size)) == NULL) {
						di->data = clipboard_copy_data(di->format, data, &di->size);
					}
				}
			}
			CloseClipboard();
		}
	} else if (treeview_get_rootitem(hTreeView, hItem) == clip_treeitem) {
		// クリップボードのデータ取得
		di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem);
		if (di != NULL && di->data == NULL && OpenClipboard(hWnd) == TRUE) {
			if ((data = GetClipboardData(di->format)) != NULL) {
				if ((di->data = format_copy_data(di->format_name, data, &di->size)) == NULL) {
					di->data = clipboard_copy_data(di->format, data, &di->size);
				}
			}
			CloseClipboard();
		}
	}
	set_cursor(FALSE);
}

/*
 * treeview_to_listview - ツリービューの内容をリストビューにコピー
 */
void treeview_to_listview(const HWND hTreeView, const HTREEITEM parent_item, const HWND hListView)
{
	HTREEITEM hItem;
	HTREEITEM start_item;
	HTREEITEM last_item = NULL;
	HTREEITEM find_item;
	LV_ITEM lvi;
	int i, j;

	SendMessage(hListView, WM_SETREDRAW, (WPARAM)FALSE, 0);

	// ツリービューアイテムの末尾を取得
	hItem = TreeView_GetNextItem(hTreeView, parent_item, TVGN_CHILD);
	while (hItem != NULL) {
		last_item = hItem;
		hItem = TreeView_GetNextItem(hTreeView, hItem, TVGN_NEXT);
	}
	if (last_item == NULL) {
		SendMessage(hListView, WM_SETREDRAW, (WPARAM)TRUE, 0);
		UpdateWindow(hListView);
		return;
	}
	start_item = last_item;
	// リストビューアイテムの削除
	for (i = ListView_GetItemCount(hListView) - 1; i >= 0; i--) {
		hItem = (HTREEITEM)listview_get_lparam(hListView, i);
		if (hItem == NULL) {
			ListView_DeleteItem(hListView, i);
			continue;
		}
		// アイテムの検索
		find_item = start_item;
		while (find_item != NULL && find_item != hItem) {
			find_item = TreeView_GetNextItem(hTreeView, find_item, TVGN_PREVIOUS);
		}
		if (find_item == NULL) {
			find_item = last_item;
			while (find_item != start_item && find_item != hItem) {
				find_item = TreeView_GetNextItem(hTreeView, find_item, TVGN_PREVIOUS);
			}
			if (find_item != hItem) {
				find_item = NULL;
			}
		}
		if (find_item == NULL) {
			// ツリービュー見つからないアイテムを削除
			ListView_DeleteItem(hListView, i);
		} else {
			// 次の検索開始位置
			start_item = find_item;
		}
	}

	// リストビューアイテムの追加
	find_item = TreeView_GetNextItem(hTreeView, parent_item, TVGN_CHILD);
	i = 0;
	while (find_item != NULL) {
		// リストビューのアイテム情報設定
		lvi.mask = LVIF_TEXT | TVIF_IMAGE | LVIF_PARAM;
		lvi.iItem = i;
		lvi.iSubItem = 0;
		lvi.pszText = LPSTR_TEXTCALLBACK;
		lvi.cchTextMax = 0;
		lvi.iImage = I_IMAGECALLBACK;
		lvi.lParam = (LPARAM)find_item;

		if ((hItem = (HTREEITEM)listview_get_lparam(hListView, i)) == find_item) {
			ListView_SetItem(hListView, &lvi);
		} else {
			// リストビューアイテムの追加
			ListView_InsertItem(hListView, &lvi);
		}
		find_item = TreeView_GetNextItem(hTreeView, find_item, TVGN_NEXT);
		i++;
	}
	if (i != ListView_GetItemCount(hListView)) {
		for (j = ListView_GetItemCount(hListView); j >= i; j--) {
			ListView_DeleteItem(hListView, j);
		}
	}
	ListView_RedrawItems(hListView, 0, ListView_GetItemCount(hListView));
	SendMessage(hListView, WM_SETREDRAW, (WPARAM)TRUE, 0);
	UpdateWindow(hListView);
}

/*
 * listview_get_disp_item - リストビューに表示するアイテム情報の設定
 */
static void listview_get_disp_item(const HWND hTreeView, LV_ITEM *lvi)
{
	HTREEITEM hItem = (HTREEITEM)lvi->lParam;
	DATA_INFO *di, *cdi;
	TCHAR *str_hkey;
	int size;

	if (hItem == NULL) {
		return;
	}
	if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, hItem)) == NULL) {
		return;
	}

	// アイコン
	if (lvi->mask & LVIF_IMAGE) {
		lvi->iImage = treeview_get_icon(hTreeView, hItem);
	}

	// テキスト
	if (lvi->mask & LVIF_TEXT) {
		switch (lvi->iSubItem) {
		case 0:
			treeview_get_text(hTreeView, hItem, lvi->pszText);
			break;

		case 1:
			if (di->type != TYPE_FOLDER) {
				size = 0;
				for (cdi = di->child; cdi != NULL; cdi = cdi->next) {
					size += cdi->size;
				}
				_itot_s(size, lvi->pszText, lvi->cchTextMax, 10);
			}
			break;

		case 2:
			if (di->type != TYPE_FOLDER) {
				data_get_modified_string(di, lvi->pszText);
			}
			break;

		case 3:
			if (treeview_get_rootitem(hTreeView, hItem) == history_treeitem) {
				// ウィンドウタイトル
				if (di->window_name != NULL) {
					lstrcpy(lvi->pszText, di->window_name);
				}
			} else {
				// ホットキー
				if ((str_hkey = menu_get_keyname(di->op_modifiers, di->op_virtkey)) != NULL) {
					lstrcpy(lvi->pszText, str_hkey);
					mem_free(&str_hkey);
				}
			}
			break;
		}
	}
}

/*
 * viewer_save_data - 表示データの保存
 */
static void viewer_save_data(const HWND hWnd, const HTREEITEM hItem)
{
	DATA_INFO *di;
	BOOL ret;

	if (save_flag == FALSE || current_wnd == NULL ||
		treeview_get_rootitem(GetDlgItem(hWnd, ID_TREE), hItem) == clip_treeitem) {
		return;
	}
	// データ取得
	if ((di = (DATA_INFO *)treeview_get_lparam(GetDlgItem(hWnd, ID_TREE), hItem)) == NULL) {
		return;
	}
	switch (di->type) {
	case TYPE_FOLDER:
		break;

	case TYPE_ITEM:
		if ((di = format_get_priority_highest(di)) == NULL) {
			return;
		}
	case TYPE_DATA:
		// データ保存
		set_cursor(TRUE);
		if (current_wnd == GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_BINVIEW)) {
			// バイナリ表示の保存
			ret = SendMessage(current_wnd, WM_SAVE_BINDATA, 0, (LPARAM)di);
		} else {
			// 形式毎のデータ保存
			ret = format_window_save_data(current_wnd, di);
		}
		if (ret == TRUE) {
			// ツリービューアイテムに反映
			treeview_title_refresh(GetDlgItem(hWnd, ID_TREE), hItem);
			viewer_set_datetime(hWnd, hItem);

			if (treeview_get_rootitem(GetDlgItem(hWnd, ID_TREE), hItem) == regist_treeitem) {
				// 登録アイテムの保存
				SendMessage(hWnd, WM_REGIST_SAVE, 0, 0);
			}
			SendMessage(hWnd, WM_VIEWER_REFRESH_STATUS, 0, 0);
		}
		set_cursor(FALSE);
		break;
	}
}

/*
 * viewer_set_list_column - カラム表示設定
 */
static void viewer_set_list_column(const HWND hTreeView, const HWND hListView, const HTREEITEM hItem)
{
	LVCOLUMN lvcol;

	ZeroMemory(&lvcol, sizeof(LVCOLUMN));
	lvcol.mask = LVCF_TEXT;
	ListView_GetColumn(hListView, 3, &lvcol);
	if (treeview_get_rootitem(hTreeView, hItem) == history_treeitem) {
		lvcol.pszText = message_get_res(IDS_LISTCOLUMN_WINDOW);
	} else {
		lvcol.pszText = message_get_res(IDS_LISTCOLUMN_HOTKEY);
	}
	ListView_SetColumn(hListView, 3, &lvcol);
}

/*
 * viewer_sel_cheange - 選択変更
 */
static BOOL viewer_sel_cheange(const HWND hWnd, const HTREEITEM old_item, const HTREEITEM new_item)
{
	HWND hTreeView = GetDlgItem(hWnd, ID_TREE);
	HWND hContainer = GetDlgItem(hWnd, ID_CONTAINER);
	BOOL redraw = IsWindowVisible(hContainer);
	HTREEITEM cItem = new_item;
	DATA_INFO *di;
	int i;

	if (redraw) SendMessage(hContainer, WM_SETREDRAW, (WPARAM)FALSE, 0);

	if (current_wnd != NULL) {
		// 旧ウィンドウを非表示
		ShowWindow(current_wnd, SW_HIDE);
		if (old_item != NULL) {
			if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, old_item)) != NULL) {
				switch (di->type) {
				case TYPE_FOLDER:
					break;

				case TYPE_ITEM:
					if ((di = format_get_priority_highest(di)) == NULL) {
						break;
					}
				case TYPE_DATA:
					// データ保存
					viewer_save_data(hWnd, old_item);
					// データ非表示
					if (current_wnd == GetDlgItem(hContainer, ID_BINVIEW)) {
						SendMessage(current_wnd, WM_SET_BINDATA, 0, 0);
					} else {
						format_window_hide_data(current_wnd, di);
					}
					break;
				}
			}
		}
	}
	current_wnd = NULL;

	if (new_item != NULL) {
		if (new_item == history_treeitem || new_item == regist_treeitem) {
			current_wnd = GetDlgItem(hContainer, ID_LIST);
			ListView_DeleteAllItems(GetDlgItem(hContainer, ID_LIST));
			treeview_to_listview(hTreeView, new_item, GetDlgItem(hContainer, ID_LIST));
			ListView_SetItemState(GetDlgItem(hContainer, ID_LIST), 0, LVIS_FOCUSED, LVIS_FOCUSED);
			viewer_set_list_column(hTreeView, GetDlgItem(hContainer, ID_LIST), new_item);

		} else {
			if ((di = (DATA_INFO *)treeview_get_lparam(hTreeView, new_item)) == NULL) {
				if (redraw) SendMessage(hContainer, WM_SETREDRAW, (WPARAM)TRUE, 0);
				InvalidateRect(hContainer, NULL, FALSE);
				UpdateWindow(hContainer);
				return FALSE;
			}
			switch (di->type) {
			case TYPE_FOLDER:
				current_wnd = GetDlgItem(hContainer, ID_LIST);
				ListView_DeleteAllItems(GetDlgItem(hContainer, ID_LIST));
				treeview_to_listview(hTreeView, new_item, GetDlgItem(hContainer, ID_LIST));
				ListView_SetItemState(GetDlgItem(hContainer, ID_LIST), 0, LVIS_FOCUSED, LVIS_FOCUSED);
				viewer_set_list_column(hTreeView, GetDlgItem(hContainer, ID_LIST), new_item);
				break;

			case TYPE_ITEM:
				// 優先順位の高いデータを取得
				if ((di = format_get_priority_highest(di)) == NULL) {
					if (redraw) SendMessage(hContainer, WM_SETREDRAW, (WPARAM)TRUE, 0);
					InvalidateRect(hContainer, NULL, FALSE);
					UpdateWindow(hContainer);
					return FALSE;
				}
				if (cItem == clip_treeitem) {
					cItem = TreeView_GetChild(hTreeView, cItem);
					while (cItem != NULL) {
						if ((DATA_INFO *)treeview_get_lparam(hTreeView, cItem) == di) {
							break;
						}
						cItem = TreeView_GetNextSibling(hTreeView, cItem);
					}
				}

			case TYPE_DATA:
				// クリップボードの内容取得
				viewer_get_clipboard_data(hWnd, cItem);

				i = format_get_index(di->format_name, 0);
				if (option.viewer_show_bin == 0 && i != -1 && (option.format_info + i)->hWnd != NULL) {
					// 形式毎のウィンドウ
					current_wnd = (option.format_info + i)->hWnd;
					// データ表示
					format_window_show_data(current_wnd, di,
						(treeview_get_rootitem(hTreeView, new_item) == clip_treeitem) ? TRUE : FALSE);
				} else {
					// バイナリビュー
					current_wnd = GetDlgItem(hContainer, ID_BINVIEW);
					SendMessage(current_wnd, WM_SET_BINDATA,
						(treeview_get_rootitem(hTreeView, new_item) == clip_treeitem) ? TRUE : FALSE, (LPARAM)di);
				}
				break;
			}
		}
		if (current_wnd != NULL) {
			// ウィンドウ表示
			ShowWindow(current_wnd, SW_SHOW);
			SendMessage(hContainer, WM_SIZE, 0, 0);
		}
	}
	if (redraw) SendMessage(hContainer, WM_SETREDRAW, (WPARAM)TRUE, 0);
	InvalidateRect(hContainer, NULL, FALSE);
	UpdateWindow(hContainer);

	SendMessage(hWnd, WM_VIEWER_REFRESH_STATUS, 0, 0);
	return TRUE;
}

/*
 * viewer_initialize - ウィンドウの初期化
 */
typedef struct {
	const TCHAR *title;
	size_t offset;
	int minimum;
	int maximum;
} CLIPMASTER_SETTING;

static const CLIPMASTER_SETTING clipmaster_settings[] = {
	{ TEXT("전체화면 대비 기본 화면 크기 X축 비율"), FIELD_OFFSET(OPTION_INFO, config_width_percent), 10, 100 },
	{ TEXT("Y축 비율"), FIELD_OFFSET(OPTION_INFO, config_height_percent), 10, 100 },
	{ TEXT("마지막 화면 설정 자동 적용"), FIELD_OFFSET(OPTION_INFO, config_remember_layout), 0, 0 },
	{ TEXT("개수로 제한"), FIELD_OFFSET(OPTION_INFO, config_history_limit_enabled), 0, 0 },
	{ TEXT("히스토리 최대 개수"), FIELD_OFFSET(OPTION_INFO, history_max), 1, 5000 },
	{ TEXT("일수로 제한"), FIELD_OFFSET(OPTION_INFO, config_history_days_enabled), 0, 0 },
	{ TEXT("히스토리 보존 일수"), FIELD_OFFSET(OPTION_INFO, config_history_days), 1, 365 },
	{ TEXT("즐겨찾기 최대 개수 (10~100)"), FIELD_OFFSET(OPTION_INFO, config_favorite_max), 10, 100 },
	{ TEXT("재부팅할 때마다 히스토리 삭제"), FIELD_OFFSET(OPTION_INFO, config_clear_history_boot), 0, 0 },
	{ TEXT("Text"), FIELD_OFFSET(OPTION_INFO, config_exclude_text), 0, 0 },
	{ TEXT("RichText / HTML"), FIELD_OFFSET(OPTION_INFO, config_exclude_richtext), 0, 0 },
	{ TEXT("Image"), FIELD_OFFSET(OPTION_INFO, config_exclude_image), 0, 0 },
	{ TEXT("File"), FIELD_OFFSET(OPTION_INFO, config_exclude_file), 0, 0 },
	{ TEXT("Audio"), FIELD_OFFSET(OPTION_INFO, config_exclude_audio), 0, 0 },
	{ TEXT("Device"), FIELD_OFFSET(OPTION_INFO, config_exclude_device), 0, 0 },
	{ TEXT("Ansi2Unicode text 강제 적용"), FIELD_OFFSET(OPTION_INFO, config_force_unicode), 0, 0 },
	{ TEXT("Windows 시작 시 자동 실행"), FIELD_OFFSET(OPTION_INFO, config_start_with_windows), 0, 0 },
	{ TEXT("히스토리 저장 시 암호화 (DPAPI)"), FIELD_OFFSET(OPTION_INFO, config_encrypt_history), 0, 0 },
	{ TEXT("프로그램 전환 시 트레이로 감춤"), FIELD_OFFSET(OPTION_INFO, config_hide_tray_on_switch), 0, 0 }
};

static void viewer_settings_layout(HWND panel)
{
	RECT client;
	int index, row, column;
	int dpi = GetDpiForWindow(panel);
	int offset = GetScrollPos(panel, SB_VERT);
	int width, content, left, right, column_width, top, table_width, part, footer, buttons;
	BOOL wide;
	const int actions[] = { ID_CLIPMASTER_SAVE, 5482, 5483, 5484, 5485 };
	GetClientRect(panel, &client);
	width = MulDiv(client.right, 96, dpi);
	content = max(1, min(1040, width - 32));
	left = max(16, (width - content) / 2);
	wide = content >= 720;
	column_width = wide ? (content - 16) / 2 : content;
	right = wide ? left + column_width + 16 : left;
#define SETTINGS_PLACE(id, x, y, w, h) MoveWindow(GetDlgItem(panel, (id)), MulDiv((x), dpi, 96), MulDiv((y), dpi, 96) - offset, max(1, MulDiv((w), dpi, 96)), max(1, MulDiv((h), dpi, 96)), TRUE)
#define SETTINGS_NUMBER(id, x, y, w) SETTINGS_PLACE(6200 + (id), (x), (y) + 3, (w) - 104, 24); SETTINGS_PLACE(ID_CLIPMASTER_FIELD + (id), (x) + (w) - 96, (y), 68, 24); SETTINGS_PLACE(6300 + (id), (x) + (w) - 24, (y) + 3, 24, 24)
	SETTINGS_PLACE(6000, left, 12, column_width, 116);
	SETTINGS_NUMBER(0, left + 12, 38, column_width - 24);
	SETTINGS_NUMBER(1, left + 12, 66, column_width - 24);
	SETTINGS_PLACE(5102, left + 12, 94, column_width - 24, 24);
	SETTINGS_PLACE(6001, left, 168, column_width, 284);
	SETTINGS_PLACE(5103, left + 12, 194, column_width - 128, 24);
	SETTINGS_PLACE(5104, left + column_width - 108, 194, 68, 24);
	SETTINGS_PLACE(6304, left + column_width - 36, 197, 24, 24);
	SETTINGS_PLACE(5105, left + 12, 222, column_width - 128, 24);
	SETTINGS_PLACE(5106, left + column_width - 108, 222, 68, 24);
	SETTINGS_PLACE(6306, left + column_width - 36, 225, 24, 24);
	SETTINGS_NUMBER(7, left + 12, 250, column_width - 24);
	SETTINGS_PLACE(5108, left + 12, 278, column_width - 24, 24);
	SETTINGS_PLACE(6450, left + 12, 308, column_width - 24, 22);
	SETTINGS_PLACE(5410, left + 12, 332, column_width - 24, 24);
	SETTINGS_PLACE(5412, left + 12, 356, column_width - 24, 24);
	SETTINGS_PLACE(5411, left + 12, 380, column_width - 24, 24);
	SETTINGS_PLACE(6002, left, 464, column_width, 184);
	for (index = 0; index < 4; index++) {
		SETTINGS_PLACE(6400 + index, left + 12, 490 + index * 28, 112, 24);
		SETTINGS_PLACE(5400 + index, left + 132, 488 + index * 28, column_width - 144, 24);
	}
	SETTINGS_PLACE(6430, left + 12, 604, column_width - 128, 24);
	SETTINGS_PLACE(5430, left + column_width - 108, 602, 68, 24);
	SETTINGS_PLACE(6431, left + column_width - 36, 605, 24, 24);
	top = wide ? 12 : 660;
	SETTINGS_PLACE(6003, right, top, column_width, 116);
	for (index = 10; index <= 15; index++) {
		row = (index - 10) / 2;
		column = (index - 10) % 2;
		SETTINGS_PLACE(5100 + index, right + 12 + column * ((column_width - 24) / 2), top + 26 + row * 28,
			(column_width - 24) / 2, 24);
	}
	top += 128;
	SETTINGS_PLACE(6004, right, top, column_width, 164);
	SETTINGS_PLACE(5116, right + 12, top + 26, column_width - 24, 24);
	for (index = 0; index < 3; index++) {
		int label_width = min(164, (column_width - 24) / 2);
		SETTINGS_PLACE(6440 + index, right + 12, top + 58 + index * 30, label_width, 24);
		SETTINGS_PLACE(5440 + index, right + 20 + label_width, top + 54 + index * 30, column_width - label_width - 32, 24);
	}
	top += 176;
	SETTINGS_PLACE(6005, right, top, column_width, 132);
	for (index = 17; index < 20; index++) SETTINGS_PLACE(ID_CLIPMASTER_FIELD + index, right + 12, top + 26 + (index - 17) * 30, column_width - 24, 24);
	top = max(648, top + 132) + 12;
	settings_handler_top = top + 52;
	SETTINGS_PLACE(6006, left, top, content, 96 + settings_handler_count * 32);
	table_width = max(1, content - 76);
	part = table_width / 4;
	for (column = 0; column < 3; column++) {
		int x = left + 48 + column * (part + 8);
		int field_width = column == 2 ? table_width - part * 2 : part;
		SETTINGS_PLACE(5590 + column, x, top + 26, field_width, 24);
		for (index = 0; index < 16; index++) {
			SETTINGS_PLACE(5500 + index * 3 + column, x, top + 52 + index * 32, field_width, 26);
			ShowWindow(GetDlgItem(panel, 5500 + index * 3 + column), index < settings_handler_count ? SW_SHOW : SW_HIDE);
		}
	}
	for (index = 0; index < 16; index++) {
		SETTINGS_PLACE(5700 + index, left + 12, top + 55 + index * 32, 32, 24);
		ShowWindow(GetDlgItem(panel, 5700 + index), index < settings_handler_count ? SW_SHOW : SW_HIDE);
	}
	SETTINGS_PLACE(5480, left + 12, top + 56 + settings_handler_count * 32, 112, 28);
	SETTINGS_PLACE(5481, left + 132, top + 56 + settings_handler_count * 32, 112, 28);
	EnableWindow(GetDlgItem(panel, 5480), settings_handler_count < 16);
	EnableWindow(GetDlgItem(panel, 5481), settings_handler_count > 3);
	footer = top + 108 + settings_handler_count * 32;
	buttons = min(5, max(1, content / 132));
	for (index = 0; index < 5; index++) SETTINGS_PLACE(actions[index], left + (index % buttons) * ((content + 8) / buttons),
		footer + (index / buttons) * 40, (content - (buttons - 1) * 8) / buttons, 32);
	footer += ((5 + buttons - 1) / buttons) * 40;
	SETTINGS_PLACE(ID_CLIPMASTER_STATUS, left, footer + 8, content, 64);
	settings_height = footer + 88;
	EnableWindow(GetDlgItem(panel, 5104), IsDlgButtonChecked(panel, 5103) == BST_CHECKED);
	EnableWindow(GetDlgItem(panel, 5106), IsDlgButtonChecked(panel, 5105) == BST_CHECKED);
#undef SETTINGS_NUMBER
#undef SETTINGS_PLACE
}

static BOOL CALLBACK viewer_settings_set_font(HWND control, LPARAM font)
{
	SendMessage(control, WM_SETFONT, (WPARAM)font, TRUE);
	return TRUE;
}

static void viewer_settings_font(HWND panel)
{
	HFONT previous = (HFONT)GetProp(panel, TEXT("ClipMaster.SettingsFont"));
	HFONT font = CreateFont(-MulDiv(9, GetDpiForWindow(panel), 72), 0, 0, 0, FW_NORMAL,
		FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH, TEXT("Malgun Gothic"));
	if (font == NULL) return;
	SetProp(panel, TEXT("ClipMaster.SettingsFont"), font);
	SendMessage(panel, WM_SETFONT, (WPARAM)font, TRUE);
	EnumChildWindows(panel, viewer_settings_set_font, (LPARAM)font);
	if (previous != NULL) DeleteObject(previous);
}

BOOL viewer_settings_message(HWND hWnd, MSG *message)
{
	HWND panel = GetDlgItem(hWnd, ID_CLIPMASTER_PANEL);
	if (message->hwnd == GetDlgItem(hWnd, ID_CLIPMASTER_SEARCH) &&
		message->message >= WM_KEYFIRST && message->message <= WM_KEYLAST) {
		TranslateMessage(message);
		DispatchMessage(message);
		return TRUE;
	}
	if (IsWindowVisible(panel) && (message->hwnd == panel || IsChild(panel, message->hwnd))) {
		if (message->message == WM_MOUSEWHEEL) {
			SendMessage(panel, WM_MOUSEWHEEL, message->wParam, message->lParam);
			return TRUE;
		}
		return IsDialogMessage(panel, message);
	}
	return FALSE;
}

static LRESULT CALLBACK viewer_settings_proc(HWND panel, UINT msg, WPARAM wParam, LPARAM lParam,
	UINT_PTR subclass_id, DWORD_PTR reference)
{
	SCROLLINFO scroll;
	RECT client;
	UNREFERENCED_PARAMETER(subclass_id);
	UNREFERENCED_PARAMETER(reference);
	if (msg == WM_COMMAND) {
		if (LOWORD(wParam) == 5103 || LOWORD(wParam) == 5105) viewer_settings_layout(panel);
		return SendMessage(GetParent(panel), msg, wParam, lParam);
	}
	if (msg == WM_NCDESTROY) {
		HFONT font = (HFONT)RemoveProp(panel, TEXT("ClipMaster.SettingsFont"));
		if (font != NULL) DeleteObject(font);
	}
	if (msg == WM_SIZE || msg == WM_VSCROLL || msg == WM_MOUSEWHEEL || msg == WM_DPICHANGED_AFTERPARENT) {
		if (msg == WM_DPICHANGED_AFTERPARENT) viewer_settings_font(panel);
		viewer_settings_layout(panel);
		ZeroMemory(&scroll, sizeof(scroll));
		scroll.cbSize = sizeof(scroll);
		scroll.fMask = SIF_ALL;
		GetScrollInfo(panel, SB_VERT, &scroll);
		GetClientRect(panel, &client);
		scroll.nMin = 0;
		scroll.nMax = MulDiv(CLIPMASTER_SETTINGS_HEIGHT, GetDpiForWindow(panel), 96) - 1;
		scroll.nPage = client.bottom;
		if (msg == WM_MOUSEWHEEL) {
			scroll.nPos -= GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * Scale(48);
		} else if (msg == WM_VSCROLL) {
			switch (LOWORD(wParam)) {
			case SB_LINEUP: scroll.nPos -= Scale(24); break;
			case SB_LINEDOWN: scroll.nPos += Scale(24); break;
			case SB_PAGEUP: scroll.nPos -= client.bottom; break;
			case SB_PAGEDOWN: scroll.nPos += client.bottom; break;
			case SB_THUMBPOSITION:
			case SB_THUMBTRACK: scroll.nPos = scroll.nTrackPos; break;
			case SB_TOP: scroll.nPos = 0; break;
			case SB_BOTTOM: scroll.nPos = scroll.nMax; break;
			}
		}
		scroll.fMask |= SIF_DISABLENOSCROLL;
		SetScrollInfo(panel, SB_VERT, &scroll, TRUE);
		viewer_settings_layout(panel);
		InvalidateRect(panel, NULL, TRUE);
		return 0;
	}
	return DefSubclassProc(panel, msg, wParam, lParam);
}

static HWND viewer_setting_control(HWND parent, const TCHAR *class_name, const TCHAR *text,
	DWORD style, int identifier, int left, int top, int width, int height)
{
	HWND control = CreateWindowEx(lstrcmp(class_name, TEXT("EDIT")) == 0 ? WS_EX_CLIENTEDGE : 0,
		class_name, text, WS_CHILD | WS_VISIBLE | style, Scale(left), Scale(top), Scale(width), Scale(height),
		parent, (HMENU)(INT_PTR)identifier, hInst, NULL);
	HFONT font = (HFONT)GetProp(parent, TEXT("ClipMaster.SettingsFont"));
	SendMessage(control, WM_SETFONT, (WPARAM)(font != NULL ? font : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
	return control;
}

static void viewer_create_settings(const HWND hWnd)
{
	HWND panel = CreateWindowEx(WS_EX_CONTROLPARENT, TEXT("STATIC"), TEXT(""),
		WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 0, 0, hWnd, (HMENU)ID_CLIPMASTER_PANEL, hInst, NULL);
	int index;
	int top = 52;
	int value;
	const CLIPMASTER_SETTING *setting;
	const TCHAR *groups[] = { TEXT("기본 설정"), TEXT("히스토리 보존 정책"), TEXT("단축키"),
		TEXT("저장 제외 clip category"), TEXT("붙여넣기 handler (서식명, 콤마로 구분)"), TEXT("프로그램 기능"), TEXT("Paste handler") };
	settings_handler_count = option.config_handler_count;
	SetWindowSubclass(panel, viewer_settings_proc, 1, 0);
	viewer_settings_font(panel);
	for (index = 0; index < ARRAYSIZE(groups); index++)
		viewer_setting_control(panel, TEXT("BUTTON"), groups[index], BS_GROUPBOX, 6000 + index, 0, 0, 1, 1);
	for (index = 0; index < ARRAYSIZE(clipmaster_settings); index++) {
		int field = index < 9 ? index : index + 1;
		if (field == 17) {
			int row, column;
			top += 76;
			settings_handler_top = top;
			viewer_setting_control(panel, TEXT("STATIC"), TEXT("대상"), 0, 5590, 16, top - 26, 96, 24);
			viewer_setting_control(panel, TEXT("STATIC"), TEXT("조건"), 0, 5591, 116, top - 26, 96, 24);
			viewer_setting_control(panel, TEXT("STATIC"), TEXT("처리 (PowerShell)"), 0, 5592, 216, top - 26, 96, 24);
			for (row = 0; row < 16; row++) {
				TCHAR label[16];
				wsprintf(label, TEXT("PH%d"), row + 1);
				viewer_setting_control(panel, TEXT("STATIC"), label, 0, 5700 + row, 16, top + row * 34, 28, 24);
				for (column = 0; column < 3; column++) {
					const TCHAR *text = column == 0 ? option.config_handlers[row].formats :
						column == 1 ? option.config_handlers[row].pattern : option.config_handlers[row].script;
					HWND edit = viewer_setting_control(panel, TEXT("EDIT"), text, ES_AUTOHSCROLL | WS_TABSTOP,
						5500 + row * 3 + column, 48, top + row * 34, 96, 26);
					SendMessage(edit, EM_SETLIMITTEXT, column == 0 ? 127 : column == 1 ? 255 : 2047, 0);
				}
			}
			viewer_setting_control(panel, TEXT("BUTTON"), TEXT("Handler 추가"), BS_PUSHBUTTON | WS_TABSTOP, 5480, 16, top, 136, 28);
			viewer_setting_control(panel, TEXT("BUTTON"), TEXT("마지막 행 삭제"), BS_PUSHBUTTON | WS_TABSTOP, 5481, 160, top, 136, 28);
			top += settings_handler_count * 34 + 64;
			top += 36;
		}
		setting = &clipmaster_settings[index];
		value = *(int *)((BYTE *)&option + setting->offset);
		if (setting->maximum == 0) {
			viewer_setting_control(panel, TEXT("BUTTON"), setting->title, BS_AUTOCHECKBOX | WS_TABSTOP,
				ID_CLIPMASTER_FIELD + field, 16, top, 288, 24);
			CheckDlgButton(panel, ID_CLIPMASTER_FIELD + field, value ? BST_CHECKED : BST_UNCHECKED);
		} else {
			if (index == 0 || index == 1 || index == 7)
				viewer_setting_control(panel, TEXT("STATIC"), setting->title, 0, 6200 + index, 16, top + 3, 208, 24);
			viewer_setting_control(panel, TEXT("EDIT"), TEXT(""), ES_NUMBER | ES_RIGHT | WS_TABSTOP,
				ID_CLIPMASTER_FIELD + index, 232, top, 64, 24);
			SetDlgItemInt(panel, ID_CLIPMASTER_FIELD + index, value, FALSE);
			viewer_setting_control(panel, TEXT("STATIC"), index < 2 ? TEXT("%") : index == 6 ? TEXT("일") : TEXT("개"),
				0, 6300 + index, 0, 0, 1, 1);
		}
		top += 34;
		if (field == 8) {
			const TCHAR *hotkey_names[] = { TEXT("ClipManager"), TEXT("FavoriteClips"), TEXT("Paste"), TEXT("PastePlus") };
			const TCHAR *hotkeys[] = { option.config_hotkey_clipmanager, option.config_hotkey_favorites,
				option.config_hotkey_paste, option.config_hotkey_pasteplus };
			int key;
			viewer_setting_control(panel, TEXT("STATIC"), TEXT("History 중복 저장 검사 정책"), 0, 6450, 0, 0, 1, 1);
			viewer_setting_control(panel, TEXT("BUTTON"), TEXT("검사 안 함 (중복 저장 허용)"), BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP,
				5410, 16, top, 288, 24);
			viewer_setting_control(panel, TEXT("BUTTON"), TEXT("전체 검사 후 중복 없이 저장"), BS_AUTORADIOBUTTON | WS_TABSTOP,
				5412, 16, top + 30, 288, 24);
			viewer_setting_control(panel, TEXT("BUTTON"), TEXT("24시간 이내 중복 없이 저장"), BS_AUTORADIOBUTTON | WS_TABSTOP,
				5411, 16, top + 30, 288, 24);
			CheckRadioButton(panel, 5410, 5412, option.config_duplicate_policy == 0 ? 5410 : option.config_duplicate_policy == 1 ? 5412 : 5411);
			top += 80;
			top += 36;
			for (key = 0; key < 4; key++) {
				HWND edit;
				viewer_setting_control(panel, TEXT("STATIC"), hotkey_names[key], 0, 6400 + key, 16, top + 3, 124, 24);
				edit = viewer_setting_control(panel, TEXT("EDIT"), hotkeys[key], ES_READONLY | ES_AUTOHSCROLL | WS_TABSTOP,
					5400 + key, 144, top, 160, 26);
				cm_hotkey_edit(edit, main_wnd);
				top += 34;
			}
		}
	}
	{
		const TCHAR *names[] = { TEXT("특수 서식 제외"), TEXT("Unicode text 변환"), TEXT("이미지 서식 선택") };
		const TCHAR *values[] = { option.config_handler_special, option.config_handler_unicode, option.config_handler_image };
		for (index = 0; index < 3; index++) {
			viewer_setting_control(panel, TEXT("STATIC"), names[index], 0, 6440 + index, 0, 0, 1, 1);
			HWND edit = viewer_setting_control(panel, TEXT("EDIT"), values[index], ES_AUTOHSCROLL | WS_TABSTOP, 5440 + index, 0, 0, 1, 1);
			SendMessage(edit, EM_SETLIMITTEXT, 255, 0);
		}
	}
	viewer_setting_control(panel, TEXT("STATIC"), TEXT("연속 실행 간격"), 0, 6430, 0, 0, 1, 1);
	viewer_setting_control(panel, TEXT("EDIT"), TEXT(""), ES_NUMBER | ES_RIGHT | WS_TABSTOP, 5430, 0, 0, 1, 1);
	SetDlgItemInt(panel, 5430, option.config_paste_interval, FALSE);
	viewer_setting_control(panel, TEXT("STATIC"), TEXT("ms"), 0, 6431, 0, 0, 1, 1);
	viewer_setting_control(panel, TEXT("BUTTON"), TEXT("설정 저장"), BS_PUSHBUTTON | WS_TABSTOP,
		ID_CLIPMASTER_SAVE, 16, top + 20, 132, 32);
	viewer_setting_control(panel, TEXT("BUTTON"), TEXT("저장 후 재시작"), BS_PUSHBUTTON | WS_TABSTOP, 5483, 160, top + 20, 136, 32);
	viewer_setting_control(panel, TEXT("BUTTON"), TEXT("설정 초기화"), BS_PUSHBUTTON | WS_TABSTOP, 5482, 16, top + 64, 280, 32);
	viewer_setting_control(panel, TEXT("BUTTON"), TEXT("히스토리 초기화"), BS_PUSHBUTTON | WS_TABSTOP, 5484, 16, top + 108, 280, 32);
	viewer_setting_control(panel, TEXT("BUTTON"), TEXT("프로그램 종료"), BS_PUSHBUTTON | WS_TABSTOP, 5485, 16, top + 152, 136, 32);
	viewer_setting_control(panel, TEXT("STATIC"), TEXT(""), SS_LEFT,
		ID_CLIPMASTER_STATUS, 16, top + 64, 288, 80);
	viewer_settings_layout(panel);
	for (index = 0; index < ARRAYSIZE(groups); index++)
		SetWindowPos(GetDlgItem(panel, 6000 + index), HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

static BOOL viewer_save_settings(const HWND hWnd)
{
	HWND panel = GetDlgItem(hWnd, ID_CLIPMASTER_PANEL);
	OPTION_INFO previous = option;
	OPTION_INFO candidate = option;
	const CLIPMASTER_SETTING *setting;
	int index;
	UINT value;
	BOOL valid;
	TCHAR error[256];
	for (index = 0; index < ARRAYSIZE(clipmaster_settings); index++) {
		int field = index < 9 ? index : index + 1;
		setting = &clipmaster_settings[index];
		if (setting->maximum == 0) {
			*(int *)((BYTE *)&candidate + setting->offset) =
				IsDlgButtonChecked(panel, ID_CLIPMASTER_FIELD + field) == BST_CHECKED;
			continue;
		}
		valid = FALSE;
		value = GetDlgItemInt(panel, ID_CLIPMASTER_FIELD + index, &valid, FALSE);
		if (!valid || value < (UINT)setting->minimum || value > (UINT)setting->maximum) {
			wsprintf(error, TEXT("%s: enter a value from %d to %d."), setting->title, setting->minimum, setting->maximum);
			SetDlgItemText(panel, ID_CLIPMASTER_STATUS, error);
			SetFocus(GetDlgItem(panel, ID_CLIPMASTER_FIELD + index));
			return FALSE;
		}
		*(int *)((BYTE *)&candidate + setting->offset) = (int)value;
	}
	if (!candidate.config_history_limit_enabled && !candidate.config_history_days_enabled) {
		SetDlgItemText(panel, ID_CLIPMASTER_STATUS, TEXT("Select at least one history retention limit."));
		return FALSE;
	}
	{
		TCHAR *keys[] = { candidate.config_hotkey_clipmanager, candidate.config_hotkey_favorites,
			candidate.config_hotkey_paste, candidate.config_hotkey_pasteplus };
		CM_HOTKEY parsed[4];
		int key, other;
		DATA_INFO *favorite;
		int count = 0;
		ZeroMemory(parsed, sizeof(parsed));
		for (key = 0; key < 4; key++) {
			GetDlgItemText(panel, 5400 + key, keys[key], 64);
			if (key == 3 && !*keys[key]) continue;
			if (!cm_hotkey_parse(keys[key], &parsed[key])) {
				SetDlgItemText(panel, ID_CLIPMASTER_STATUS, TEXT("Enter a modifier and a key for each hotkey."));
				return FALSE;
			}
			for (other = 0; other < key; other++) {
				if (parsed[key].key == parsed[other].key && parsed[key].modifiers == parsed[other].modifiers && parsed[key].caps == parsed[other].caps) {
					SetDlgItemText(panel, ID_CLIPMASTER_STATUS, TEXT("Hotkeys must be different."));
					return FALSE;
				}
			}
		}
		for (favorite = regist_data.child; favorite != NULL; favorite = favorite->next)
			if (favorite->type == TYPE_ITEM) count++;
		if (count > candidate.config_favorite_max) {
			SetDlgItemText(panel, ID_CLIPMASTER_STATUS, TEXT("Remove favorites before lowering the maximum below the current count."));
			return FALSE;
		}
	}
	candidate.config_duplicate_policy = IsDlgButtonChecked(panel, 5411) == BST_CHECKED ? 2 :
		IsDlgButtonChecked(panel, 5412) == BST_CHECKED ? 1 : 0;
	value = GetDlgItemInt(panel, 5430, &valid, FALSE);
	if (!valid || value > 60000) {
		SetDlgItemText(panel, ID_CLIPMASTER_STATUS, TEXT("연속 실행 간격은 0~60000ms 범위로 입력해 주세요."));
		return FALSE;
	}
	candidate.config_paste_interval = (int)value;
	GetDlgItemText(panel, 5440, candidate.config_handler_special, 256);
	GetDlgItemText(panel, 5441, candidate.config_handler_unicode, 256);
	GetDlgItemText(panel, 5442, candidate.config_handler_image, 256);
	candidate.config_handler_count = settings_handler_count;
	for (index = 0; index < 16; index++) {
		if (index >= settings_handler_count) { ZeroMemory(&candidate.config_handlers[index], sizeof(candidate.config_handlers[index])); continue; }
		GetDlgItemText(panel, 5500 + index * 3, candidate.config_handlers[index].formats, 128);
		GetDlgItemText(panel, 5501 + index * 3, candidate.config_handlers[index].pattern, 256);
		GetDlgItemText(panel, 5502 + index * 3, candidate.config_handlers[index].script, 2048);
		if ((!*candidate.config_handlers[index].formats) != (!*candidate.config_handlers[index].script)) {
			SetDlgItemText(panel, ID_CLIPMASTER_STATUS, TEXT("Each handler needs both a target format and a PowerShell action."));
			return FALSE;
		}
	}
	option = candidate;
	if (!SendMessage(main_wnd, WM_CONFIG_APPLY, 0, (LPARAM)&previous)) {
		option = previous;
		SetDlgItemText(panel, ID_CLIPMASTER_STATUS, TEXT("Could not apply settings. Check hotkey conflicts and storage permissions."));
		return FALSE;
	}
	return TRUE;
}

static LRESULT CALLBACK viewer_search_proc(HWND window, UINT message, WPARAM parameter, LPARAM data,
	UINT_PTR subclass_id, DWORD_PTR reference)
{
	UNREFERENCED_PARAMETER(subclass_id);
	UNREFERENCED_PARAMETER(reference);
	if (message == WM_KEYDOWN && parameter == VK_RETURN) {
		SendMessage(GetParent(window), WM_TILE_SEARCH, 0, 0);
		return 0;
	}
	if (message == WM_CHAR && parameter == VK_RETURN) {
		return 0;
	}
	return DefSubclassProc(window, message, parameter, data);
}

static void viewer_set_paste_target(HWND target)
{
	DWORD process = 0;
	DWORD thread = GetWindowThreadProcessId(target, &process);
	GUITHREADINFO information;
	if (thread == 0 || process == GetCurrentProcessId()) {
		return;
	}
	paste_target = target;
	paste_focus = NULL;
	ZeroMemory(&information, sizeof(information));
	information.cbSize = sizeof(information);
	if (GetGUIThreadInfo(thread, &information)) {
		paste_focus = information.hwndFocus;
	}
}

static BOOL viewer_paste_clip(HWND window, DATA_INFO *item, BOOL exact)
{
	BOOL copied;
	if (exact) {
		TCHAR error[BUF_SIZE] = TEXT("");
		DATA_INFO *copy = data_item_copy(item, FALSE, FALSE, error);
		if (copy == NULL) return FALSE;
		copied = clipboard_set_datainfo(window, copy, error);
		data_free(copy);
		if (!copied && *error) MessageBox(window, error, ERROR_TITLE, MB_OK | MB_ICONERROR);
	} else copied = (BOOL)SendMessage(main_wnd, WM_ITEM_TO_CLIPBOARD, 1, (LPARAM)item);
	if (!copied) return FALSE;
	ShowWindow(window, SW_HIDE);
	if (IsWindow(paste_target)) {
		DWORD thread = GetWindowThreadProcessId(paste_target, NULL);
		_SetForegroundWindow(paste_target);
		if (IsWindow(paste_focus) && (paste_focus == paste_target || IsChild(paste_target, paste_focus))) {
			BOOL attached = AttachThreadInput(GetCurrentThreadId(), thread, TRUE);
			if (attached) { SetFocus(paste_focus); AttachThreadInput(GetCurrentThreadId(), thread, FALSE); }
		}
		if (GetForegroundWindow() == paste_target) {
			SendMessage(main_wnd, WM_UNREGIST_HOTKEY, 0, 0);
			sendkey_paste(paste_target);
			SendMessage(main_wnd, WM_REGIST_HOTKEY, 0, 0);
		}
	}
	return TRUE;
}

static BOOL viewer_commit_favorite(HWND window, DATA_INFO *original, DATA_INFO *replacement)
{
	DATA_INFO **link = &regist_data.child;
	DATA_INFO *next;
	if (original != NULL) {
		while (*link != NULL && *link != original) link = &(*link)->next;
		if (*link == NULL) { data_free(replacement); return FALSE; }
	}
	SendMessage(main_wnd, WM_UNREGIST_HOTKEY, 0, 0);
	TreeView_SelectItem(GetDlgItem(window, ID_TREE), regist_treeitem);
	next = original != NULL ? original->next : *link;
	if (replacement != NULL) replacement->next = next;
	*link = replacement != NULL ? replacement : next;
	if (!SendMessage(main_wnd, WM_REGIST_SAVE, 0, 0)) {
		*link = original != NULL ? original : next;
		if (replacement != NULL) { replacement->next = NULL; data_free(replacement); }
		SendMessage(main_wnd, WM_REGIST_HOTKEY, 0, 0);
		MessageBox(window, TEXT("Could not save favorites."), WINDOW_TITLE, MB_OK | MB_ICONERROR);
		return FALSE;
	}
	SendMessage(main_wnd, WM_REGIST_CHANGED, 0, 0);
	if (original != NULL) { original->next = NULL; data_free(original); }
	SendMessage(main_wnd, WM_REGIST_HOTKEY, 0, 0);
	return TRUE;
}

static void viewer_select_tab(const HWND hWnd, int index)
{
	HTREEITEM root;
	if (index < 0 || index > 3) {
		return;
	}
	TabCtrl_SetCurSel(GetDlgItem(hWnd, ID_CLIPMASTER_TABS), index);
	option.config_last_tab = index;
	ShowWindow(GetDlgItem(hWnd, ID_TREE), SW_HIDE);
	ShowWindow(GetDlgItem(hWnd, ID_CONTAINER), SW_HIDE);
	ShowWindow(GetDlgItem(hWnd, ID_CLIPMASTER_PASTEPLUS), index == 2 ? SW_SHOW : SW_HIDE);
	ShowWindow(GetDlgItem(hWnd, ID_CLIPMASTER_FAVORITES), index == 1 ? SW_SHOW : SW_HIDE);
	ShowWindow(GetDlgItem(hWnd, ID_CLIPMASTER_TILES), index == 0 ? SW_SHOW : SW_HIDE);
	ShowWindow(GetDlgItem(hWnd, ID_CLIPMASTER_SEARCH), index == 0 ? SW_SHOW : SW_HIDE);
	ShowWindow(GetDlgItem(hWnd, ID_CLIPMASTER_SEARCH_LABEL), index == 0 ? SW_SHOW : SW_HIDE);
	ShowWindow(GetDlgItem(hWnd, ID_CLIPMASTER_FIND), index == 0 ? SW_SHOW : SW_HIDE);
	ShowWindow(GetDlgItem(hWnd, ID_CLIPMASTER_PANEL), index == 3 ? SW_SHOW : SW_HIDE);
	if (index != 3) {
		root = index == 0 ? history_treeitem : index == 1 ? regist_treeitem : clip_treeitem;
		TreeView_SelectItem(GetDlgItem(hWnd, ID_TREE), root);
		if (index == 0) {
			tileview_refresh(GetDlgItem(hWnd, ID_CLIPMASTER_TILES), history_data.child);
		} else if (index == 1) {
			tileview_refresh(GetDlgItem(hWnd, ID_CLIPMASTER_FAVORITES), regist_data.child);
		} else if (index == 2) {
			pasteplus_refresh(GetDlgItem(hWnd, ID_CLIPMASTER_PASTEPLUS), history_data.child);
		}
		SetFocus(GetDlgItem(hWnd, index == 0 ? ID_CLIPMASTER_SEARCH : index == 1 ? ID_CLIPMASTER_FAVORITES : ID_CLIPMASTER_PASTEPLUS));
	} else {
		SetFocus(GetDlgItem(GetDlgItem(hWnd, ID_CLIPMASTER_PANEL), ID_CLIPMASTER_FIELD));
	}
	viewer_set_controls(hWnd);
}

static void viewer_draw_tab(const DRAWITEMSTRUCT *draw)
{
	const COLORREF colors[] = { RGB(219, 235, 252), RGB(219, 242, 226), RGB(255, 234, 210), RGB(250, 222, 230) };
	const COLORREF accents[] = { RGB(42, 105, 176), RGB(38, 128, 76), RGB(183, 105, 28), RGB(170, 66, 98) };
	RECT bounds = draw->rcItem, marker = bounds;
	TCHAR text[64];
	TCITEM item;
	HBRUSH brush;
	int saved = SaveDC(draw->hDC);
	BOOL selected = TabCtrl_GetCurSel(draw->hwndItem) == (int)draw->itemID;
	ZeroMemory(&item, sizeof(item));
	item.mask = TCIF_TEXT;
	item.pszText = text;
	item.cchTextMax = ARRAYSIZE(text);
	text[0] = TEXT('\0');
	TabCtrl_GetItem(draw->hwndItem, draw->itemID, &item);
	brush = CreateSolidBrush(colors[draw->itemID]);
	FillRect(draw->hDC, &bounds, brush);
	DeleteObject(brush);
	marker.top = marker.bottom - max(1, MulDiv(selected ? 3 : 1, GetDpiForWindow(draw->hwndItem), 96));
	brush = CreateSolidBrush(accents[draw->itemID]);
	FillRect(draw->hDC, &marker, brush);
	DeleteObject(brush);
	bounds.bottom = marker.top;
	SelectObject(draw->hDC, (HFONT)SendMessage(draw->hwndItem, WM_GETFONT, 0, 0));
	SetBkMode(draw->hDC, TRANSPARENT);
	SetTextColor(draw->hDC, selected ? accents[draw->itemID] : RGB(40, 40, 40));
	DrawText(draw->hDC, text, -1, &bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	if (selected && GetFocus() == draw->hwndItem) {
		InflateRect(&bounds, -3, -3);
		DrawFocusRect(draw->hDC, &bounds);
	}
	RestoreDC(draw->hDC, saved);
}

static BOOL viewer_initialize(const HWND hWnd)
{
#define CF_CNT				1
	HIMAGELIST icon_list;
	HWND hTreeView;
	HWND tabs;
	TCITEM tab_item;
	const TCHAR *tab_names[] = { TEXT("ClipManager"), TEXT("FavoriteClips"), TEXT("PastePlus"), TEXT("Setting") };
	TCHAR buf[BUF_SIZE];
	TCHAR *str_menu;
	TCHAR *str_hkey;
	UINT cf[CF_CNT];
	int i;

	tabs = CreateWindowEx(0, WC_TABCONTROL, TEXT(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | TCS_OWNERDRAWFIXED,
		0, 0, 0, 0, hWnd, (HMENU)ID_CLIPMASTER_TABS, hInst, NULL);
	SendMessage(tabs, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
	ZeroMemory(&tab_item, sizeof(tab_item));
	tab_item.mask = TCIF_TEXT;
	for (i = 0; i < 4; i++) {
		tab_item.pszText = (TCHAR *)tab_names[i];
		TabCtrl_InsertItem(tabs, i, &tab_item);
	}
	viewer_create_settings(hWnd);
	tileview_create(hInst, hWnd, ID_CLIPMASTER_TILES);
	tileview_create(hInst, hWnd, ID_CLIPMASTER_FAVORITES);
	tileview_favorites(GetDlgItem(hWnd, ID_CLIPMASTER_FAVORITES), TRUE);
	pasteplus_create(hInst, hWnd, ID_CLIPMASTER_PASTEPLUS);
	viewer_setting_control(hWnd, TEXT("STATIC"), TEXT("clip search"), SS_LEFT | SS_CENTERIMAGE,
		ID_CLIPMASTER_SEARCH_LABEL, 0, 0, 1, 1);
	viewer_setting_control(hWnd, TEXT("EDIT"), TEXT(""), ES_AUTOHSCROLL | WS_TABSTOP,
		ID_CLIPMASTER_SEARCH, 0, 0, 1, 1);
	SendDlgItemMessage(hWnd, ID_CLIPMASTER_SEARCH, EM_SETLIMITTEXT, 255, 0);
	SetWindowSubclass(GetDlgItem(hWnd, ID_CLIPMASTER_SEARCH), viewer_search_proc, 1, 0);
	viewer_setting_control(hWnd, TEXT("BUTTON"), TEXT("\x2192"), BS_PUSHBUTTON | WS_TABSTOP,
		ID_CLIPMASTER_FIND, 0, 0, 1, 1);
	{
		HWND tooltip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hWnd, NULL, hInst, NULL);
		TOOLINFO tool;
		ZeroMemory(&tool, sizeof(tool));
		tool.cbSize = sizeof(tool);
		tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
		tool.hwnd = hWnd;
		tool.uId = (UINT_PTR)GetDlgItem(hWnd, ID_CLIPMASTER_FIND);
		tool.lpszText = TEXT("Search");
		SendMessage(tooltip, TTM_ADDTOOL, 0, (LPARAM)&tool);
	}

	// コントロールの作成
	// ツールバー
	toolbar_create(hWnd, ID_TOOLBAR);
	// イメージリスト
	icon_list = create_imagelist(hInst);
	// ツリービュー
	hTreeView = treeview_create(hInst, hWnd, ID_TREE, icon_list);
	// 形式毎の情報を表示するコンテナ
	container_create(hInst, hWnd, ID_CONTAINER);
	// リストビュー
	listview_create(hInst, GetDlgItem(hWnd, ID_CONTAINER), ID_LIST, icon_list);
	// バイナリ表示ウィンドウ
	binview_create(hInst, GetDlgItem(hWnd, ID_CONTAINER), ID_BINVIEW);
	// 形式毎のウィンドウ
	format_window_create(GetDlgItem(hWnd, ID_CONTAINER));
	SendMessage(GetDlgItem(hWnd, ID_CONTAINER), WM_ALLHIDE, 0, 0);
	// ステータスバー
	statusbar_create(hWnd, ID_STATUSBAR);

	// クリップボードのアイテム取得
	clip_di.type = TYPE_ITEM;
	clip_di.child = clipboard_to_datainfo(hWnd);
	// 初期アイテム設定
	treeview_set_init_item(hTreeView);

	// ウィンドウメニュー
	CheckMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_FILE), ID_MENUITEM_WATCH,
		((option.main_clipboard_watch == 0) ? MF_UNCHECKED : MF_CHECKED));
	CheckMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_VIEW), ID_MENUITEM_SHOW_TOOLBAR,
		((option.viewer_show_toolbar == 0) ? MF_UNCHECKED : MF_CHECKED));
	CheckMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_VIEW), ID_MENUITEM_SHOW_STATUSBAR,
		((option.viewer_show_statusbar == 0) ? MF_UNCHECKED : MF_CHECKED));
	CheckMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_VIEW), ID_MENUITEM_SHOW_FORMAT,
		((option.tree_show_format == 0) ? MF_UNCHECKED : MF_CHECKED));
	CheckMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_VIEW), ID_MENUITEM_SHOW_BIN,
		((option.viewer_show_bin == 0) ? MF_UNCHECKED : MF_CHECKED));

	// ポップアップメニュー
	h_popup_menu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENU_POPUP));
	// ポップアップメニューにツールメニューを関連付ける
	GetMenuString(GetMenu(hWnd), WINDOW_MENU_TOOL, buf, BUF_SIZE - 1, MF_BYPOSITION);
	ModifyMenu(GetSubMenu(h_popup_menu, POPUPMENU_HISTORY), ID_MENUITEM_TOOL_MENU, MF_POPUP,
		(UINT)GetSubMenu(GetMenu(hWnd), WINDOW_MENU_TOOL), buf);
	ModifyMenu(GetSubMenu(h_popup_menu, POPUPMENU_HISTORY_LV), ID_MENUITEM_TOOL_MENU, MF_POPUP,
		(UINT)GetSubMenu(GetMenu(hWnd), WINDOW_MENU_TOOL), buf);
	ModifyMenu(GetSubMenu(h_popup_menu, POPUPMENU_REGIST), ID_MENUITEM_TOOL_MENU, MF_POPUP,
		(UINT)GetSubMenu(GetMenu(hWnd), WINDOW_MENU_TOOL), buf);
	ModifyMenu(GetSubMenu(h_popup_menu, POPUPMENU_REGIST_LV), ID_MENUITEM_TOOL_MENU, MF_POPUP,
		(UINT)GetSubMenu(GetMenu(hWnd), WINDOW_MENU_TOOL), buf);
	// ツールメニューにツールを設定
	for (i = 0; i < option.tool_cnt; i++) {
		if (!((option.tool_info + i)->call_type & CALLTYPE_VIEWER)) {
			continue;
		}
		if (lstrcmp((option.tool_info + i)->title, TEXT("-")) == 0) {
			AppendMenu(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_TOOL), MF_SEPARATOR, 0, NULL);
		} else {
			str_hkey = menu_get_keyname((option.tool_info + i)->modifiers, (option.tool_info + i)->virtkey);
			if (str_hkey == NULL) {
				AppendMenu(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_TOOL), MF_STRING, ID_MENUITEM_TOOL + i,
					(option.tool_info + i)->title);
			} else {
				str_menu = mem_alloc(sizeof(TCHAR) * (lstrlen((option.tool_info + i)->title) + 1 + lstrlen(str_hkey) + 1));
				if (str_menu != NULL) {
					wsprintf(str_menu, TEXT("%s\t%s"), (option.tool_info + i)->title, str_hkey);
					AppendMenu(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_TOOL), MF_STRING, ID_MENUITEM_TOOL + i, str_menu);
					mem_free(&str_menu);
				}
				mem_free(&str_hkey);
			}
		}
	}
	if (GetMenuItemCount(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_TOOL)) == 2) {
		AppendMenu(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_TOOL), MF_STRING | MF_GRAYED,
			ID_MENUITEM_TOOL, message_get_res(IDS_VIEWER_MENU_TOOL_NOTHING));
	}

	// ドロップターゲットに設定
	cf[0] = CF_TEXT;
	OLE_IDropTarget_RegisterDragDrop(hWnd, WM_DRAGDROP, cf, CF_CNT);

	// ビューアを開いた時に実行するツール
	tool_execute_all(hWnd, CALLTYPE_VIEWER_OPEN, NULL);
	return TRUE;
}

/*
 * viewer_set_controls - コントロールの位置、サイズを設定する
 */
static void viewer_set_controls(const HWND hWnd)
{
	RECT window_rect;
	RECT toolbar_rect;
	DWORD toolbar_size = 0;
	RECT statusbar_rect;
	DWORD statusbar_size = 0;
	int sep_size;

	// 他のウィンドウの表示でDPIが変わっている場合があるため設定し直す
	SetDpiFromWindow(hWnd);

	GetClientRect(hWnd, (LPRECT)&window_rect);

	// ToolBarのサイズの取得
	if (IsWindowVisible(GetDlgItem(hWnd, ID_TOOLBAR)) != 0) {
		GetWindowRect(GetDlgItem(hWnd, ID_TOOLBAR), (LPRECT)&toolbar_rect);
		toolbar_size = (toolbar_rect.bottom - toolbar_rect.top);
	}
	// StatusBarのサイズの取得
	if (IsWindowVisible(GetDlgItem(hWnd, ID_STATUSBAR)) != 0) {
		GetWindowRect(GetDlgItem(hWnd, ID_STATUSBAR), (LPRECT)&statusbar_rect);
		statusbar_size = (statusbar_rect.bottom - statusbar_rect.top);
	}

	sep_size = Scale(option.viewer_sep_size);
	MoveWindow(GetDlgItem(hWnd, ID_CLIPMASTER_TABS), 0, toolbar_size,
		window_rect.right, Scale(30), TRUE);
	toolbar_size += Scale(34);
	MoveWindow(GetDlgItem(hWnd, ID_CLIPMASTER_PANEL), 0, toolbar_size,
		window_rect.right, max(0, (int)(window_rect.bottom - statusbar_size - toolbar_size)), TRUE);
	MoveWindow(GetDlgItem(hWnd, ID_CLIPMASTER_FAVORITES), 0, toolbar_size,
		window_rect.right, max(0, (int)(window_rect.bottom - statusbar_size - toolbar_size)), TRUE);
	MoveWindow(GetDlgItem(hWnd, ID_CLIPMASTER_PASTEPLUS), 0, toolbar_size,
		window_rect.right, max(0, (int)(window_rect.bottom - statusbar_size - toolbar_size)), TRUE);
	MoveWindow(GetDlgItem(hWnd, ID_CLIPMASTER_SEARCH_LABEL), Scale(12), toolbar_size + Scale(8),
		Scale(76), Scale(28), TRUE);
	MoveWindow(GetDlgItem(hWnd, ID_CLIPMASTER_SEARCH), Scale(96), toolbar_size + Scale(8),
		max(1, window_rect.right - Scale(148)), Scale(28), TRUE);
	MoveWindow(GetDlgItem(hWnd, ID_CLIPMASTER_FIND), max(0, window_rect.right - Scale(44)),
		toolbar_size + Scale(8), Scale(32), Scale(28), TRUE);
	MoveWindow(GetDlgItem(hWnd, ID_CLIPMASTER_TILES), 0, toolbar_size + Scale(44),
		window_rect.right, max(0, (int)(window_rect.bottom - statusbar_size - toolbar_size - Scale(44))), TRUE);

	// TreeViewの位置、サイズの設定
	MoveWindow(GetDlgItem(hWnd, ID_TREE),
		0, toolbar_size, sep_size, window_rect.bottom - statusbar_size - toolbar_size, TRUE);
	UpdateWindow(GetDlgItem(hWnd, ID_TREE));

	// Containerの位置、サイズの設定
	MoveWindow(GetDlgItem(hWnd, ID_CONTAINER), sep_size + (FRAME_CNT * 2), toolbar_size,
		window_rect.right - sep_size - (FRAME_CNT * 2), window_rect.bottom - statusbar_size - toolbar_size, TRUE);
	UpdateWindow(GetDlgItem(hWnd, ID_CONTAINER));
}

/*
 * viewer_reset_dpi - DPIに依存するコントロールを作り直す
 */
static void viewer_reset_dpi(const HWND hWnd)
{
	HWND hTreeView;
	HWND hListView;
	HIMAGELIST icon_list;
	HIMAGELIST old_list;

	hTreeView = GetDlgItem(hWnd, ID_TREE);
	hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);

	// ツールバーの作り直し
	if (GetDlgItem(hWnd, ID_TOOLBAR) != NULL) {
		DestroyWindow(GetDlgItem(hWnd, ID_TOOLBAR));
		toolbar_create(hWnd, ID_TOOLBAR);
		dark_mode_set_control(GetDlgItem(hWnd, ID_TOOLBAR));
	}
	// ステータスバーのパーツの再設定
	statusbar_reset_parts(GetDlgItem(hWnd, ID_STATUSBAR));

	// イメージリストの作り直し
	if ((icon_list = create_imagelist(hInst)) != NULL) {
		old_list = (HIMAGELIST)TreeView_SetImageList(hTreeView, icon_list, TVSIL_NORMAL);
		if (hListView != NULL) {
			ListView_SetImageList(hListView, icon_list, LVSIL_SMALL);
		}
		if (old_list != NULL) {
			ImageList_Destroy(old_list);
		}
	}
	// フォントの作り直し
	treeview_set_font(hTreeView);
	listview_set_font(hListView);

	// リストビューのカラム幅の再設定
	if (hListView != NULL) {
		ListView_SetColumnWidth(hListView, 0, Scale(option.list_column_data));
		ListView_SetColumnWidth(hListView, 1, Scale(option.list_column_size));
		ListView_SetColumnWidth(hListView, 2, Scale(option.list_column_date));
		ListView_SetColumnWidth(hListView, 3, Scale(option.list_column_window));
	}

	// 形式毎のウィンドウへDPIの変更を通知
	SendMessage(GetDlgItem(hWnd, ID_CONTAINER), WM_DPICHANGED_AFTERPARENT, 0, 0);
	SendMessage(GetDlgItem(hWnd, ID_CLIPMASTER_PANEL), WM_DPICHANGED_AFTERPARENT, 0, 0);

	// コントロールの再配置
	viewer_set_controls(hWnd);
	InvalidateRect(hWnd, NULL, TRUE);
}

/*
 * viewer_close - ウィンドウを閉じる
 */
static BOOL viewer_close(const HWND hWnd)
{
	HWND hListView;

	// サイズの強制保存
	SendMessage(hWnd, WM_EXITSIZEMOVE, 0, 0);

	// ビューアを閉じる時に実行するツール
	tool_execute_all(hWnd, CALLTYPE_VIEWER_CLOSE, NULL);

	set_cursor(TRUE);
	viewer_sel_cheange(hWnd, TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE)), NULL);

	if (option.history_save == 1 && option.history_always_save == 1) {
		// 履歴の保存
		SendMessage(hWnd, WM_HISTORY_SAVE, 0, 0);
	}
	// 登録アイテムの保存
	SendMessage(hWnd, WM_REGIST_SAVE, 0, 0);

	// フォーマット毎のウィンドウの破棄
	format_window_destroy();

	// データ解放
	data_free(clip_di.child);
	clip_di.child = NULL;

	// リストビューのカラム幅取得
	SetDpiFromWindow(hWnd);
	hListView = GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST);
	option.list_column_data = UnScale(ListView_GetColumnWidth(hListView, 0));
	option.list_column_size = UnScale(ListView_GetColumnWidth(hListView, 1));
	option.list_column_date = UnScale(ListView_GetColumnWidth(hListView, 2));
	option.list_column_window = UnScale(ListView_GetColumnWidth(hListView, 3));

	// ツリビューーの解放
	treeview_close(GetDlgItem(hWnd, ID_TREE));
	// リストビューの解放
	listview_close(hListView);
	// イメージリストの解放
	ImageList_Destroy((void *)TreeView_SetImageList(GetDlgItem(hWnd, ID_TREE), NULL, TVSIL_NORMAL));

	DestroyMenu(h_popup_menu);

	OLE_IDropTarget_RevokeDragDrop(hWnd);
	set_cursor(FALSE);
	return TRUE;
}

/*
 * viewer_proc - ウィンドウのプロシージャ
 */
static LRESULT CALLBACK viewer_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LPIDROPTARGET_NOTIFY pdtn;
	static HWND FocusWnd;
	static BOOL sep_move;
	static BOOL ctrl_flag;
	static int DnD_keystate;
	int ret;

	switch (msg) {
	case WM_DRAWITEM:
		if (wParam == ID_CLIPMASTER_TABS && ((DRAWITEMSTRUCT *)lParam)->itemID < 4) {
			viewer_draw_tab((DRAWITEMSTRUCT *)lParam);
			return TRUE;
		}
		break;
	case WM_CREATE:
		// ウィンドウ作成
		viewer_initialize(hWnd);
		// ダークモードの設定
		dark_mode_set_window(hWnd);
		viewer_set_controls(hWnd);

		FocusWnd = GetDlgItem(hWnd, ID_TREE);
		SetFocus(FocusWnd);
		viewer_select_tab(hWnd, option.config_remember_layout && option.config_last_tab >= 0 &&
			option.config_last_tab <= 3 ? option.config_last_tab : 0);
		break;

	case WM_UAHDRAWMENU:
	case WM_UAHDRAWMENUITEM:
	case WM_NCPAINT:
	case WM_NCACTIVATE:
		// メニューバーの描画
		{
			LRESULT dark_ret;

			if (dark_mode_menubar_message(hWnd, msg, wParam, lParam, &dark_ret) == TRUE) {
				return dark_ret;
			}
		}
		return DefWindowProc(hWnd, msg, wParam, lParam);

	case WM_SETTINGCHANGE:
	case WM_THEMECHANGED:
		// 配色の変更
		if (dark_mode_is_color_change(msg, lParam) == TRUE) {
			dark_mode_update();
			dark_mode_refresh_window(hWnd);
		}
		return DefWindowProc(hWnd, msg, wParam, lParam);

	case WM_ERASEBKGND:
		// 背景の描画
		if (dark_mode_is_dark() == TRUE) {
			RECT erase_rect;

			GetClientRect(hWnd, &erase_rect);
			FillRect((HDC)wParam, &erase_rect, dark_mode_get_brush(COLOR_BTNFACE));
			return TRUE;
		}
		return DefWindowProc(hWnd, msg, wParam, lParam);

	case WM_CLOSE:
		// ウィンドウを閉じる
		viewer_close(hWnd);

		SendMessage(main_wnd, WM_VIEWER_NOTIFY_CLOSE, 0, 0);
		DestroyWindow(hWnd);
		break;

	case WM_ACTIVATE:
		if (LOWORD(wParam) != WA_INACTIVE && lParam != 0) {
			viewer_set_paste_target((HWND)lParam);
		}
		if (LOWORD(wParam) == WA_INACTIVE) {
			set_cursor(TRUE);
			viewer_save_data(hWnd, TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE)));
			set_cursor(FALSE);
			if (IsIconic(hWnd) == 0 && GetFocus() != NULL) {
				FocusWnd = GetFocus();
			}
			if (option.config_hide_tray_on_switch != 0 && lParam != 0) {
				DWORD process = 0;
				GetWindowThreadProcessId((HWND)lParam, &process);
				if (process != GetCurrentProcessId()) ShowWindow(hWnd, SW_HIDE);
			}
		}
		break;

	case WM_SETFOCUS:
		SetFocus(FocusWnd);
		break;

	case WM_LBUTTONDOWN:
		// 境界の移動
		if (GetForegroundWindow() != hWnd) {
			break;
		}
		if (sep_move == FALSE) {
			if (frame_initialize(hWnd) == FALSE) {
				break;
			}
			sep_move = TRUE;
			frame_draw(hWnd, GetDlgItem(hWnd, ID_TREE));
			SetTimer(hWnd, TIMER_SEP, 1, NULL);
		}
		break;

	case WM_MOUSEMOVE:
		if (sep_move == TRUE) {
			frame_draw(hWnd, GetDlgItem(hWnd, ID_TREE));
		}
		if (DnD_mode == TRUE) {
			// ドラッグ中
			ret = dragdrop_set_drag_item(hWnd);
			if (ret == DRAG_MODE_NONE) {
				SetCursor(LoadCursor(hInst, MAKEINTRESOURCE(IDC_CURSOR_BAN)));
			} else if (ret & DRAG_MODE_MOVE) {
				SetCursor(LoadCursor(hInst, MAKEINTRESOURCE(IDC_CURSOR_MOVE)));
			} else {
				SetCursor(LoadCursor(hInst, MAKEINTRESOURCE(IDC_CURSOR_COPY)));
			}
		}
		break;

	case WM_LBUTTONUP:
		if (sep_move == TRUE) {
			KillTimer(hWnd, TIMER_SEP);
			sep_move = FALSE;
			if ((ret = frame_draw_end(hWnd)) == -1) {
				break;
			}
			SetDpiFromWindow(hWnd);
			option.viewer_sep_size = UnScale(ret);
			set_cursor(TRUE);
			viewer_set_controls(hWnd);
			set_cursor(FALSE);
		}
	case WM_RBUTTONUP:
		if (DnD_mode == TRUE) {
			// ドロップ
			KillTimer(hWnd, TIMER_DRAG);
			ReleaseCapture();
			DnD_mode = FALSE;
			TreeView_Select(GetDlgItem(hWnd, ID_TREE), NULL, TVGN_DROPHILITE);
			ListView_SetItemState(GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST), -1, 0, LVIS_DROPHILITED);

			// ホットキーの解除
			SendMessage(hWnd, WM_UNREGIST_HOTKEY, 0, 0);
			// ドロップ処理
			dragdrop_drop_item(hWnd, msg);
			// ホットキーの設定
			SendMessage(hWnd, WM_REGIST_HOTKEY, 0, 0);
		}
		break;

	case WM_SIZE:
		// サイズ変更
		SendMessage(GetDlgItem(hWnd, ID_TOOLBAR), WM_SIZE, wParam, lParam);
		SendMessage(GetDlgItem(hWnd, ID_STATUSBAR), WM_SIZE, wParam, lParam);
		viewer_set_controls(hWnd);
		break;

	case WM_DPICHANGED:
		// DPIの変更
		SetDpi(HIWORD(wParam));
		if (lParam != 0) {
			// ウィンドウサイズの変更
			RECT *new_rect;

			new_rect = (RECT *)lParam;
			SetWindowPos(hWnd, NULL,
				new_rect->left, new_rect->top,
				new_rect->right - new_rect->left, new_rect->bottom - new_rect->top,
				SWP_NOZORDER | SWP_NOACTIVATE);
		}
		// コントロールの作り直し
		viewer_reset_dpi(hWnd);
		break;

	case WM_EXITSIZEMOVE:
		// サイズ変更完了
		if (option.config_remember_layout && IsWindowVisible(hWnd) != 0 && IsIconic(hWnd) == 0 && IsZoomed(hWnd) == 0) {
			option.config_saved_geometry = 1;
			SetDpiFromWindow(hWnd);
			GetWindowRect(hWnd, (LPRECT)&option.viewer_rect);
				option.viewer_rect.right = UnScale(option.viewer_rect.right - option.viewer_rect.left);
			option.viewer_rect.bottom = UnScale(option.viewer_rect.bottom - option.viewer_rect.top);
		}
		break;

	case WM_TIMER:
		switch (wParam) {
		// 境界の移動
		case TIMER_SEP:
			if (hWnd != GetForegroundWindow() || GetAsyncKeyState(VK_ESCAPE) < 0 ||
				GetAsyncKeyState(VK_RBUTTON) < 0) {
				KillTimer(hWnd, wParam);
				frame_free();
				sep_move = FALSE;
				InvalidateRect(hWnd, NULL, FALSE);
				UpdateWindow(hWnd);
			}
			break;

		// D&D
		case TIMER_DRAG:
			if (hWnd != GetForegroundWindow() || GetKeyState(VK_ESCAPE) < 0) {
				KillTimer(hWnd, wParam);
				ReleaseCapture();
				DnD_mode = FALSE;
				TreeView_Select(GetDlgItem(hWnd, ID_TREE), NULL, TVGN_DROPHILITE);
				ListView_SetItemState(GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST), -1, 0, LVIS_DROPHILITED);
			} else if (ctrl_flag == FALSE && GetKeyState(VK_CONTROL) < 0) {
				SendMessage(hWnd, WM_MOUSEMOVE, 0, 0);
				ctrl_flag = TRUE;
			} else if (ctrl_flag == TRUE && GetKeyState(VK_CONTROL) >= 0) {
				SendMessage(hWnd, WM_MOUSEMOVE, 0, 0);
				ctrl_flag = FALSE;
			}
			break;

		case TIMER_SET_MENU:
			KillTimer(hWnd, wParam);
			set_enable_window_menu(hWnd);
			break;

		case TIMER_REFLECT_DATA:
			KillTimer(hWnd, wParam);
			set_cursor(TRUE);
			viewer_save_data(hWnd, TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE)));
			set_cursor(FALSE);
			break;

		case TIMER_LV_REFRESH:
			KillTimer(hWnd, wParam);
			set_cursor(TRUE);
			treeview_to_listview(GetDlgItem(hWnd, ID_TREE),
				TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE)),
				GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST));
			set_cursor(FALSE);
			break;

		case TIMER_TV_ACTION:
			KillTimer(hWnd, wParam);
			if (option.list_default_action == 2) {
				// 名前を付けて保存
				viewer_data_save(hWnd, TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE)));
			} else {
				// クリップボードに送る
				SendMessage(hWnd, WM_COMMAND, ID_MENUITEM_CLIPBOARD, 0);
			}
			break;
		}
		break;

	case WM_VIEWER_SET_TARGET:
		viewer_set_paste_target((HWND)lParam);
		break;

	case WM_VIEWER_PREPARE_HISTORY:
		TreeView_SelectItem(GetDlgItem(hWnd, ID_TREE), history_treeitem);
		break;

	case WM_TILE_SEARCH:
		{
			TCHAR keyword[256];
			GetDlgItemText(hWnd, ID_CLIPMASTER_SEARCH, keyword, ARRAYSIZE(keyword));
			tileview_search(GetDlgItem(hWnd, ID_CLIPMASTER_TILES), keyword);
		}
		break;

	case WM_PASTEPLUS_PASTE:
		if (lParam != 0) viewer_paste_clip(hWnd, (DATA_INFO *)lParam, TRUE);
		break;

	case WM_TILE_NEW:
	case WM_TILE_EDIT:
	case WM_TILE_DELETE:
		{
			DATA_INFO *original = (DATA_INFO *)lParam;
			DATA_INFO *replacement;
			DATA_INFO *entry;
			int count = 0;
			for (entry = regist_data.child; entry != NULL; entry = entry->next) {
				if (entry->type == TYPE_ITEM) count++;
			}
			if (msg == WM_TILE_NEW && count >= option.config_favorite_max) {
				MessageBox(hWnd, TEXT("The favorites limit has been reached."), WINDOW_TITLE, MB_OK | MB_ICONINFORMATION);
				break;
			}
			if (msg != WM_TILE_NEW && treeview_lparam_to_item(GetDlgItem(hWnd, ID_TREE), regist_treeitem, lParam) == NULL) break;
			if (msg == WM_TILE_DELETE) {
				viewer_commit_favorite(hWnd, original, NULL);
			} else {
				replacement = clipedit_show(hInst, hWnd, original, (int)wParam);
				if (replacement != NULL) viewer_commit_favorite(hWnd, original, replacement);
			}
		}
		break;

	case WM_TILE_PASTE:
	case WM_TILE_FAVORITE:
		{
			HTREEITEM item = treeview_lparam_to_item(GetDlgItem(hWnd, ID_TREE), history_treeitem, lParam);
			TCHAR error[BUF_SIZE] = TEXT("");
			if (item == NULL && msg == WM_TILE_PASTE)
				item = treeview_lparam_to_item(GetDlgItem(hWnd, ID_TREE), regist_treeitem, lParam);
			if (item == NULL) {
				break;
			}
			if (msg == WM_TILE_FAVORITE) {
				DATA_INFO *favorite;
				int count = 0;
				for (favorite = regist_data.child; favorite != NULL; favorite = favorite->next) {
					if (favorite->type == TYPE_ITEM) count++;
				}
				if (count >= option.config_favorite_max) {
					MessageBox(hWnd, TEXT("The favorites limit has been reached."), WINDOW_TITLE, MB_OK | MB_ICONINFORMATION);
				} else if (regist_treeitem != NULL && MessageBox(hWnd, TEXT("Add this clip to favorites?"),
					WINDOW_TITLE, MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
					favorite = data_item_copy((DATA_INFO *)lParam, FALSE, FALSE, error);
					if (favorite != NULL) {
						mem_free(&favorite->window_name);
						viewer_commit_favorite(hWnd, NULL, favorite);
					} else if (*error) {
						MessageBox(hWnd, error, ERROR_TITLE, MB_OK | MB_ICONERROR);
					}
				}
			} else viewer_paste_clip(hWnd, (DATA_INFO *)lParam, FALSE);
		}
		break;

	case WM_VIEWER_SHOW_FAVORITES:
		viewer_select_tab(hWnd, 1);
		break;

	case WM_VIEWER_SELECT_TAB:
		viewer_select_tab(hWnd, (int)wParam);
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case 5480:
		case 5481:
			settings_handler_count = min(16, max(3, settings_handler_count + (LOWORD(wParam) == 5480 ? 1 : -1)));
			SendMessage(GetDlgItem(hWnd, ID_CLIPMASTER_PANEL), WM_SIZE, 0, 0);
			break;
		case 5482:
			PostMessage(main_wnd, WM_CONFIG_RESET, 0, 0);
			break;
		case 5483:
			if (viewer_save_settings(hWnd)) PostMessage(main_wnd, WM_CONFIG_RESTART, 0, 0);
			break;
		case 5484:
			if (viewer_save_settings(hWnd)) {
				SendMessage(hWnd, WM_VIEWER_PREPARE_HISTORY, 0, 0);
				if (SendMessage(main_wnd, WM_HISTORY_CLEAR, 0, 0)) PostMessage(main_wnd, WM_CONFIG_RESTART, 0, 0);
			}
			break;
		case 5485:
			PostMessage(main_wnd, WM_CLOSE, 0, 0);
			break;
		case ID_CLIPMASTER_FIND:
			SendMessage(hWnd, WM_TILE_SEARCH, 0, 0);
			break;
		case ID_CLIPMASTER_SEARCH:
			if (HIWORD(wParam) == EN_CHANGE && GetWindowTextLength(GetDlgItem(hWnd, ID_CLIPMASTER_SEARCH)) == 0)
				tileview_search(GetDlgItem(hWnd, ID_CLIPMASTER_TILES), TEXT(""));
			break;
		case ID_CLIPMASTER_SAVE:
			if (viewer_save_settings(hWnd)) {
				SetDlgItemText(GetDlgItem(hWnd, ID_CLIPMASTER_PANEL), ID_CLIPMASTER_STATUS, TEXT("설정을 저장했습니다."));
			}
			break;
		case ID_CLIPMASTER_SETTINGS:
			viewer_select_tab(hWnd, 3);
			break;

		case ID_ACCEL_CTRL_TAB:
			// フォーカスの移動
			if (GetFocus() != GetDlgItem(hWnd, ID_TREE)) {
				SetFocus(GetDlgItem(hWnd, ID_TREE));
			} else {
				SetFocus(GetDlgItem(hWnd, ID_CONTAINER));
			}
			break;

		case ID_MENUITEM_SAVE:
			// 現在の状態を保存
			set_cursor(TRUE);
			viewer_save_data(hWnd, TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE)));
			SendMessage(hWnd, WM_REGIST_SAVE, 0, 0);
			SendMessage(hWnd, WM_HISTORY_SAVE, 0, 0);
			SendMessage(hWnd, WM_OPTION_SAVE, 0, 0);
			set_cursor(FALSE);
			break;

		case ID_MENUITEM_IMPORT:
			// インポート
			if (regist_treeitem == NULL) {
				break;
			}
			viewer_import_item(hWnd);
			break;

		case ID_MENUITEM_EXPORT:
			// エクスポート
			if (regist_treeitem == NULL) {
				break;
			}
			viewer_export_item(hWnd);
			break;

		case ID_MENUITEM_WATCH:
			// クリップボードの監視切り替え
			SendMessage(hWnd, WM_SET_CLIPBOARD_WATCH, !option.main_clipboard_watch, 0);
			break;

		case ID_MENUITEM_CLOSE:
			SendMessage(hWnd, WM_CLOSE, 0, 0);
			break;

		case ID_MENUITEM_EXIT:
			SendMessage(main_wnd, WM_CLOSE, 0, 0);
			break;

		case ID_MENUITEM_SELECT_ALL:
			// すべて選択
			if (current_wnd != GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST)) {
				break;
			}
			SetFocus(current_wnd);
			ListView_SetItemState(current_wnd, -1, LVIS_SELECTED, LVIS_SELECTED);
			break;

		case ID_MENUITEM_SHOW_TOOLBAR:
			// ツールバー表示切替
			if (option.viewer_show_toolbar == 0) {
				option.viewer_show_toolbar = 1;
				CheckMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_VIEW), LOWORD(wParam), MF_CHECKED);
				ShowWindow(GetDlgItem(hWnd, ID_TOOLBAR), SW_SHOW);
			} else {
				option.viewer_show_toolbar = 0;
				CheckMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_VIEW), LOWORD(wParam), MF_UNCHECKED);
				ShowWindow(GetDlgItem(hWnd, ID_TOOLBAR), SW_HIDE);
			}
			viewer_set_controls(hWnd);
			break;

		case ID_MENUITEM_SHOW_STATUSBAR:
			// ステータスバー表示切替
			if (option.viewer_show_statusbar == 0) {
				option.viewer_show_statusbar = 1;
				CheckMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_VIEW), LOWORD(wParam), MF_CHECKED);
				ShowWindow(GetDlgItem(hWnd, ID_STATUSBAR), SW_SHOW);
			} else {
				option.viewer_show_statusbar = 0;
				CheckMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_VIEW), LOWORD(wParam), MF_UNCHECKED);
				ShowWindow(GetDlgItem(hWnd, ID_STATUSBAR), SW_HIDE);
			}
			viewer_set_controls(hWnd);
			break;

		case ID_MENUITEM_SHOW_FORMAT:
			// 形式表示
			if (option.tree_show_format == 0) {
				option.tree_show_format = 1;
				CheckMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_VIEW), LOWORD(wParam), MF_CHECKED);
			} else {
				option.tree_show_format = 0;
				CheckMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_VIEW), LOWORD(wParam), MF_UNCHECKED);
			}
			set_cursor(TRUE);
			SendMessage(hWnd, WM_SETREDRAW, (WPARAM)FALSE, 0);

			viewer_sel_cheange(hWnd, TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE)), NULL);
			treeview_set_init_item(GetDlgItem(hWnd, ID_TREE));

			SendMessage(hWnd, WM_SETREDRAW, (WPARAM)TRUE, 0);
			InvalidateRect(hWnd, NULL, FALSE);
			UpdateWindow(hWnd);
			set_cursor(FALSE);
			break;

		case ID_MENUITEM_SHOW_BIN:
			// バイナリ表示
			if (option.viewer_show_bin == 0) {
				option.viewer_show_bin = 1;
				CheckMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_VIEW), LOWORD(wParam), MF_CHECKED);
			} else {
				option.viewer_show_bin = 0;
				CheckMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_VIEW), LOWORD(wParam), MF_UNCHECKED);
			}
			set_cursor(TRUE);
			viewer_sel_cheange(hWnd, TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE)),
				TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE)));
			set_cursor(FALSE);
			break;

		case ID_MENUITEM_OPTION:
			// オプション
			SendMessage(hWnd, WM_OPTION_SHOW, 0, 0);
			break;

		case ID_MENUITEM_TOOL_SET:
			// ツール設定
			SendMessage(hWnd, WM_OPTION_SHOW, 0, (LPARAM)OPTION_SHOW_TOOL);
			break;

		case ID_MENUITEM_ABOUT:
			// バージョン情報
		{
			TCHAR var_msg[BUF_SIZE];
			TCHAR path[MAX_PATH];
			DWORD size;
			lstrcpy(var_msg, APP_NAME);
			GetModuleFileName(NULL, path, sizeof(path));
			size = GetFileVersionInfoSize(path, NULL);
			if (size) {
				VS_FIXEDFILEINFO* FileInfo;
				UINT len;
				BYTE* buf = mem_alloc(size);
				if (buf != NULL) {
					GetFileVersionInfo(path, 0, size, buf);
					VerQueryValue(buf, TEXT("\\"), &FileInfo, &len);
					wsprintf(var_msg + lstrlen(var_msg), TEXT(" Ver %d.%d.%d"),
						HIWORD(FileInfo->dwFileVersionMS),
						LOWORD(FileInfo->dwFileVersionMS),
						HIWORD(FileInfo->dwFileVersionLS));
					mem_free(&buf);
				}
			}
			lstrcat(var_msg, TEXT("\nCopyright (C) 1996-2026 by Ohno Tomoaki. All rights reserved.\n\n")
				TEXT("WEB SITE: https://www.nakka.com/\nE-MAIL: nakka@nakka.com"));
			MessageBox(hWnd, var_msg, TEXT("About"), MB_OK | MB_ICONINFORMATION);
		}
			break;

		case ID_MENUITEM_OPEN:
			// 表示
			viewer_show_item(hWnd);
			break;

		case ID_MENUITEM_CLIPBOARD_TB:
			lParam = 0;
		case ID_MENUITEM_CLIPBOARD:
			// クリップボードに送る
			treeview_to_clipboard(hWnd, (HTREEITEM)lParam);
			break;

		case ID_MENUITEM_PASTE_TB:
			lParam = 0;
		case ID_MENUITEM_PASTE:
			// クリップボードから貼り付け
			viewer_item_paste(hWnd, (HTREEITEM)lParam);
			break;

		case ID_MENUITEM_REGIST_ADD_TB:
			lParam = 0;
		case ID_MENUITEM_REGIST_ADD:
			// 登録アイテムに追加
			if (regist_treeitem == NULL) {
				break;
			}
			viewer_regist_add(hWnd, (HTREEITEM)lParam, FALSE);
			break;

		case ID_MENUITEM_REGIST_MOVE:
			// 登録アイテムの移動
			if (regist_treeitem == NULL) {
				break;
			}
			viewer_regist_add(hWnd, (HTREEITEM)lParam, TRUE);
			break;

		case ID_MENUITEM_UP_TB:
			lParam = 0;
		case ID_MENUITEM_UP:
			// 上へ
			viewer_move_up(hWnd, (HTREEITEM)lParam);
			break;

		case ID_MENUITEM_DOWN_TB:
			lParam = 0;
		case ID_MENUITEM_DOWN:
			// 下へ
			viewer_move_down(hWnd, (HTREEITEM)lParam);
			break;

		case ID_MENUITEM_NEW_ITEM_TB:
			lParam = 0;
		case ID_MENUITEM_NEW_ITEM:
			// 新規作成
			viewer_create_item(hWnd, (HTREEITEM)lParam);
			break;

		case ID_MENUITEM_CREATE_FOLDER:
			// フォルダの作成
			viewer_create_folder(hWnd, (HTREEITEM)lParam);
			break;

		case ID_MENUITEM_DATA_SAVE:
			// 名前を付けて保存
			viewer_data_save(hWnd, (HTREEITEM)lParam);
			break;

		case ID_MENUITEM_RENAME:
			// 名前の変更
			if (GetFocus() == GetDlgItem(hWnd, ID_TREE)) {
				TreeView_EditLabel(GetDlgItem(hWnd, ID_TREE),
					((lParam != 0) ? (HTREEITEM)lParam : TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE))));
			} else {
				ListView_SetItemState(GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST), -1, 0, LVIS_SELECTED);
				ListView_SetItemState(GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST),
					ListView_GetNextItem(GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST), -1, LVNI_FOCUSED),
					LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
				ListView_EditLabel(GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST),
					ListView_GetNextItem(GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST), -1, LVNI_FOCUSED));
			}
			break;

		case ID_MENUITEM_CLEAR_NAME:
			// 名前のクリア
			viewer_clear_name(hWnd, (HTREEITEM)lParam);
			break;

		case ID_MENUITEM_SET_HOTKEY:
			// ホットキーの設定
			SendMessage(hWnd, WM_UNREGIST_HOTKEY, 0, 0);
			if (viewer_set_hotkey(hWnd, (HTREEITEM)lParam) == TRUE) {
				SendMessage(hWnd, WM_VIEWER_REFRESH_STATUS, 0, 0);
			}
			SendMessage(hWnd, WM_REGIST_HOTKEY, 0, 0);
			break;

		case ID_MENUITEM_DELETE_TB:
			lParam = 0;
		case ID_MENUITEM_DELETE:
			// 削除
			viewer_delete_item(hWnd, (HTREEITEM)lParam);
			SendMessage(hWnd, WM_VIEWER_REFRESH_STATUS, 0, 0);
			break;

		case ID_MENUITEM_SET_FORMAT:
			viewer_format_to_option(hWnd, (HTREEITEM)lParam, OPTION_SHOW_FORMAT);
			break;

		case ID_MENUITEM_SET_FILTER:
			viewer_format_to_option(hWnd, (HTREEITEM)lParam, OPTION_SHOW_FILTER);
			break;

		default:
			if (LOWORD(wParam) >= ID_MENUITEM_TOOL && LOWORD(wParam) < ID_MENUITEM_TOOL + 1000) {
				viewer_tool_execute(hWnd, (HTREEITEM)lParam, LOWORD(wParam) - ID_MENUITEM_TOOL);
			}
			break;
		}
		break;

	case WM_NOTIFY:
		if (((NMHDR *)lParam)->hwndFrom == GetDlgItem(hWnd, ID_CLIPMASTER_TABS) &&
			((NMHDR *)lParam)->code == TCN_SELCHANGE) {
			viewer_select_tab(hWnd, TabCtrl_GetCurSel(GetDlgItem(hWnd, ID_CLIPMASTER_TABS)));
			return 0;
		}
		// コントロール通知メッセージ
		// ツリービュー
		if (((NMHDR *)lParam)->hwndFrom == GetDlgItem(hWnd, ID_TREE)) {
			return treeview_notify_proc(hWnd, lParam);
		}
		// リストビュー
		if (((NMHDR *)lParam)->hwndFrom == GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST)) {
			return listview_notify_proc(hWnd, lParam);
		}
		if (((NMHDR *)lParam)->hwndFrom == GetWindow(GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST), GW_CHILD)) {
			return listview_header_notify_proc(GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST),
				GetDlgItem(hWnd, ID_TREE), lParam);
		}
		// ステータスバー
		if (((NMHDR *)lParam)->code == TTN_NEEDTEXT && ((NMHDR *)lParam)->idFrom < 3) {
			return statusbar_notify_proc(GetDlgItem(hWnd, ID_STATUSBAR), lParam);
		}
		// ツールバー
		if (((NMHDR *)lParam)->hwndFrom == GetDlgItem(hWnd, ID_TOOLBAR) &&
			((NMHDR *)lParam)->code == NM_CUSTOMDRAW) {
			LRESULT dark_ret;

			if (dark_mode_toolbar_customdraw(lParam, &dark_ret) == TRUE) {
				return dark_ret;
			}
			break;
		}
		if (((NMHDR *)lParam)->code == TTN_NEEDTEXT) {
			((TOOLTIPTEXT*)lParam)->hinst = hInst;
			((TOOLTIPTEXT*)lParam)->lpszText = MAKEINTRESOURCE(((NMHDR *)lParam)->idFrom);
		}
		break;

	case WM_TV_EVENT:
		// ツリービューイベント
		switch (wParam) {
		case TVN_BEGINLABELEDIT:
			if (treeview_get_rootitem(GetDlgItem(hWnd, ID_TREE), ((TV_DISPINFO *)lParam)->item.hItem) == regist_treeitem &&
				treeview_get_lparam(GetDlgItem(hWnd, ID_TREE), ((TV_DISPINFO *)lParam)->item.hItem) != 0 &&
				((DATA_INFO *)treeview_get_lparam(GetDlgItem(hWnd, ID_TREE), ((TV_DISPINFO *)lParam)->item.hItem))->type != TYPE_DATA) {
				SendMessage(hWnd, WM_ENABLE_ACCELERATOR, FALSE, 0);
				return FALSE;
			}
			return TRUE;

		case TVN_ENDLABELEDIT:
			SendMessage(hWnd, WM_ENABLE_ACCELERATOR, TRUE, 0);
			return viewer_rename(hWnd, ((TV_DISPINFO *)lParam)->item.hItem, ((TV_DISPINFO *)lParam)->item.pszText);

		case TVN_SELCHANGED:
			set_cursor(TRUE);
			viewer_sel_cheange(hWnd,
				((NM_TREEVIEW *)lParam)->itemOld.hItem, ((NM_TREEVIEW *)lParam)->itemNew.hItem);
			set_enable_window_menu(hWnd);
			set_cursor(FALSE);
			return TRUE;

		case TVN_KEYDOWN:
			switch (((TV_KEYDOWN *)lParam)->wVKey) {
			case 'A':
				if (GetKeyState(VK_CONTROL) < 0) {
					// すべて選択
					SendMessage(hWnd, WM_COMMAND, ID_MENUITEM_SELECT_ALL, 0);
					return TRUE;
				}
				break;

			case 'C':
				if (GetKeyState(VK_CONTROL) < 0) {
					// コピー
					SendMessage(hWnd, WM_COMMAND, ID_MENUITEM_CLIPBOARD, 0);
					return TRUE;
				}
				break;

			case 'V':
				if (GetKeyState(VK_CONTROL) < 0) {
					// 貼り付け
					SendMessage(hWnd, WM_COMMAND, ID_MENUITEM_PASTE, 0);
					return TRUE;
				}
				break;

			case VK_TAB:
				SetFocus(GetDlgItem(hWnd, ID_CONTAINER));
				return TRUE;

			case VK_RETURN:
				SetTimer(hWnd, TIMER_TV_ACTION, 1, NULL);
				return TRUE;

			case VK_DELETE:
				SendMessage(hWnd, WM_COMMAND, ID_MENUITEM_DELETE, 0);
				return TRUE;

			case VK_APPS:
				viewer_show_menu(hWnd);
				return TRUE;

			case VK_F2:
				SendMessage(hWnd, WM_COMMAND, ID_MENUITEM_RENAME, 0);
				return TRUE;
			}
			break;

		case TVN_BEGINDRAG:
		case TVN_BEGINRDRAG:
			SetFocus(GetDlgItem(hWnd, ID_TREE));
			if (viewer_ole_start_drag(hWnd, ((NM_TREEVIEW *)lParam)->itemNew.hItem) == FALSE) {
				if ((DnD_mode = dragdrop_start_drag(hWnd, ((NM_TREEVIEW *)lParam)->itemNew.hItem)) == TRUE) {
					SetTimer(hWnd, TIMER_DRAG, 1, NULL);
				}
			}
			break;

		case NM_CUSTOMDRAW:
			// カスタムドロー
			switch (((LPNMTVCUSTOMDRAW)lParam)->nmcd.dwDrawStage) {
			case CDDS_PREPAINT:
				return CDRF_NOTIFYITEMDRAW;

			case CDDS_ITEMPREPAINT:
				{
					DATA_INFO *di;

					di = (DATA_INFO *)((LPNMTVCUSTOMDRAW)lParam)->nmcd.lItemlParam;
					if (di == NULL || di == &clip_di || di->type != TYPE_ITEM || di->title != NULL) {
						return CDRF_DODEFAULT;
					}
					di = format_get_priority_highest(di);
					if (di->menu_title == NULL && di->format_name != NULL) {
						if (((LPNMTVCUSTOMDRAW)lParam)->nmcd.uItemState == (CDIS_FOCUS | CDIS_SELECTED)) {
							((LPNMTVCUSTOMDRAW)lParam)->clrText = dark_mode_get_color(COLOR_HIGHLIGHTTEXT);
						} else {
							((LPNMTVCUSTOMDRAW)lParam)->clrText = dark_mode_get_accent_color();
						}
					}
				}
				return CDRF_DODEFAULT;
			}
			break;

		case NM_SETFOCUS:
			SetTimer(hWnd, TIMER_REFLECT_DATA, 1, NULL);
			set_enable_window_menu(hWnd);
			break;

		case NM_RCLICK:
			viewer_show_menu(hWnd);
			break;

		case NM_DBLCLK:
			if (TreeView_GetChild(GetDlgItem(hWnd, ID_TREE), TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE))) == NULL &&
				treeview_get_hitest(GetDlgItem(hWnd, ID_TREE)) == TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE))) {
				SetTimer(hWnd, TIMER_TV_ACTION, 1, NULL);
			}
			break;
		}
		break;

	case WM_LV_EVENT:
		// リストビューイベント
		switch (wParam) {
		case LVN_ITEMCHANGED:
			SetTimer(hWnd, TIMER_SET_MENU, 100, NULL);
			break;

		case LVN_GETDISPINFO:
			listview_get_disp_item(GetDlgItem(hWnd, ID_TREE), &(((LV_DISPINFO *)lParam)->item));
			return TRUE;

		case LVN_BEGINLABELEDIT:
			if (treeview_get_rootitem(GetDlgItem(hWnd, ID_TREE),
				TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE))) == regist_treeitem) {
				SendMessage(hWnd, WM_ENABLE_ACCELERATOR, FALSE, 0);
				return FALSE;
			}
			return TRUE;

		case LVN_ENDLABELEDIT:
			SendMessage(hWnd, WM_ENABLE_ACCELERATOR, TRUE, 0);
			return viewer_rename(hWnd, NULL, ((LV_DISPINFO *)lParam)->item.pszText);

		case LVN_ITEMACTIVATE:
			viewer_item_activate(hWnd);
			break;

		case LVN_KEYDOWN:
			switch (((LV_KEYDOWN *)lParam)->wVKey) {
			case 'A':
				if (GetKeyState(VK_CONTROL) < 0) {
					// すべて選択
					SendMessage(hWnd, WM_COMMAND, ID_MENUITEM_SELECT_ALL, 0);
					return TRUE;
				}
				break;

			case 'C':
				if (GetKeyState(VK_CONTROL) < 0) {
					// コピー
					SendMessage(hWnd, WM_COMMAND, ID_MENUITEM_CLIPBOARD, 0);
					return TRUE;
				}
				break;

			case 'V':
				if (GetKeyState(VK_CONTROL) < 0) {
					// 貼り付け
					SendMessage(hWnd, WM_COMMAND, ID_MENUITEM_PASTE, 0);
					return TRUE;
				}
				break;

			case VK_TAB:
				SetFocus(GetDlgItem(hWnd, ID_TREE));
				return TRUE;

			case VK_DELETE:
				SendMessage(hWnd, WM_COMMAND, ID_MENUITEM_DELETE, 0);
				return TRUE;

			case VK_APPS:
				viewer_show_menu(hWnd);
				return TRUE;

			case VK_F2:
				SendMessage(hWnd, WM_COMMAND, ID_MENUITEM_RENAME, 0);
				return TRUE;

			case VK_BACK:
				if (TreeView_GetParent(GetDlgItem(hWnd, ID_TREE),
					TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE))) != NULL) {
					TreeView_SelectItem(GetDlgItem(hWnd, ID_TREE),
						TreeView_GetParent(GetDlgItem(hWnd, ID_TREE),
						TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE))));
					SetFocus(GetDlgItem(hWnd, ID_CONTAINER));
				}
				return TRUE;
			}
			break;

		case LVN_BEGINDRAG:
		case LVN_BEGINRDRAG:
			SetFocus(GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST));
			if (viewer_ole_start_drag(hWnd, NULL) == FALSE) {
				if ((DnD_mode = dragdrop_start_drag(hWnd, NULL)) == TRUE) {
					SetTimer(hWnd, TIMER_DRAG, 1, NULL);
				}
			}
			break;

		case NM_CUSTOMDRAW:
			// カスタムドロー
			switch (((LPNMLVCUSTOMDRAW)lParam)->nmcd.dwDrawStage) {
			case CDDS_PREPAINT:
				return CDRF_NOTIFYITEMDRAW;

			case CDDS_ITEMPREPAINT:
				{
					HTREEITEM hItem;
					DATA_INFO *di;

					hItem = (HTREEITEM)((LPNMLVCUSTOMDRAW)lParam)->nmcd.lItemlParam;
					if (hItem == NULL) {
						return CDRF_DODEFAULT;
					}
					di = (DATA_INFO *)treeview_get_lparam(GetDlgItem(hWnd, ID_TREE), hItem);
					if (di == NULL || di == &clip_di || di->type != TYPE_ITEM || di->title != NULL) {
						return CDRF_DODEFAULT;
					}
					di = format_get_priority_highest(di);
					if (di->menu_title == NULL && di->format_name != NULL) {
						if (((LPNMLVCUSTOMDRAW)lParam)->nmcd.uItemState == (CDIS_FOCUS | CDIS_SELECTED)) {
							((LPNMLVCUSTOMDRAW)lParam)->clrText = dark_mode_get_color(COLOR_HIGHLIGHTTEXT);
						} else {
							((LPNMLVCUSTOMDRAW)lParam)->clrText = dark_mode_get_accent_color();
						}
					}
				}
				return CDRF_DODEFAULT;
			}
			break;

		case NM_SETFOCUS:
			set_enable_window_menu(hWnd);
			break;

		case NM_RCLICK:
			viewer_show_menu(hWnd);
			break;
		}
		break;

	case WM_VIEWER_CHANGE_CLIPBOARD:
		// クリップボードの内容変化
		if (clip_treeitem != NULL) {
			BOOL clip_flag = FALSE;

			set_cursor(TRUE);
			SendMessage(GetDlgItem(hWnd, ID_TREE), WM_SETREDRAW, (WPARAM)FALSE, 0);

			// アイテムの削除
			if (treeview_get_rootitem(GetDlgItem(hWnd, ID_TREE),
				TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE))) == clip_treeitem) {
				if (GetForegroundWindow() == hWnd) {
					SetFocus(GetDlgItem(hWnd, ID_TREE));
				}
				TreeView_SelectItem(GetDlgItem(hWnd, ID_TREE), NULL);
				clip_flag = TRUE;
			}
			treeview_delete_child(GetDlgItem(hWnd, ID_TREE), clip_treeitem);

			// クリップボードの形式取得
			data_free(clip_di.child);
			clip_di.child = clipboard_to_datainfo(hWnd);

			// クリップボードの形式を表示
			treeview_datainfo_to_treeitem(GetDlgItem(hWnd, ID_TREE), clip_treeitem, clip_di.child);
			if (clip_flag == TRUE) {
				// 表示データ更新
				TreeView_SelectItem(GetDlgItem(hWnd, ID_TREE), clip_treeitem);
			}
			SendMessage(GetDlgItem(hWnd, ID_TREE), WM_SETREDRAW, (WPARAM)TRUE, 0);
			UpdateWindow(GetDlgItem(hWnd, ID_TREE));
			set_cursor(FALSE);
		}
		break;

	case WM_VIEWER_CHANGE_WATCH:
		// クリップボード監視切り替え
		CheckMenuItem(GetSubMenu(GetMenu(hWnd), WINDOW_MENU_FILE), ID_MENUITEM_WATCH,
			((option.main_clipboard_watch == 0) ? MF_UNCHECKED : MF_CHECKED));
		break;

	case WM_VIEWER_REFRESH_STATUS:
		// ステータスバーの更新
		statusbar_set_text(hWnd, GetDlgItem(hWnd, ID_STATUSBAR));
		break;

	case WM_DRAGDROP:
		// ドラッグ＆ドロップ
		pdtn = (LPIDROPTARGET_NOTIFY)lParam;
		switch (wParam) {
		case IDROPTARGET_NOTIFY_DRAGENTER:
			DnD_keystate = pdtn->grfKeyState;
		case IDROPTARGET_NOTIFY_DRAGOVER:
			viewer_ole_get_drag_effect(hWnd, pdtn);
			break;

		case IDROPTARGET_NOTIFY_DRAGLEAVE:
			TreeView_Select(GetDlgItem(hWnd, ID_TREE), NULL, TVGN_DROPHILITE);
			ListView_SetItemState(GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST), -1, 0, LVIS_DROPHILITED);
			break;

		case IDROPTARGET_NOTIFY_DROP:
			TreeView_Select(GetDlgItem(hWnd, ID_TREE), NULL, TVGN_DROPHILITE);
			ListView_SetItemState(GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST), -1, 0, LVIS_DROPHILITED);

			if (viewer_ole_create_drop_item(hWnd, pdtn, DnD_keystate) == FALSE) {
				pdtn->dwEffect = DROPEFFECT_NONE;
			}
			break;
		}
		break;

	case WM_GETDATA:
		// ドラッグ＆ドロップしているデータを取得
		*((HGLOBAL *)lParam) = viewer_ole_get_drag_data(hWnd, wParam);
		break;

	case WM_GET_VERSION:
	case WM_GET_WORKPATH:
	case WM_GET_CLIPBOARD_WATCH:
	case WM_SET_CLIPBOARD_WATCH:
	case WM_GET_FORMAT_ICON:
	case WM_ENABLE_ACCELERATOR:
	case WM_REGIST_HOTKEY:
	case WM_UNREGIST_HOTKEY:
	case WM_OPTION_SHOW:
	case WM_OPTION_GET:
	case WM_OPTION_LOAD:
	case WM_OPTION_SAVE:
	case WM_HISTORY_GET_ROOT:
	case WM_HISTORY_LOAD:
	case WM_HISTORY_SAVE:
	case WM_REGIST_GET_ROOT:
	case WM_REGIST_LOAD:
	case WM_REGIST_SAVE:
	case WM_ITEM_TO_CLIPBOARD:
	case WM_ITEM_CREATE:
	case WM_ITEM_COPY:
	case WM_ITEM_FREE:
	case WM_ITEM_FREE_DATA:
	case WM_ITEM_CHECK:
	case WM_ITEM_TO_BYTES:
	case WM_ITEM_FROM_BYTES:
	case WM_ITEM_TO_FILE:
	case WM_ITEM_FROM_FILE:
	case WM_ITEM_GET_PARENT:
	case WM_ITEM_GET_FORMAT_TO_ITEM:
	case WM_ITEM_GET_PRIORITY_HIGHEST:
	case WM_ITEM_GET_TITLE:
	case WM_ITEM_GET_OPEN_INFO:
	case WM_ITEM_GET_SAVE_INFO:
	case WM_VIEWER_SHOW:
		return SendMessage(main_wnd, msg, wParam, lParam);

	case WM_HISTORY_CHANGED:
	case WM_REGIST_CHANGED:
		{
			BOOL history = msg == WM_HISTORY_CHANGED;
			DATA_INFO **items = history ? &history_data.child : &regist_data.child;
			HTREEITEM root = history ? history_treeitem : regist_treeitem;
			HWND tree = GetDlgItem(hWnd, ID_TREE);
			BOOL redraw = IsWindowVisible(tree);
			data_adjust(items);
			tileview_refresh(GetDlgItem(hWnd, history ? ID_CLIPMASTER_TILES : ID_CLIPMASTER_FAVORITES), *items);
			if (history && TabCtrl_GetCurSel(GetDlgItem(hWnd, ID_CLIPMASTER_TABS)) == 2)
				pasteplus_refresh(GetDlgItem(hWnd, ID_CLIPMASTER_PASTEPLUS), *items);
			if (root == NULL) break;
			set_cursor(TRUE);
			if (redraw) SendMessage(tree, WM_SETREDRAW, (WPARAM)FALSE, 0);
			treeview_sync_datainfo(tree, root, *items);
			if (redraw) {
				SendMessage(tree, WM_SETREDRAW, (WPARAM)TRUE, 0);
				UpdateWindow(tree);
			}
			if (treeview_get_rootitem(tree, TreeView_GetSelection(tree)) == root)
				treeview_to_listview(tree, TreeView_GetSelection(tree),
					GetDlgItem(GetDlgItem(hWnd, ID_CONTAINER), ID_LIST));
			SendMessage(hWnd, WM_VIEWER_REFRESH_STATUS, 0, 0);
			set_cursor(FALSE);
		}
		break;

	case WM_VIEWER_GET_HWND:
		// ビューアのウィンドウハンドルを取得
		return (LRESULT)hWnd;

	case WM_VIEWER_GET_MAIN_HWND:
		// 本体のウィンドウハンドルを取得
		return (LRESULT)main_wnd;

	case WM_VIEWER_GET_SELECTION:
		// 選択アイテムを取得
		if (treeview_get_rootitem(GetDlgItem(hWnd, ID_TREE),
			TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE))) == clip_treeitem) {
			return (LRESULT)NULL;
		}
		return (LRESULT)treeview_get_lparam(GetDlgItem(hWnd, ID_TREE),
			TreeView_GetSelection(GetDlgItem(hWnd, ID_TREE)));

	case WM_VIEWER_SELECT_ITEM:
		// ツリーアイテムを選択
		{
			HTREEITEM hItem;

			if (history_treeitem != NULL &&
				(hItem = treeview_lparam_to_item(GetDlgItem(hWnd, ID_TREE), history_treeitem, lParam)) != NULL) {
				TreeView_SelectItem(GetDlgItem(hWnd, ID_TREE), hItem);
				return TRUE;
			}
			if (regist_treeitem != NULL &&
				(hItem = treeview_lparam_to_item(GetDlgItem(hWnd, ID_TREE), regist_treeitem, lParam)) != NULL) {
				TreeView_SelectItem(GetDlgItem(hWnd, ID_TREE), hItem);
				return TRUE;
			}
		}
		return FALSE;

	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

/*
 * viewer_regist - ウィンドウクラスの登録
 */
BOOL viewer_regist(const HINSTANCE hInstance)
{
	WNDCLASS wc;

	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC)viewer_proc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_MAIN));
	wc.hCursor = LoadCursor(NULL, IDC_SIZEWE);
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU_VIEWER);
	wc.lpszClassName = WINDOW_CLASS;
	// ウィンドウクラスの登録
	return RegisterClass(&wc);
}

/*
 * viewer_create - ビューアの作成
 */
HWND viewer_create(const HWND pWnd, const int CmdShow)
{
	HWND hWnd;
	POINT pt;
	RECT work_area;
	int width;
	int height;
	int left;
	int top;

	main_wnd = pWnd;

	// 表示するモニタのDPIに合わせる
	pt.x = option.viewer_rect.left;
	pt.y = option.viewer_rect.top;
	SetDpiFromPoint(pt);
	if (SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0) == FALSE) {
		SetRect(&work_area, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
	}
	width = min(max(MulDiv(work_area.right - work_area.left, option.config_width_percent, 100), Scale(340)),
		work_area.right - work_area.left);
	height = min(max(MulDiv(work_area.bottom - work_area.top, option.config_height_percent, 100), Scale(240)),
		work_area.bottom - work_area.top);
	left = work_area.left + (work_area.right - work_area.left - width) / 2;
	top = work_area.top + (work_area.bottom - work_area.top - height) / 2;
	if (option.config_remember_layout && option.config_saved_geometry) {
		width = min(max(Scale(option.viewer_rect.right), Scale(340)), work_area.right - work_area.left);
		height = min(max(Scale(option.viewer_rect.bottom), Scale(240)), work_area.bottom - work_area.top);
		left = min(max(option.viewer_rect.left, work_area.left), work_area.right - width);
		top = min(max(option.viewer_rect.top, work_area.top), work_area.bottom - height);
	}

	// ウィンドウの作成
	hWnd = CreateWindow(WINDOW_CLASS,
		WINDOW_TITLE,
		WS_OVERLAPPEDWINDOW,
		left,
		top,
		width,
		height,
		NULL, NULL, hInst, NULL);

	if (hWnd == NULL) {
		MessageBox(NULL, message_get_res(IDS_ERROR_WINDOW_INIT), ERROR_TITLE, MB_ICONERROR);
		return NULL;
	}
	ShowWindow(hWnd, CmdShow);
	UpdateWindow(hWnd);
	return hWnd;
}
/* End of source */
