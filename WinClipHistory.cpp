#define _INC_OLE
#include <windows.h>
#undef _INC_OLE
#include <shlwapi.h>
#include <wincodec.h>
#include <algorithm>
#include <new>
#include <string>
#include <system_error>
#include <thread>
#include <vector>
#include <wrl/client.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/base.h>
extern "C" {
#include "General.h"
#include "Memory.h"
#include "String.h"
#include "Data.h"
}
#include "WinClipHistory.h"
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;
using namespace winrt::Windows::ApplicationModel::DataTransfer;

static bool add_format(DATA_INFO *item, UINT format, const wchar_t *name, const void *bytes, size_t size)
{
    DATA_INFO *data, **link;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    void *target = memory ? GlobalLock(memory) : nullptr;
    if (!target) {
        if (memory) GlobalFree(memory);
        return false;
    }
    CopyMemory(target, bytes, size);
    GlobalUnlock(memory);
    data = data_create_data(format, const_cast<wchar_t *>(name), memory, static_cast<DWORD>(size), FALSE, nullptr);
    if (!data) {
        GlobalFree(memory);
        return false;
    }
    for (link = &item->child; *link; link = &(*link)->next) {}
    *link = data;
    return true;
}

static bool add_text(DATA_INFO *item, const winrt::hstring &text)
{
    return add_format(item, CF_UNICODETEXT, L"UNICODE TEXT", text.c_str(), (text.size() + 1) * sizeof(wchar_t));
}

static bool add_narrow(DATA_INFO *item, const wchar_t *format_name, const winrt::hstring &text, UINT code_page)
{
    int length = WideCharToMultiByte(code_page, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) return false;
    std::string bytes(static_cast<size_t>(length) + 1, '\0');
    WideCharToMultiByte(code_page, 0, text.c_str(), static_cast<int>(text.size()), &bytes[0], length, nullptr, nullptr);
    return add_format(item, RegisterClipboardFormat(format_name), format_name, bytes.data(), bytes.size());
}

static bool add_bitmap(DATA_INFO *item, const DataPackageView &view)
{
    using namespace winrt::Windows::Storage::Streams;
    IRandomAccessStreamWithContentType stream = view.GetBitmapAsync().get().OpenReadAsync().get();
    uint64_t size = stream.Size();
    if (size == 0 || size > 0x20000000) return false;
    DataReader reader(stream);
    reader.LoadAsync(static_cast<uint32_t>(size)).get();
    std::vector<uint8_t> encoded(static_cast<size_t>(size));
    reader.ReadBytes(encoded);

    ComPtr<IStream> memory;
    memory.Attach(SHCreateMemStream(encoded.data(), static_cast<UINT>(encoded.size())));
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICBitmapSource> converted;
    UINT width = 0, height = 0;
    if (!memory || FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
        FAILED(factory->CreateDecoderFromStream(memory.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) ||
        FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&width, &height)) || width == 0 || height == 0 ||
        FAILED(WICConvertBitmapSource(GUID_WICPixelFormat24bppBGR, frame.Get(), &converted))) return false;

    ULONGLONG stride = (static_cast<ULONGLONG>(width) * 24 + 31) / 32 * 4;
    ULONGLONG total = sizeof(BITMAPINFOHEADER) + stride * height;
    if (width > 32767 || height > 32767 || total > 0x20000000) return false;
    std::vector<BYTE> dib(static_cast<size_t>(total), 0);
    BITMAPINFOHEADER *header = reinterpret_cast<BITMAPINFOHEADER *>(dib.data());
    header->biSize = sizeof(BITMAPINFOHEADER);
    header->biWidth = static_cast<LONG>(width);
    header->biHeight = static_cast<LONG>(height);      // bottom-up
    header->biPlanes = 1;
    header->biBitCount = 24;
    header->biCompression = BI_RGB;
    header->biSizeImage = static_cast<DWORD>(stride * height);
    for (UINT y = 0; y < height; y++) {
        WICRect row = { 0, static_cast<INT>(y), static_cast<INT>(width), 1 };
        BYTE *target = dib.data() + sizeof(BITMAPINFOHEADER) + stride * (height - 1 - y);
        if (FAILED(converted->CopyPixels(&row, static_cast<UINT>(stride), static_cast<UINT>(stride), target))) return false;
    }
    return add_format(item, CF_DIB, L"DIB", dib.data(), dib.size());
}

/* Each format is read independently: one unreadable format does not discard the clip. */
template <typename Reader>
static void try_add(Reader &&reader)
{
    try {
        reader();
    } catch (const winrt::hresult_error &) {
    } catch (const std::bad_alloc &) {
    }
}

static DATA_INFO *make_item(const ClipboardHistoryItem &entry)
{
    DATA_INFO *item = data_create_item(nullptr, FALSE, nullptr);
    if (!item) return nullptr;
    FILETIME utc = winrt::clock::to_file_time(entry.Timestamp());
    if (!FileTimeToLocalFileTime(&utc, &item->modified)) item->modified = utc;

    DataPackageView view = entry.Content();
    try_add([&] { if (view.Contains(StandardDataFormats::Text())) add_text(item, view.GetTextAsync().get()); });
    try_add([&] { if (view.Contains(StandardDataFormats::Html())) add_narrow(item, L"HTML Format", view.GetHtmlFormatAsync().get(), CP_UTF8); });
    try_add([&] { if (view.Contains(StandardDataFormats::Rtf())) add_narrow(item, L"Rich Text Format", view.GetRtfAsync().get(), CP_ACP); });
    try_add([&] { if (view.Contains(StandardDataFormats::Bitmap())) add_bitmap(item, view); });
    if (!item->child) {
        data_free(item);
        return nullptr;
    }
    return item;
}

static bool runtime_available()
{
    HMODULE winrt = LoadLibrary(L"api-ms-win-core-winrt-l1-1-0.dll");
    HMODULE winrt_error = LoadLibrary(L"api-ms-win-core-winrt-error-l1-1-1.dll");
    bool available = winrt != nullptr && winrt_error != nullptr;
    if (winrt) FreeLibrary(winrt);
    if (winrt_error) FreeLibrary(winrt_error);
    return available;
}

static void set_error(WINCLIP_RESULT *result, const wchar_t *message)
{
    lstrcpyn(result->error, message, ARRAYSIZE(result->error));
}

static void load_worker(HWND window, int max_items)
{
    WINCLIP_RESULT *result = new(std::nothrow) WINCLIP_RESULT();
    if (!result) {
        PostMessage(window, WM_WINCLIP_LOADED, 0, 0);
        return;
    }
    bool initialized = false;
    if (!runtime_available()) {
        set_error(result, L"The Windows clipboard history requires Windows 10 (version 1809) or later.");
    } else try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        initialized = true;
        ClipboardHistoryItemsResult history = Clipboard::GetHistoryItemsAsync().get();
        if (history.Status() == ClipboardHistoryItemsResultStatus::ClipboardHistoryDisabled) {
            set_error(result, L"Windows clipboard history is turned off.\r\nTurn it on in Settings > System > Clipboard, then try again.");
        } else if (history.Status() != ClipboardHistoryItemsResultStatus::Success) {
            set_error(result, L"Windows denied access to the clipboard history. Bring ClipMaster to the foreground and try again.");
        } else {
            auto entries = history.Items();      // newest first
            uint32_t count = entries.Size();
            if (max_items >= 0 && static_cast<uint32_t>(max_items) < count) count = static_cast<uint32_t>(max_items);
            DATA_INFO **tail = &result->items;
            for (uint32_t index = count; index > 0; index--) {      // append oldest first
                DATA_INFO *item = nullptr;
                try {
                    item = make_item(entries.GetAt(index - 1));
                } catch (const winrt::hresult_error &) {
                }
                if (item) {
                    *tail = item;
                    tail = &item->next;
                }
            }
        }
    } catch (const winrt::hresult_error &error) {
        set_error(result, error.message().c_str());
    } catch (...) {
        set_error(result, L"Could not read the Windows clipboard history.");
    }
    if (initialized) winrt::uninit_apartment();
    if (!PostMessage(window, WM_WINCLIP_LOADED, 0, reinterpret_cast<LPARAM>(result))) winclip_result_free(result);
}

extern "C" BOOL winclip_history_start(HWND window, int max_items)
{
    try {
        std::thread(load_worker, window, max_items).detach();
    } catch (const std::system_error &) {
        return FALSE;
    }
    return TRUE;
}

extern "C" void winclip_result_free(WINCLIP_RESULT *result)
{
    if (!result) return;
    data_free(result->items);
    delete result;
}
