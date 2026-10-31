#define _INC_OLE
#include <windows.h>
#undef _INC_OLE
#include <tchar.h>
#include <shlwapi.h>
#include <math.h>
#include "General.h"
#include "Memory.h"
#include "String.h"
#include "Data.h"
#include "ClipSample.h"

typedef struct {
    DWORD files;
    POINT point;
    BOOL non_client;
    BOOL wide;
} SAMPLE_DROP;

#pragma pack(push, 1)
typedef struct {
    char riff[4];
    DWORD riff_size;
    char wave[4];
    char fmt[4];
    DWORD fmt_size;
    WORD tag, channels;
    DWORD rate, bytes_per_second;
    WORD align, bits;
    char data[4];
    DWORD data_size;
} SAMPLE_WAVE;
#pragma pack(pop)

static BOOL sample_add(DATA_INFO *item, UINT format, const TCHAR *name, const void *bytes, DWORD size)
{
    DATA_INFO *data, **link;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    void *target = memory ? GlobalLock(memory) : NULL;
    if (target == NULL) {
        if (memory) GlobalFree(memory);
        return FALSE;
    }
    CopyMemory(target, bytes, size);
    GlobalUnlock(memory);
    data = data_create_data(format, (TCHAR *)name, memory, size, FALSE, NULL);
    if (data == NULL) {
        GlobalFree(memory);
        return FALSE;
    }
    for (link = &item->child; *link != NULL; link = &(*link)->next) {}
    *link = data;
    return TRUE;
}

static BOOL sample_text(DATA_INFO *item, const TCHAR *text)
{
    return sample_add(item, CF_UNICODETEXT, TEXT("UNICODE TEXT"), text, (lstrlen(text) + 1) * sizeof(TCHAR));
}

static BOOL sample_image(DATA_INFO *item, int width, int height, COLORREF color)
{
    DWORD stride = ((DWORD)width * 3 + 3) & ~3U;
    DWORD size = sizeof(BITMAPINFOHEADER) + stride * height;
    BYTE *bytes = mem_calloc(size);
    BITMAPINFOHEADER *header = (BITMAPINFOHEADER *)bytes;
    BOOL added;
    int x, y;
    if (bytes == NULL) return FALSE;
    header->biSize = sizeof(BITMAPINFOHEADER);
    header->biWidth = width;
    header->biHeight = height;
    header->biPlanes = 1;
    header->biBitCount = 24;
    header->biCompression = BI_RGB;
    header->biSizeImage = stride * height;
    for (y = 0; y < height; y++) {
        BYTE *pixel = bytes + sizeof(BITMAPINFOHEADER) + stride * y;
        for (x = 0; x < width; x++, pixel += 3) {
            pixel[0] = GetBValue(color);
            pixel[1] = GetGValue(color);
            pixel[2] = GetRValue(color);
        }
    }
    added = sample_add(item, CF_DIB, TEXT("DIB"), bytes, size);
    mem_free(&bytes);
    return added;
}

static BOOL sample_audio(DATA_INFO *item)
{
    enum { RATE = 8000, SAMPLES = 2000 };
    BYTE bytes[sizeof(SAMPLE_WAVE) + SAMPLES];
    SAMPLE_WAVE *wave = (SAMPLE_WAVE *)bytes;
    int index;
    ZeroMemory(bytes, sizeof(bytes));
    CopyMemory(wave->riff, "RIFF", 4);
    wave->riff_size = sizeof(bytes) - 8;
    CopyMemory(wave->wave, "WAVE", 4);
    CopyMemory(wave->fmt, "fmt ", 4);
    wave->fmt_size = 16;
    wave->tag = 1;
    wave->channels = 1;
    wave->rate = RATE;
    wave->bytes_per_second = RATE;
    wave->align = 1;
    wave->bits = 8;
    CopyMemory(wave->data, "data", 4);
    wave->data_size = SAMPLES;
    for (index = 0; index < SAMPLES; index++)
        bytes[sizeof(SAMPLE_WAVE) + index] = (BYTE)(128 + 90 * sin(6.283185307 * 440 * index / RATE));
    return sample_add(item, CF_WAVE, TEXT("WAVE DATA"), bytes, sizeof(bytes));
}

static BOOL sample_file_path(TCHAR *path)
{
    TCHAR directory[MAX_PATH];
    HANDLE file;
    DWORD written;
    int pass;
    for (pass = 0; pass < 2; pass++) {
        if (pass == 0) {
            GetModuleFileName(NULL, directory, MAX_PATH);
            PathRemoveFileSpec(directory);
        } else if (!GetTempPath(MAX_PATH, directory)) {
            break;
        }
        PathAppend(directory, TEXT("samples"));
        CreateDirectory(directory, NULL);
        PathCombine(path, directory, TEXT("Lorem Ipsum.txt"));
        if (PathFileExists(path)) return TRUE;
        file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) continue;
        WriteFile(file, "Lorem Ipsum", 11, &written, NULL);
        CloseHandle(file);
        return TRUE;
    }
    return FALSE;
}

static BOOL sample_file(DATA_INFO *item)
{
    TCHAR path[MAX_PATH];
    DWORD size;
    BYTE *bytes;
    BOOL added;
    if (!sample_file_path(path)) return FALSE;
    size = sizeof(SAMPLE_DROP) + (lstrlen(path) + 2) * sizeof(TCHAR);
    bytes = mem_calloc(size);
    if (bytes == NULL) return FALSE;
    ((SAMPLE_DROP *)bytes)->files = sizeof(SAMPLE_DROP);
    ((SAMPLE_DROP *)bytes)->wide = TRUE;
    lstrcpy((TCHAR *)(bytes + sizeof(SAMPLE_DROP)), path);
    added = sample_add(item, CF_HDROP, TEXT("DROP FILE LIST"), bytes, size);
    mem_free(&bytes);
    return added;
}

static DATA_INFO *sample_item(const TCHAR *title, int kind, const FILETIME *stamp)
{
    TCHAR metadata[32];
    DATA_INFO *item = data_create_item(title, FALSE, NULL);
    if (item == NULL) return NULL;
    item->modified = *stamp;
    wsprintf(metadata, TEXT("ClipMaster:tile:%d"), kind);
    item->plugin_string = alloc_copy(metadata);
    return item;
}

static void sample_push(DATA_INFO ***tail, DATA_INFO *item, BOOL complete)
{
    if (item == NULL) return;
    if (!complete) {
        data_free(item);
        return;
    }
    **tail = item;
    *tail = &item->next;
}

DATA_INFO *clipsample_create(void)
{
    static const TCHAR *texts[] = {
        TEXT("Lorem Ipsum"),
        TEXT("Lorem Ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor."),
        TEXT("Lorem Ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation.")
    };
    static const char rtf[] = "{\\rtf1\\ansi{\\colortbl;\\red214\\green69\\blue65;\\red52\\green120\\blue205;}\\cf1\\b Lorem \\cf2\\i Ipsum}";
    static const int sizes[5][2] = { { 192, 48 }, { 192, 96 }, { 384, 96 }, { 192, 192 }, { 96, 192 } };
    static const COLORREF colors[5] = { RGB(214, 69, 65), RGB(236, 150, 34), RGB(46, 160, 90), RGB(52, 120, 205), RGB(130, 80, 190) };
    DATA_INFO *head = NULL, **tail = &head, *item;
    SYSTEMTIME now;
    FILETIME stamp;
    int index;
    GetLocalTime(&now);
    SystemTimeToFileTime(&now, &stamp);
    for (index = 0; index < 3; index++) {
        item = sample_item(NULL, index + 1, &stamp);
        sample_push(&tail, item, item && sample_text(item, texts[index]));
    }
    item = sample_item(NULL, 1, &stamp);
    sample_push(&tail, item, item && sample_text(item, TEXT("Lorem Ipsum")) &&
        sample_add(item, RegisterClipboardFormat(TEXT("Rich Text Format")), TEXT("Rich Text Format"), rtf, sizeof(rtf)));
    item = sample_item(TEXT("Lorem Ipsum.txt"), 1, &stamp);
    sample_push(&tail, item, item && sample_file(item));
    item = sample_item(TEXT("Lorem Ipsum (audio)"), 1, &stamp);
    sample_push(&tail, item, item && sample_audio(item));
    item = sample_item(TEXT("Lorem Ipsum (device)"), 1, &stamp);
    sample_push(&tail, item, item && sample_add(item, CF_DSPTEXT, TEXT("PRIVATE TEXT"), "Lorem Ipsum", 12));
    for (index = 0; index < 5; index++) {
        item = sample_item(NULL, index + 1, &stamp);
        sample_push(&tail, item, item && sample_image(item, sizes[index][0], sizes[index][1], colors[index]));
    }
    return head;
}
