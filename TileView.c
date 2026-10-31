#define _INC_OLE
#include <windows.h>
#undef _INC_OLE
#include <windowsx.h>
#include <commctrl.h>
#include <tchar.h>
#include <shlwapi.h>
#include "General.h"
#include "Memory.h"
#include "String.h"
#include "Data.h"
#include "DarkMode.h"
#include "TileView.h"
#include "gdip.h"

typedef struct {
    DATA_INFO *item;
    TCHAR *text;
    HBITMAP image;
    int image_width;
    int image_height;
    SYSTEMTIME date;
    RECT bounds;
    int kind;
    RECT divider;
} TILE_ENTRY;

typedef struct {
    DATA_INFO *item;
    FILETIME modified;
    RECT bounds;
    RECT divider;
    int kind, image_width, image_height;
} TILE_POSITION;

typedef struct {
    TILE_ENTRY *entries;
    int count;
    int selected;
    int offset;
    int height;
    BOOL favorites;
    BOOL laying_out;
    HFONT font;
    TCHAR keyword[256];
    TILE_POSITION *positions;
    int position_count, layout_width, layout_dpi;
    int left, top, row_height, occupied[2], current_span;
    int *paint_order;
    BOOL samples;
} TILE_VIEW;

static int tile_scale(HWND window, int value)
{
    return MulDiv(value, GetDpiForWindow(window), 96);
}

static TCHAR tile_category(const DATA_INFO *item)
{
    const int priorities[] = { 0, 2, 3, 4, 6, 5, 1 };
    const TCHAR *letters = TEXT("?TRIFAD");
    const DATA_INFO *format;
    int category = 0;
    for (format = item->child; format != NULL; format = format->next) {
        const TCHAR *name = format->format_name;
        int candidate = 0;
        switch (format->format) {
        case CF_TEXT: case CF_UNICODETEXT: case CF_OEMTEXT:
            candidate = 1;
            break;
        case CF_BITMAP: case CF_DIB: case CF_DIBV5: case CF_TIFF: case CF_PALETTE:
        case CF_ENHMETAFILE: case CF_METAFILEPICT:
            candidate = 3;
            break;
        case CF_HDROP:
            candidate = 4;
            break;
        case CF_WAVE: case CF_RIFF:
            candidate = 5;
            break;
        case CF_DSPBITMAP: case CF_DSPENHMETAFILE: case CF_DSPMETAFILEPICT:
        case CF_DSPTEXT: case CF_OWNERDISPLAY:
            candidate = 6;
            break;
        }
        if (!candidate && name != NULL) {
            if (!lstrcmpi(name, TEXT("FileName")) || !lstrcmpi(name, TEXT("FileNameW")) ||
                StrStrI(name, TEXT("FileGroupDescriptor")) || !lstrcmpi(name, TEXT("FileContents")) ||
                !lstrcmpi(name, TEXT("Shell IDList Array"))) candidate = 4;
            else if (StrStrI(name, TEXT("audio/"))) candidate = 5;
            else if (!lstrcmpi(name, TEXT("PNG")) || !lstrcmpi(name, TEXT("JFIF")) ||
                !lstrcmpi(name, TEXT("JPEG")) || !lstrcmpi(name, TEXT("GIF")) ||
                !lstrcmpi(name, TEXT("TIFF")) || StrStrI(name, TEXT("image/"))) candidate = 3;
            else if (!lstrcmpi(name, TEXT("Rich Text Format")) || StrStrI(name, TEXT("text/rtf")) ||
                StrStrI(name, TEXT("text/html")) || !lstrcmpi(name, TEXT("HTML Format"))) candidate = 2;
        }
        if (priorities[candidate] > priorities[category]) category = candidate;
    }
    return letters[category];
}

static HBITMAP tile_preview(HWND window, DATA_INFO *item, int *image_width, int *image_height)
{
    DATA_INFO *format;
    BITMAP bitmap;
    const BYTE *bytes = NULL;
    const BITMAPINFOHEADER *header = NULL;
    SIZE_T size, offset = 0;
    ULONGLONG stride;
    DWORD colors;
    int width = 0, height = 0, preview_width, preview_height;
    HDC context, destination, source;
    HBITMAP preview, old_preview, old_source;
    BOOL drawn;
    for (format = item->child; format != NULL; format = format->next) {
        if (format->format == CF_UNICODETEXT || format->format == CF_TEXT || format->format == CF_OEMTEXT) {
            return NULL;
        }
    }
    for (format = item->child; format != NULL; format = format->next) {
        if (format->data == NULL) continue;
        if (format->format == CF_BITMAP) {
            if (!GetObject(format->data, sizeof(bitmap), &bitmap)) continue;
            width = bitmap.bmWidth;
            height = bitmap.bmHeight;
            break;
        }
        if (format->format == CF_TIFF || (format->format_name &&
            (!lstrcmpi(format->format_name, TEXT("PNG")) || !lstrcmpi(format->format_name, TEXT("JFIF")) ||
             !lstrcmpi(format->format_name, TEXT("JPEG")) || !lstrcmpi(format->format_name, TEXT("GIF")) ||
             !lstrcmpi(format->format_name, TEXT("TIFF")) || StrStrI(format->format_name, TEXT("image/"))))) {
            BYTE *encoded = GlobalLock(format->data);
            HBITMAP image = encoded ? image_bytes_to_bitmap(encoded,
                (DWORD)min(GlobalSize(format->data), format->size), image_width, image_height) : NULL;
            if (encoded) GlobalUnlock(format->data);
            if (image) return image;
        }
        if (format->format != CF_DIB && format->format != CF_DIBV5) continue;
        size = min(GlobalSize(format->data), format->size);
        if (size < sizeof(BITMAPINFOHEADER)) continue;
        bytes = GlobalLock(format->data);
        if (bytes == NULL) continue;
        header = (const BITMAPINFOHEADER *)bytes;
        if (header->biSize < sizeof(BITMAPINFOHEADER) || header->biSize > size ||
            header->biWidth <= 0 || header->biHeight == 0 || header->biHeight == LONG_MIN ||
            header->biPlanes != 1 || (header->biCompression != BI_RGB && header->biCompression != BI_BITFIELDS) ||
            (header->biBitCount != 1 && header->biBitCount != 4 && header->biBitCount != 8 &&
             header->biBitCount != 16 && header->biBitCount != 24 && header->biBitCount != 32)) {
            GlobalUnlock(format->data);
            bytes = NULL;
            continue;
        }
        colors = header->biClrUsed;
        if (colors == 0 && header->biBitCount <= 8) colors = 1U << header->biBitCount;
        if (colors > (size - header->biSize) / sizeof(RGBQUAD)) {
            GlobalUnlock(format->data);
            bytes = NULL;
            continue;
        }
        offset = header->biSize + colors * sizeof(RGBQUAD);
        if (header->biSize == sizeof(BITMAPINFOHEADER) && header->biCompression == BI_BITFIELDS) offset += 12;
        width = header->biWidth;
        height = header->biHeight < 0 ? -header->biHeight : header->biHeight;
        stride = ((ULONGLONG)width * header->biBitCount + 31) / 32 * 4;
        if (offset > size || stride * height > size - offset) {
            GlobalUnlock(format->data);
            bytes = NULL;
            continue;
        }
        break;
    }
    if (format == NULL || width <= 0 || height <= 0) return NULL;
    *image_width = width;
    *image_height = height;
    preview_width = width >= height ? min(width, 384) : max(1, MulDiv(width, min(height, 384), height));
    preview_height = height >= width ? min(height, 384) : max(1, MulDiv(height, min(width, 384), width));
    context = GetDC(window);
    destination = CreateCompatibleDC(context);
    preview = CreateCompatibleBitmap(context, preview_width, preview_height);
    old_preview = SelectObject(destination, preview);
    SetStretchBltMode(destination, HALFTONE);
    SetBrushOrgEx(destination, 0, 0, NULL);
    if (bytes != NULL) {
        int lines = StretchDIBits(destination, 0, 0, preview_width, preview_height, 0, 0, width, height,
            bytes + offset, (const BITMAPINFO *)bytes, DIB_RGB_COLORS, SRCCOPY);
        drawn = lines != 0 && lines != GDI_ERROR;
        GlobalUnlock(format->data);
    } else {
        source = CreateCompatibleDC(context);
        old_source = SelectObject(source, format->data);
        drawn = StretchBlt(destination, 0, 0, preview_width, preview_height, source, 0, 0, width, height, SRCCOPY);
        SelectObject(source, old_source);
        DeleteDC(source);
    }
    SelectObject(destination, old_preview);
    DeleteDC(destination);
    ReleaseDC(window, context);
    if (!drawn) {
        DeleteObject(preview);
        return NULL;
    }
    return preview;
}

BOOL tileview_has_image(HWND window, DATA_INFO *item)
{
    int width = 0, height = 0;
    HBITMAP image = tile_preview(window, item, &width, &height);
    if (image != NULL) DeleteObject(image);
    return image != NULL;
}

BOOL tileview_portrait_image(HWND window, DATA_INFO *item)
{
    int width = 0, height = 0;
    HBITMAP image = tile_preview(window, item, &width, &height);
    if (image != NULL) DeleteObject(image);
    return image != NULL && height > width;
}

static TCHAR *tile_text(DATA_INFO *item)
{
    DATA_INFO *format;
    const TCHAR *source;
    TCHAR *text;
    SIZE_T length, available;
    for (format = item->child; format != NULL; format = format->next) {
        if (format->format != CF_UNICODETEXT || format->data == NULL) {
            continue;
        }
        available = min(GlobalSize(format->data), format->size) / sizeof(TCHAR);
        source = GlobalLock(format->data);
        if (source == NULL) {
            continue;
        }
        for (length = 0; length < available && length < 65536 && source[length]; length++) {}
        text = mem_alloc((length + 1) * sizeof(TCHAR));
        if (text != NULL) {
            CopyMemory(text, source, length * sizeof(TCHAR));
            text[length] = TEXT('\0');
        }
        GlobalUnlock(format->data);
        return text;
    }
    return alloc_copy(data_get_title(item));
}

static void tile_clear(TILE_VIEW *view)
{
    int index;
    for (index = 0; index < view->count; index++) {
        mem_free(&view->entries[index].text);
        if (view->entries[index].image != NULL) DeleteObject(view->entries[index].image);
    }
    mem_free(&view->entries);
    mem_free(&view->paint_order);
    view->count = 0;
}

static int __cdecl tile_compare_position(void *context, const void *first, const void *second)
{
    TILE_VIEW *view = context;
    RECT *left = &view->entries[*(const int *)first].bounds;
    RECT *right = &view->entries[*(const int *)second].bounds;
    return left->top != right->top ? (left->top > right->top ? 1 : -1) :
        left->left != right->left ? (left->left > right->left ? 1 : -1) : 0;
}

static void tile_layout(HWND window, TILE_VIEW *view)
{
    RECT client, measured;
    SCROLLINFO scroll;
    HDC context;
    HFONT previous;
    int index, sequence, start = 0, width, height, left, top, row_height = 0;
    int occupied[2] = { 0, 0 }, current_span = 0, column, candidate;
    int gap = tile_scale(window, 8);
    int margin = tile_scale(window, 12);
    int cell = tile_scale(window, 96);
    int block = cell * 2 + gap;
    TILE_ENTRY *entry, *prior = NULL;
    if (view->laying_out) {
        return;
    }
    view->laying_out = TRUE;
    GetClientRect(window, &client);
    left = margin;
    top = margin;
    if (!view->favorites) {
        BOOL reusable = view->layout_width == client.right && view->layout_dpi == (int)GetDpiForWindow(window) &&
            view->position_count <= view->count;
        for (sequence = 0; reusable && sequence < view->position_count; sequence++) {
            TILE_POSITION *position = &view->positions[sequence];
            entry = &view->entries[view->count - 1 - sequence];
            reusable = position->item == entry->item &&
                CompareFileTime(&position->modified, &entry->item->modified) == 0 &&
                position->kind == entry->kind && position->image_width == entry->image_width &&
                position->image_height == entry->image_height;
        }
        if (!reusable) view->position_count = 0;
        start = view->position_count;
        if (start) {
            left = view->left;
            top = view->top;
            row_height = view->row_height;
            occupied[0] = view->occupied[0];
            occupied[1] = view->occupied[1];
            current_span = view->current_span;
            prior = &view->entries[view->count - start];
        }
        if (view->count != view->position_count) {
            TILE_POSITION *positions = view->count ? mem_calloc(view->count * sizeof(TILE_POSITION)) : NULL;
            if (view->count && positions == NULL) {
                view->laying_out = FALSE;
                return;
            }
            if (start) CopyMemory(positions, view->positions, start * sizeof(TILE_POSITION));
            mem_free(&view->positions);
            view->positions = positions;
        }
    }
    context = GetDC(window);
    previous = SelectObject(context, view->font);
    for (sequence = start; sequence < view->count; sequence++) {
        index = view->favorites ? sequence : view->count - 1 - sequence;
        entry = &view->entries[index];
        SetRectEmpty(&entry->divider);
        if (!view->favorites && (prior == NULL || entry->date.wYear != prior->date.wYear ||
            entry->date.wMonth != prior->date.wMonth || entry->date.wDay != prior->date.wDay ||
            entry->date.wHour / 12 != prior->date.wHour / 12)) {
            top += row_height ? row_height + gap : 0;
            SetRect(&entry->divider, margin, top, client.right - margin, top + tile_scale(window, 28));
            top = entry->divider.bottom;
            left = margin;
            row_height = 0;
            occupied[0] = occupied[1] = 0;
            current_span = 0;
        }
        int columns = 2, rows = 1;
        BOOL portrait = entry->image != NULL && entry->image_height > entry->image_width;
        if (entry->image != NULL) {
            columns = portrait ? 1 : (LONGLONG)entry->image_width >= (LONGLONG)entry->image_height * 4 ? 4 : 2;
            if ((LONGLONG)entry->image_width < (LONGLONG)entry->image_height * 2) rows = 2;
        }
        width = columns * (cell + gap) - gap;
        height = entry->image == NULL ? tile_scale(window, 48) : rows * (cell + gap) - gap;
        while (entry->image == NULL && (height < cell || columns < 4)) {
            SetRect(&measured, 0, 0, max(1, width - gap * 2), 0);
            DrawText(context, entry->text, -1, &measured, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            if (measured.bottom <= height - gap * 2) {
                break;
            }
            if (height < cell) height = cell;
            else columns *= 2;
            width = columns * (cell + gap) - gap;
        }
        if (entry->kind >= 1 && entry->kind <= 4) {
            columns = entry->kind == 3 ? 4 : 2;
            rows = entry->kind == 4 ? 2 : 1;
            width = columns * (cell + gap) - gap;
            height = entry->kind == 1 ? tile_scale(window, 48) : rows * (cell + gap) - gap;
        }
        if (entry->kind == 5 && (portrait || entry->item == NULL)) {
            columns = 1;
            width = cell;
            height = block;
        }
        width = min(width, max(1, client.right - margin * 2));
        int span = columns == 4 ? 2 : 1;
        int slots = height <= tile_scale(window, 48) ? 1 : height <= cell ? 2 : 4;
        // Only small/horizontal tiles stack below; under a long tile they fill the least-used half.
        column = -1;
        for (candidate = 0; columns != 1 && span == 1 && candidate < current_span; candidate++) {
            if (occupied[candidate] + slots <= 4 &&
                left + candidate * (block + gap) + width <= client.right - margin &&
                (column < 0 || occupied[candidate] < occupied[column])) column = candidate;
        }
        if (column < 0) {
            if (current_span != 0) left += current_span < 0 ? cell + gap : current_span * (block + gap);
            if (left > margin && left + width > client.right - margin) {
                top += row_height + gap;
                left = margin;
            }
            occupied[0] = occupied[1] = 0;
            current_span = columns == 1 ? -1 : span;
            column = 0;
        }
        int tile_left = left + column * (block + gap);
        int tile_top = top + MulDiv(occupied[column], block, 4);
        SetRect(&entry->bounds, tile_left, tile_top, tile_left + width, tile_top + height);
        if (span == 2) occupied[0] = occupied[1] = slots;
        else occupied[column] += slots;
        row_height = block;
        prior = entry;
        if (!view->favorites) {
            TILE_POSITION *position = &view->positions[sequence];
            position->item = entry->item;
            position->modified = entry->item->modified;
            position->bounds = entry->bounds;
            position->divider = entry->divider;
            position->kind = entry->kind;
            position->image_width = entry->image_width;
            position->image_height = entry->image_height;
        }
    }
    SelectObject(context, previous);
    ReleaseDC(window, context);
    if (!view->favorites) {
        int end = top + row_height;
        view->position_count = view->count;
        view->layout_width = client.right;
        view->layout_dpi = GetDpiForWindow(window);
        view->left = left;
        view->top = top;
        view->row_height = row_height;
        view->occupied[0] = occupied[0];
        view->occupied[1] = occupied[1];
        view->current_span = current_span;
        for (sequence = 0; sequence < view->count; sequence++) {
            TILE_POSITION *position = &view->positions[sequence];
            entry = &view->entries[view->count - 1 - sequence];
            SetRect(&entry->bounds, client.right - position->bounds.right,
                margin + end - position->bounds.bottom + tile_scale(window, 28),
                client.right - position->bounds.left,
                margin + end - position->bounds.top + tile_scale(window, 28));
            SetRectEmpty(&entry->divider);
            if (sequence + 1 == view->count || !IsRectEmpty(&view->positions[sequence + 1].divider)) {
                int group_end = sequence + 1 == view->count ? end : view->positions[sequence + 1].divider.top - gap;
                SetRect(&entry->divider, margin, margin + end - group_end,
                    client.right - margin, margin + end - group_end + tile_scale(window, 28));
            }
        }
    }
    mem_free(&view->paint_order);
    view->paint_order = view->count ? mem_alloc(view->count * sizeof(int)) : NULL;
    if (view->paint_order) {
        for (index = 0; index < view->count; index++) view->paint_order[index] = index;
        qsort_s(view->paint_order, view->count, sizeof(int), tile_compare_position, view);
    }
    view->height = top + row_height + margin;
    ZeroMemory(&scroll, sizeof(scroll));
    scroll.cbSize = sizeof(scroll);
    scroll.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    scroll.nMax = max(0, view->height - 1);
    scroll.nPage = client.bottom;
    scroll.nPos = view->offset;
    SetScrollInfo(window, SB_VERT, &scroll, TRUE);
    view->offset = GetScrollPos(window, SB_VERT);
    view->laying_out = FALSE;
    InvalidateRect(window, NULL, FALSE);
}

void tileview_refresh(HWND window, DATA_INFO *items)
{
    TILE_VIEW *view = (TILE_VIEW *)GetWindowLongPtr(window, GWLP_USERDATA);
    DATA_INFO *item;
    int count = 0;
    if (view == NULL) {
        return;
    }
    tile_clear(view);
    view->selected = -1;
    for (item = items; item != NULL; item = item->next) {
        if (item->type == TYPE_ITEM) {
            count++;
        }
    }
    count += view->favorites ? 5 : 0;
    view->entries = count ? mem_calloc(sizeof(TILE_ENTRY) * count) : NULL;
    if (view->entries != NULL) {
        for (item = items; item != NULL; item = item->next) {
            if (item->type != TYPE_ITEM) {
                continue;
            }
            view->entries[view->count].item = item;
            view->entries[view->count].text = tile_text(item);
            if (view->entries[view->count].text == NULL) {
                continue;
            }
            FileTimeToSystemTime(&item->modified, &view->entries[view->count].date);
            view->entries[view->count].image = tile_preview(window, item,
                &view->entries[view->count].image_width, &view->entries[view->count].image_height);
            if (item->plugin_string != NULL && lstrlen(item->plugin_string) == 17 &&
                _tcsncmp(item->plugin_string, TEXT("ClipMaster:tile:"), 16) == 0) {
                view->entries[view->count].kind = item->plugin_string[16] - TEXT('0');
            }
            view->count++;
        }
        if (view->favorites) {
            for (count = 1; count <= 5; count++) {
                view->entries[view->count].kind = count;
                view->entries[view->count++].text = alloc_copy(TEXT("+"));
            }
        }
    }
    tile_layout(window, view);
}

void tileview_search(HWND window, const TCHAR *keyword)
{
    TILE_VIEW *view = (TILE_VIEW *)GetWindowLongPtr(window, GWLP_USERDATA);
    if (view != NULL) {
        lstrcpyn(view->keyword, keyword, ARRAYSIZE(view->keyword));
        InvalidateRect(window, NULL, FALSE);
    }
}

void tileview_favorites(HWND window, BOOL enabled)
{
    TILE_VIEW *view = (TILE_VIEW *)GetWindowLongPtr(window, GWLP_USERDATA);
    if (view != NULL) view->favorites = enabled;
}

void tileview_samples(HWND window, BOOL enabled)
{
    TILE_VIEW *view = (TILE_VIEW *)GetWindowLongPtr(window, GWLP_USERDATA);
    if (view != NULL) view->samples = enabled;
}

static void tile_draw_text(HDC context, RECT bounds, const TCHAR *text, const TCHAR *keyword, COLORREF foreground)
{
    TEXTMETRIC metrics;
    SIZE extent;
    int length = lstrlen(text), position = 0, fit, line_end, cursor, match_end = 0;
    int keyword_length = lstrlen(keyword);
    int left, next, count;
    const TCHAR *match = keyword_length ? StrStrI(text, keyword) : NULL;
    GetTextMetrics(context, &metrics);
    while (position < length && bounds.top + metrics.tmHeight <= bounds.bottom) {
        GetTextExtentExPoint(context, text + position, length - position, bounds.right - bounds.left,
            &fit, NULL, &extent);
        fit = max(1, fit);
        line_end = position + fit;
        for (cursor = position; cursor < line_end; cursor++) {
            if (text[cursor] == TEXT('\r') || text[cursor] == TEXT('\n')) {
                line_end = cursor;
                break;
            }
        }
        if (cursor == position + fit && line_end < length) {
            for (cursor = line_end; cursor > position; cursor--) {
                if (text[cursor - 1] == TEXT(' ') || text[cursor - 1] == TEXT('\t')) {
                    line_end = cursor;
                    break;
                }
            }
        }
        left = bounds.left;
        cursor = position;
        while (cursor < line_end) {
            while (match != NULL && match + keyword_length <= text + cursor) {
                match = StrStrI(match + keyword_length, keyword);
            }
            next = match != NULL ? (int)(match - text) : length;
            match_end = next + keyword_length;
            count = min(line_end, cursor >= next ? match_end : next) - cursor;
            count = max(1, count);
            SetTextColor(context, cursor >= next && cursor < match_end ? RGB(220, 48, 48) : foreground);
            TextOut(context, left, bounds.top, text + cursor, count);
            GetTextExtentPoint32(context, text + cursor, count, &extent);
            left += extent.cx;
            cursor += count;
        }
        position = line_end;
        if (position < length && text[position] == TEXT('\r')) position++;
        if (position < length && text[position] == TEXT('\n')) position++;
        bounds.top += metrics.tmHeight;
    }
}

static void tile_paint(HWND window, TILE_VIEW *view, HDC target)
{
    RECT client, bounds, text_bounds;
    HDC context = CreateCompatibleDC(target);
    HBITMAP buffer, old_bitmap;
    HFONT old_font, category_font;
    HBRUSH surface, search_surface, category_surfaces[2];
    HPEN pen, old_pen;
    int index, sequence;
    TCHAR date[64];
    GetClientRect(window, &client);
    buffer = CreateCompatibleBitmap(target, max(1, client.right), max(1, client.bottom));
    old_bitmap = SelectObject(context, buffer);
    old_font = SelectObject(context, view->font);
    FillRect(context, &client, dark_mode_get_brush(COLOR_WINDOW));
    surface = CreateSolidBrush(dark_mode_get_color(COLOR_BTNFACE));
    search_surface = CreateSolidBrush(RGB(255, 255, 224));
    category_surfaces[0] = CreateSolidBrush(RGB(160, 160, 160));
    category_surfaces[1] = CreateSolidBrush(RGB(88, 88, 88));
    category_font = CreateFont(-tile_scale(window, 14), 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, TEXT("Segoe UI"));
    pen = CreatePen(PS_SOLID, 1, dark_mode_get_color(COLOR_3DSHADOW));
    old_pen = SelectObject(context, pen);
    SetBkMode(context, TRANSPARENT);
    for (sequence = 0; sequence < view->count; sequence++) {
        index = view->paint_order ? view->paint_order[sequence] : sequence;
        TILE_ENTRY *entry = &view->entries[index];
        RECT badge;
        BOOL show_category = entry->item != NULL;
        BOOL matched = view->keyword[0] != TEXT('\0') && StrStrI(entry->text, view->keyword) != NULL;
        bounds = entry->divider;
        OffsetRect(&bounds, 0, -view->offset);
        if (!IsRectEmpty(&entry->divider) && bounds.bottom >= 0 && bounds.top <= client.bottom) {
            wsprintf(date, TEXT("%02d/%02d/%s"), entry->date.wMonth, entry->date.wDay,
                entry->date.wHour < 12 ? TEXT("\xC624\xC804") : TEXT("\xC624\xD6C4"));
            if (view->samples) lstrcpy(date, TEXT("Clip samples"));
            SetTextColor(context, dark_mode_get_color(COLOR_WINDOWTEXT));
            DrawText(context, date, -1, &bounds, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            MoveToEx(context, bounds.left + tile_scale(window, 100), (bounds.top + bounds.bottom) / 2, NULL);
            LineTo(context, bounds.right, (bounds.top + bounds.bottom) / 2);
        }
        bounds = entry->bounds;
        OffsetRect(&bounds, 0, -view->offset);
        if (bounds.bottom < 0 || bounds.top > client.bottom) {
            continue;
        }
        FillRect(context, &bounds, matched ? search_surface : surface);
        FrameRect(context, &bounds, dark_mode_get_brush(index == view->selected ? COLOR_HIGHLIGHT : COLOR_3DSHADOW));
        text_bounds = bounds;
        InflateRect(&text_bounds, -tile_scale(window, 8), -tile_scale(window, 8));
        SetRect(&badge, bounds.right - tile_scale(window, 23), bounds.bottom - tile_scale(window, 23),
            bounds.right - tile_scale(window, 3), bounds.bottom - tile_scale(window, 3));
        if (view->favorites && entry->item != NULL) {
            RECT close = bounds;
            close.left = close.right - tile_scale(window, 24);
            close.bottom = close.top + tile_scale(window, 24);
            SetTextColor(context, dark_mode_get_color(COLOR_WINDOWTEXT));
            DrawText(context, TEXT("\x00D7"), -1, &close, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            text_bounds.top += tile_scale(window, 16);
        }
        SaveDC(context);
        IntersectClipRect(context, text_bounds.left, text_bounds.top, text_bounds.right, text_bounds.bottom);
        if (show_category && entry->image == NULL)
            ExcludeClipRect(context, badge.left - tile_scale(window, 2), badge.top, badge.right, badge.bottom);
        if (entry->item == NULL) {
            HFONT large = CreateFont(-tile_scale(window, 28), 0, 0, 0, FW_NORMAL,
                FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, TEXT("Segoe UI"));
            HFONT previous = SelectObject(context, large);
            SetTextColor(context, dark_mode_get_color(COLOR_WINDOWTEXT));
            DrawText(context, TEXT("+"), -1, &text_bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(context, previous);
            DeleteObject(large);
            if (entry->kind == 4 || entry->kind == 5) {
                RECT label = text_bounds;
                HFONT label_font = CreateFont(-tile_scale(window, 16), 0, 0, 0, FW_MEDIUM,
                    FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH, TEXT("Segoe UI"));
                previous = SelectObject(context, label_font);
                label.top = (text_bounds.top + text_bounds.bottom) / 2 + tile_scale(window, 20);
                SetTextColor(context, RGB(128, 128, 128));
                DrawText(context, TEXT("image only"), -1, &label, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
                SelectObject(context, previous);
                DeleteObject(label_font);
            }
        } else if (entry->image != NULL) {
            BITMAP bitmap;
            HDC source = CreateCompatibleDC(context);
            HBITMAP old_image = SelectObject(source, entry->image);
            int width = text_bounds.right - text_bounds.left;
            int height = text_bounds.bottom - text_bounds.top;
            int source_width, source_height;
            GetObject(entry->image, sizeof(bitmap), &bitmap);
            source_width = bitmap.bmWidth;
            source_height = bitmap.bmHeight;
            if ((LONGLONG)source_width * height > (LONGLONG)source_height * width) {
                source_width = max(1, MulDiv(source_height, width, max(1, height)));
            } else {
                source_height = max(1, MulDiv(source_width, height, max(1, width)));
            }
            SetStretchBltMode(context, HALFTONE);
            SetBrushOrgEx(context, 0, 0, NULL);
            StretchBlt(context, text_bounds.left, text_bounds.top, width, height, source,
                (bitmap.bmWidth - source_width) / 2, (bitmap.bmHeight - source_height) / 2,
                source_width, source_height, SRCCOPY);
            SelectObject(source, old_image);
            DeleteDC(source);
        } else {
            tile_draw_text(context, text_bounds, entry->text, view->keyword,
                matched ? RGB(0, 0, 0) : dark_mode_get_color(COLOR_WINDOWTEXT));
        }
        RestoreDC(context, -1);
        if (show_category) {
            TCHAR label[] = { tile_category(entry->item), TEXT('\0') };
            SaveDC(context);
            SelectObject(context, category_surfaces[label[0] == TEXT('T') || label[0] == TEXT('R') || label[0] == TEXT('I') ? 0 : 1]);
            SelectObject(context, GetStockObject(NULL_PEN));
            Ellipse(context, badge.left, badge.top, badge.right, badge.bottom);
            SelectObject(context, category_font);
            SetTextColor(context, RGB(255, 255, 255));
            DrawText(context, label, -1, &badge, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            RestoreDC(context, -1);
        }
        if (index == view->selected && GetFocus() == window) {
            InflateRect(&bounds, -3, -3);
            DrawFocusRect(context, &bounds);
        }
    }
    BitBlt(target, 0, 0, client.right, client.bottom, context, 0, 0, SRCCOPY);
    SelectObject(context, old_pen);
    SelectObject(context, old_font);
    SelectObject(context, old_bitmap);
    DeleteObject(pen);
    DeleteObject(surface);
    DeleteObject(search_surface);
    DeleteObject(category_surfaces[0]);
    DeleteObject(category_surfaces[1]);
    DeleteObject(category_font);
    DeleteObject(buffer);
    DeleteDC(context);
}

static LRESULT CALLBACK tile_proc(HWND window, UINT message, WPARAM parameter, LPARAM data)
{
    TILE_VIEW *view = (TILE_VIEW *)GetWindowLongPtr(window, GWLP_USERDATA);
    SCROLLINFO scroll;
    POINT point;
    int index;
    if (message == WM_CREATE) {
        view = mem_calloc(sizeof(TILE_VIEW));
        if (view == NULL) return -1;
        view->selected = -1;
        view->font = CreateFont(-MulDiv(9, GetDpiForWindow(window), 72), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, TEXT("Segoe UI"));
        SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)view);
        return 0;
    }
    if (view == NULL) return DefWindowProc(window, message, parameter, data);
    switch (message) {
    case WM_SIZE:
        tile_layout(window, view);
        return 0;
    case WM_DPICHANGED_AFTERPARENT:
        if (view->font != NULL) DeleteObject(view->font);
        view->font = CreateFont(-MulDiv(9, GetDpiForWindow(window), 72), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, TEXT("Segoe UI"));
        tile_layout(window, view);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC context = BeginPaint(window, &paint);
            tile_paint(window, view, context);
            EndPaint(window, &paint);
        }
        return 0;
    case WM_PRINTCLIENT:
        tile_paint(window, view, (HDC)parameter);
        return 0;
    case WM_MOUSEWHEEL:
    case WM_VSCROLL:
        ZeroMemory(&scroll, sizeof(scroll));
        scroll.cbSize = sizeof(scroll);
        scroll.fMask = SIF_ALL;
        GetScrollInfo(window, SB_VERT, &scroll);
        if (message == WM_MOUSEWHEEL) {
            scroll.nPos -= GET_WHEEL_DELTA_WPARAM(parameter) / WHEEL_DELTA * tile_scale(window, 72);
        } else {
            switch (LOWORD(parameter)) {
            case SB_LINEUP: scroll.nPos -= tile_scale(window, 32); break;
            case SB_LINEDOWN: scroll.nPos += tile_scale(window, 32); break;
            case SB_PAGEUP: scroll.nPos -= scroll.nPage; break;
            case SB_PAGEDOWN: scroll.nPos += scroll.nPage; break;
            case SB_THUMBTRACK: scroll.nPos = scroll.nTrackPos; break;
            case SB_TOP: scroll.nPos = 0; break;
            case SB_BOTTOM: scroll.nPos = scroll.nMax; break;
            }
        }
        scroll.fMask = SIF_POS;
        SetScrollInfo(window, SB_VERT, &scroll, TRUE);
        view->offset = GetScrollPos(window, SB_VERT);
        InvalidateRect(window, NULL, FALSE);
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        point.x = GET_X_LPARAM(data);
        point.y = GET_Y_LPARAM(data) + view->offset;
        for (index = 0; index < view->count; index++) {
            if (PtInRect(&view->entries[index].bounds, point)) {
                TILE_ENTRY *entry = &view->entries[index];
                UINT notification = message == WM_LBUTTONUP ? WM_TILE_PASTE : WM_TILE_FAVORITE;
                view->selected = index;
                SetFocus(window);
                InvalidateRect(window, NULL, FALSE);
                if (view->samples) {
                    return message == WM_LBUTTONUP ? SendMessage(GetParent(window), WM_TILE_SAMPLE, entry->kind, (LPARAM)entry->item) : 0;
                }
                if (view->favorites) {
                    notification = entry->item == NULL ? WM_TILE_NEW : message == WM_RBUTTONUP ? WM_TILE_EDIT : WM_TILE_PASTE;
                    if (entry->item != NULL && message == WM_LBUTTONUP &&
                        point.x >= entry->bounds.right - tile_scale(window, 24) &&
                        point.y < entry->bounds.top + tile_scale(window, 24)) notification = WM_TILE_DELETE;
                }
                return SendMessage(GetParent(window), notification, entry->kind, (LPARAM)entry->item);
            }
        }
        return 0;
    case WM_KEYDOWN:
        if (parameter == VK_UP || parameter == VK_LEFT || parameter == VK_DOWN || parameter == VK_RIGHT) {
            view->selected = max(0, min(view->count - 1, view->selected +
                (parameter == VK_UP || parameter == VK_LEFT ? -1 : 1)));
            if (view->selected < view->count) {
                RECT client;
                GetClientRect(window, &client);
                view->offset = max(0, min(view->entries[view->selected].bounds.top,
                    view->height - client.bottom));
                SetScrollPos(window, SB_VERT, view->offset, TRUE);
            }
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        if (parameter == VK_RETURN && view->selected >= 0 && view->selected < view->count) {
            TILE_ENTRY *entry = &view->entries[view->selected];
            return SendMessage(GetParent(window), view->samples ? WM_TILE_SAMPLE : entry->item == NULL ? WM_TILE_NEW : WM_TILE_PASTE,
                entry->kind, (LPARAM)entry->item);
        }
        break;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(window, NULL, FALSE);
        return 0;
    case WM_NCDESTROY:
        tile_clear(view);
        mem_free(&view->positions);
        if (view->font != NULL) DeleteObject(view->font);
        mem_free(&view);
        SetWindowLongPtr(window, GWLP_USERDATA, 0);
        break;
    }
    return DefWindowProc(window, message, parameter, data);
}

HWND tileview_create(HINSTANCE instance, HWND parent, int identifier)
{
    WNDCLASS descriptor;
    ZeroMemory(&descriptor, sizeof(descriptor));
    descriptor.lpfnWndProc = tile_proc;
    descriptor.hInstance = instance;
    descriptor.hCursor = LoadCursor(NULL, IDC_ARROW);
    descriptor.lpszClassName = TEXT("ClipMasterTiles");
    if (!RegisterClass(&descriptor) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return NULL;
    }
    return CreateWindowEx(0, descriptor.lpszClassName, TEXT(""),
        WS_CHILD | WS_TABSTOP | WS_VSCROLL | WS_CLIPSIBLINGS, 0, 0, 0, 0,
        parent, (HMENU)(INT_PTR)identifier, instance, NULL);
}