#ifndef _INC_WINCLIPHISTORY_H
#define _INC_WINCLIPHISTORY_H
#include "Data.h"

/* Posted to the window passed to winclip_history_start. lParam is a WINCLIP_RESULT * that the receiver
 * must release with winclip_result_free (items it hands over to another owner are set to NULL first). */
#define WM_WINCLIP_LOADED (WM_APP + 1160)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    DATA_INFO *items;       /* clip items, oldest first; NULL when nothing was read */
    TCHAR error[256];       /* non-empty when the Windows clipboard history could not be read */
} WINCLIP_RESULT;

/* Reads up to max_items newest entries of the Windows clipboard history (Win+V) on a worker thread. */
BOOL winclip_history_start(HWND window, int max_items);
void winclip_result_free(WINCLIP_RESULT *result);

#ifdef __cplusplus
}
#endif
#endif
