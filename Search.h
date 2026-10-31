/*
 * CLCL
 *
 * Search.h
 */

#pragma once

/* Include Files */
#define _INC_OLE
#include <windows.h>
#undef  _INC_OLE
#include <tchar.h>

/* Struct */
typedef struct _SEARCH_MATCH {
	int pos;
	int len;
} SEARCH_MATCH;

#ifdef __cplusplus
extern "C" {
#endif

/* Function Prototypes */
BOOL search_text_match(const TCHAR *text, const TCHAR *pattern, SEARCH_MATCH **matches, int *match_cnt);
void search_free_matches(SEARCH_MATCH *matches);

#ifdef __cplusplus
}
#endif
/* End of source */
