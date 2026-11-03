#define _INC_OLE
#include <windows.h>
#undef _INC_OLE
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <tchar.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <new>
extern "C" {
#include "General.h"
#include "Memory.h"
#include "String.h"
#include "Data.h"
#include "Ini.h"
#include "Format.h"
#include "ClipBoard.h"
#include "DarkMode.h"
extern OPTION_INFO option;
}
#include "PastePlus.h"
#pragma comment(lib, "Crypt32.lib")

struct FormatTile {
    DATA_INFO *format;
    int handler;
    HWND window;
    DWORD byte_size;
    ULONGLONG byte_hash;
};

struct PastePane {
    DATA_INFO *snapshot = nullptr;
    std::vector<FormatTile> tiles;
    HWND information = nullptr;
    HWND status = nullptr;
    int offset = 0;
    int height = 0;
    bool layout = false;
};

struct SymbolButton {
    HWND window;
    wchar_t symbol;
};

struct SymbolsPane {
    std::vector<HWND> headings;
    std::vector<SymbolButton> symbols;
    HWND charmap_button = nullptr;
    HFONT font = nullptr;
    int offset = 0;
    int height = 0;
    bool layout = false;
};

struct CharacterMapWindow {
    HWND window = nullptr;
    DWORD process = 0;
};

struct CharacterMapFontSearch {
    HWND combo = nullptr;
    LPVOID font_name = nullptr;
    int index = CB_ERR;
};

static int pane_scale(HWND window, int value) { return MulDiv(value, GetDpiForWindow(window), 96); }

static ULONGLONG bytes_hash(const std::vector<BYTE> &bytes)
{
    ULONGLONG hash = 14695981039346656037ULL;
    for (BYTE value : bytes) { hash ^= value; hash *= 1099511628211ULL; }
    return hash;
}

static bool decode_multibyte(const BYTE *bytes, DWORD size, UINT codepage, DWORD flags, std::wstring &text)
{
    int characters = MultiByteToWideChar(codepage, flags, reinterpret_cast<const char *>(bytes), static_cast<int>(size), nullptr, 0);
    if (!characters) return false;
    text.resize(characters);
    return MultiByteToWideChar(codepage, flags, reinterpret_cast<const char *>(bytes), static_cast<int>(size), &text[0], characters) != 0;
}

static bool decode_utf16(const BYTE *bytes, DWORD size, std::wstring &text)
{
    if (size < 4) return false;
    DWORD offset = 0;
    bool big_endian = false;
    if (bytes[0] == 0xFF && bytes[1] == 0xFE) offset = 2;
    else if (bytes[0] == 0xFE && bytes[1] == 0xFF) { offset = 2; big_endian = true; }
    else {
        DWORD pairs = size / 2, even_zeroes = 0, odd_zeroes = 0;
        for (DWORD index = 0; index < pairs; index++) {
            even_zeroes += bytes[index * 2] == 0;
            odd_zeroes += bytes[index * 2 + 1] == 0;
        }
        if (max(even_zeroes, odd_zeroes) * 5 < pairs * 3) return false;
        big_endian = even_zeroes > odd_zeroes;
    }
    DWORD characters = min((size - offset) / 2, 32768UL);
    text.clear();
    text.reserve(characters);
    for (DWORD index = 0; index < characters; index++) {
        BYTE first = bytes[offset + index * 2], second = bytes[offset + index * 2 + 1];
        wchar_t character = static_cast<wchar_t>(big_endian ? (first << 8) | second : first | (second << 8));
        if (!character) break;
        text.push_back(character);
    }
    return !text.empty();
}

static bool readable_text(const std::wstring &text)
{
    if (text.empty()) return false;
    size_t printable = 0;
    for (wchar_t character : text) {
        if (character == L'\r' || character == L'\n' || character == L'\t' ||
            (character >= 0x20 && (character < 0x7F || character > 0x9F))) printable++;
    }
    return printable * 5 >= text.size() * 4;
}

static std::wstring escape_controls(const std::wstring &text)
{
    std::wstring result;
    wchar_t value[8];
    for (wchar_t character : text) {
        if (character == L'\r' || character == L'\n' || character == L'\t' ||
            (character >= 0x20 && (character < 0x7F || character > 0x9F))) result += character;
        else {
            wsprintf(value, character <= 0xFF ? L"\\x%02X" : L"\\u%04X", static_cast<unsigned int>(character));
            result += value;
        }
    }
    return result;
}

static bool format_is_text(DATA_INFO *format)
{
    return format->format == CF_TEXT || format->format == CF_UNICODETEXT ||
        format->format == CF_OEMTEXT || format->format == CF_HDROP ||
        (format->format_name && (StrStrI(format->format_name, L"text") ||
         StrStrI(format->format_name, L"html") || StrStrI(format->format_name, L"rtf") ||
         StrStrI(format->format_name, L"rich") || StrStrI(format->format_name, L"csv") ||
         StrStrI(format->format_name, L"json") || StrStrI(format->format_name, L"xml") ||
         StrStrI(format->format_name, L"url") || StrStrI(format->format_name, L"uri") ||
         StrStrI(format->format_name, L"svg") || StrStrI(format->format_name, L"markdown")));
}

static bool format_is_unavailable(DATA_INFO *format)
{
    return !format->data || format->format == CF_LOCALE || format->format == CF_PALETTE ||
        format->format == CF_OWNERDISPLAY ||
        (format->format >= CF_DSPTEXT && format->format <= CF_DSPENHMETAFILE) ||
        (format->format >= CF_PRIVATEFIRST && format->format <= CF_PRIVATELAST) ||
        (format->format_name && (StrStrI(format->format_name, L"Ole Private") ||
         StrStrI(format->format_name, L"DataObject") || StrStrI(format->format_name, L"DropEffect") ||
         !lstrcmpi(format->format_name, L"OleClipboardPersistOnFlush") ||
         !lstrcmpi(format->format_name, L"OneNote 2010 Internal")));
}

static bool target_matches(const wchar_t *targets, DATA_INFO *format);

static bool format_has_handler(DATA_INFO *format)
{
    for (int index = 0; index < option.config_handler_count; index++)
        if (*option.config_handlers[index].formats && *option.config_handlers[index].script &&
            target_matches(option.config_handlers[index].formats, format)) return true;
    return false;
}

static std::wstring format_text(DATA_INFO *format, const std::vector<BYTE> *cached_bytes = nullptr,
    bool raw_binary = false)
{
    wchar_t image[128];
    if (!raw_binary && format->format == CF_BITMAP && format->data) {
        BITMAP bitmap;
        if (GetObject(format->data, sizeof(bitmap), &bitmap)) {
            wsprintf(image, L"Bitmap: %ld x %ld, %u bpp", bitmap.bmWidth, bitmap.bmHeight, bitmap.bmBitsPixel);
            return image;
        }
    }
    if (!raw_binary && (format->format == CF_DIB || format->format == CF_DIBV5) && format->data) {
        BYTE *dib = static_cast<BYTE *>(GlobalLock(format->data));
        if (dib) {
            DWORD available = min(GlobalSize(format->data), format->size);
            if (available >= sizeof(BITMAPINFOHEADER)) {
                BITMAPINFOHEADER *header = reinterpret_cast<BITMAPINFOHEADER *>(dib);
                if (header->biSize >= sizeof(BITMAPINFOHEADER) && header->biSize <= available) {
                    wsprintf(image, L"DIB: %ld x %ld, %u bpp", header->biWidth,
                        header->biHeight < 0 ? -header->biHeight : header->biHeight, header->biBitCount);
                    GlobalUnlock(format->data);
                    return image;
                }
            }
            GlobalUnlock(format->data);
        }
    }
    if (!raw_binary && format->format == CF_ENHMETAFILE && format->data) {
        ENHMETAHEADER header = {};
        if (GetEnhMetaFileHeader(static_cast<HENHMETAFILE>(format->data), sizeof(header), &header)) {
            wsprintf(image, L"Enhanced metafile: %ld x %ld, %u bytes",
                header.rclBounds.right - header.rclBounds.left, header.rclBounds.bottom - header.rclBounds.top,
                GetEnhMetaFileBits(static_cast<HENHMETAFILE>(format->data), 0, nullptr));
            return image;
        }
    }
    if (!raw_binary && format->format == CF_METAFILEPICT && format->data) {
        METAFILEPICT *picture = static_cast<METAFILEPICT *>(GlobalLock(format->data));
        if (picture) {
            wsprintf(image, L"Metafile picture: %ld x %ld, %u bytes", picture->xExt, picture->yExt,
                GetMetaFileBitsEx(picture->hMF, 0, nullptr));
            GlobalUnlock(format->data);
            return image;
        }
    }
    if (!raw_binary && !format_is_text(format)) {
        wsprintf(image, L"Binary data: %u bytes", format->size);
        return image;
    }
    if (format->format == CF_UNICODETEXT && format->data) {
        const wchar_t *text = static_cast<const wchar_t *>(GlobalLock(format->data));
        if (text) {
            size_t available = min(GlobalSize(format->data), format->size) / sizeof(wchar_t);
            size_t length = 0;
            while (length < available && length < 65536 && text[length]) length++;
            std::wstring result(text, length);
            GlobalUnlock(format->data);
            return result;
        }
    }
    if (format->format == CF_HDROP && format->data) {
        std::wstring result;
        UINT count = DragQueryFile(static_cast<HDROP>(format->data), 0xFFFFFFFF, nullptr, 0);
        for (UINT index = 0; index < count; index++) {
            wchar_t path[32768];
            if (DragQueryFile(static_cast<HDROP>(format->data), index, path, ARRAYSIZE(path))) result += std::wstring(path) + L"\r\n";
        }
        return result;
    }
    DWORD size = 0;
    BYTE *allocated = nullptr;
    const BYTE *bytes = nullptr;
    if (cached_bytes && !cached_bytes->empty()) {
        bytes = cached_bytes->data();
        size = static_cast<DWORD>(cached_bytes->size());
    } else {
        allocated = format_data_to_bytes(format, &size);
        if (!allocated) allocated = clipboard_data_to_bytes(format, &size);
        bytes = allocated;
    }
    if (!bytes) return L"";
    std::wstring result;
    if (raw_binary && !format_is_text(format)) {
        wchar_t value[8];
        DWORD display_size = min(size, 2048UL);
        for (DWORD index = 0; index < display_size; index++) {
            wsprintf(value, L"%02X%s", bytes[index], (index + 1) % 16 ? L" " : L"\r\n");
            result += value;
        }
        if (size > display_size) result += L"\r\n[truncated]";
        mem_free(reinterpret_cast<void **>(&allocated));
        return result;
    }
    DWORD length = min(size, 65536UL);
    while (length && bytes[length - 1] == 0) length--;
    std::wstring decoded;
    bool named_text = format->format_name && (StrStrI(format->format_name, L"text") ||
        StrStrI(format->format_name, L"html") || StrStrI(format->format_name, L"rtf") ||
        StrStrI(format->format_name, L"rich") || StrStrI(format->format_name, L"csv") ||
        StrStrI(format->format_name, L"json") || StrStrI(format->format_name, L"xml") ||
        StrStrI(format->format_name, L"url") || StrStrI(format->format_name, L"uri") ||
        StrStrI(format->format_name, L"svg") || StrStrI(format->format_name, L"markdown"));
    bool standard_text = format->format == CF_TEXT || format->format == CF_OEMTEXT;
    bool utf16 = decode_utf16(bytes, length, decoded);
    if (!utf16 && length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
        decode_multibyte(bytes + 3, length - 3, CP_UTF8, MB_ERR_INVALID_CHARS, decoded);
    if (decoded.empty() && !standard_text && !utf16)
        decode_multibyte(bytes, length, CP_UTF8, MB_ERR_INVALID_CHARS, decoded);
    UINT codepage = format->format == CF_OEMTEXT ? CP_OEMCP : CP_ACP;
    if (decoded.empty() && length) decode_multibyte(bytes, length, codepage, 0, decoded);
    if (named_text || readable_text(decoded)) {
        result = escape_controls(decoded);
        if (size > length && length == 65536) result += L"\r\n[truncated]";
    } else {
        wsprintf(image, L"Binary data: %u bytes", size);
        result = image;
    }
    mem_free(reinterpret_cast<void **>(&allocated));
    return result;
}

static std::wstring quote_ps(const wchar_t *text)
{
    std::wstring result = L"'";
    for (; *text; text++) { result += *text; if (*text == L'\'') result += L'\''; }
    return result + L"'";
}

static bool powershell_transform(const wchar_t *pattern, const wchar_t *script, const std::wstring &input,
    std::wstring &output, std::wstring &error)
{
    SECURITY_ATTRIBUTES security = { sizeof(security), nullptr, TRUE };
    wchar_t temporary[MAX_PATH], input_path[MAX_PATH], output_path[MAX_PATH], executable[MAX_PATH];
    HANDLE source = INVALID_HANDLE_VALUE, destination = INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION process = {};
    STARTUPINFO startup = {};
    DWORD written, exit_code = 1;
    bool success = false;
    GetTempPath(ARRAYSIZE(temporary), temporary);
    if (!GetTempFileName(temporary, L"cmi", 0, input_path)) return false;
    if (!GetTempFileName(temporary, L"cmo", 0, output_path)) { DeleteFile(input_path); return false; }
    source = CreateFile(input_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE,
        &security, OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    destination = CreateFile(output_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE,
        &security, OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (source != INVALID_HANDLE_VALUE && destination != INVALID_HANDLE_VALUE) {
        int length = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
        std::string utf8(length, '\0');
        if (length) WideCharToMultiByte(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), &utf8[0], length, nullptr, nullptr);
        if (WriteFile(source, utf8.data(), length, &written, nullptr) && written == static_cast<DWORD>(length)) {
            SetFilePointer(source, 0, nullptr, FILE_BEGIN);
            std::wstring command = L"$ErrorActionPreference='Stop';[Console]::InputEncoding=[Text.UTF8Encoding]::new($false);"
                L"[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false);$clip=[Console]::In.ReadToEnd();";
            if (*pattern) command += L"$rx=[regex]::new(" + quote_ps(pattern) + L",[Text.RegularExpressions.RegexOptions]::None,[timespan]::FromMilliseconds(250));if(!$rx.IsMatch($clip)){exit 2};";
            command += L"& { " + std::wstring(script) + L" } | ForEach-Object { [Console]::WriteLine([string]$_) };";
            DWORD encoded_length = 0;
            CryptBinaryToString(reinterpret_cast<const BYTE *>(command.data()), static_cast<DWORD>(command.size() * sizeof(wchar_t)),
                CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &encoded_length);
            std::wstring encoded(encoded_length, L'\0');
            CryptBinaryToString(reinterpret_cast<const BYTE *>(command.data()), static_cast<DWORD>(command.size() * sizeof(wchar_t)),
                CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &encoded[0], &encoded_length);
            encoded.resize(wcslen(encoded.c_str()));
            GetSystemDirectory(executable, ARRAYSIZE(executable));
            std::wstring executable_path = std::wstring(executable) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
            std::wstring arguments = L"\"" + executable_path + L"\" -NoLogo -NoProfile -NonInteractive -EncodedCommand " + encoded;
            startup.cb = sizeof(startup);
            startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            startup.wShowWindow = SW_HIDE;
            startup.hStdInput = source;
            startup.hStdOutput = destination;
            startup.hStdError = destination;
            if (CreateProcess(executable_path.c_str(), &arguments[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                nullptr, temporary, &startup, &process)) {
                HANDLE job = CreateJobObject(nullptr, nullptr);
                if (job) {
                    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
                    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
                    AssignProcessToJobObject(job, process.hProcess);
                }
                DWORD wait = WaitForSingleObject(process.hProcess, 15000);
                if (wait != WAIT_OBJECT_0) { TerminateProcess(process.hProcess, 1); WaitForSingleObject(process.hProcess, 5000); }
                GetExitCodeProcess(process.hProcess, &exit_code);
                LARGE_INTEGER size;
                if (wait == WAIT_OBJECT_0 && exit_code == 0 && GetFileSizeEx(destination, &size) && size.QuadPart <= 8388608) {
                    std::string bytes(static_cast<size_t>(size.QuadPart), '\0');
                    SetFilePointer(destination, 0, nullptr, FILE_BEGIN);
                    DWORD read = 0;
                    if (ReadFile(destination, bytes.empty() ? nullptr : &bytes[0], static_cast<DWORD>(bytes.size()), &read, nullptr)) {
                        int characters = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), read, nullptr, 0);
                        output.resize(characters);
                        if (characters) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), read, &output[0], characters);
                        if (output.size() >= 2 && output.substr(output.size() - 2) == L"\r\n") output.resize(output.size() - 2);
                        success = characters != 0 || read == 0;
                    }
                }
                if (job) CloseHandle(job);
                CloseHandle(process.hThread);
                CloseHandle(process.hProcess);
                if (wait != WAIT_OBJECT_0) error = L"Handler timed out.";
                else if (exit_code == 2) error = L"The condition did not match.";
                else if (!success) error = L"Handler failed or returned invalid output.";
            } else error = L"Could not start Windows PowerShell.";
        }
    }
    if (source != INVALID_HANDLE_VALUE) CloseHandle(source);
    if (destination != INVALID_HANDLE_VALUE) CloseHandle(destination);
    DeleteFile(input_path);
    DeleteFile(output_path);
    if (!success && error.empty()) error = L"Could not prepare the handler.";
    return success;
}

static bool target_matches(const wchar_t *targets, DATA_INFO *format)
{
    std::wstring list = targets;
    size_t start = 0;
    for (;;) {
        size_t end = list.find_first_of(L",;|", start);
        std::wstring name = list.substr(start, end == std::wstring::npos ? end : end - start);
        while (!name.empty() && iswspace(name.front())) name.erase(name.begin());
        while (!name.empty() && iswspace(name.back())) name.pop_back();
        if ((!name.empty() && format->format_name && _wcsicmp(name.c_str(), format->format_name) == 0) ||
            (_wcsicmp(name.c_str(), L"CF_TEXT") == 0 && format->format == CF_TEXT) ||
            ((_wcsicmp(name.c_str(), L"CF_OEMTEXT") == 0 || _wcsicmp(name.c_str(), L"CT_OEMTEXT") == 0) && format->format == CF_OEMTEXT) ||
            ((_wcsicmp(name.c_str(), L"CF_UNICODETEXT") == 0 || _wcsicmp(name.c_str(), L"CF_UNICODE") == 0) && format->format == CF_UNICODETEXT) ||
            (_wcsicmp(name.c_str(), L"CF_DIB") == 0 && format->format == CF_DIB) ||
            (_wcsicmp(name.c_str(), L"CF_HDROP") == 0 && format->format == CF_HDROP)) return true;
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return false;
}

static void pane_clear(PastePane *pane)
{
    for (auto &tile : pane->tiles) DestroyWindow(tile.window);
    pane->tiles.clear();
    data_free(pane->snapshot);
    pane->snapshot = nullptr;
}

static LRESULT CALLBACK format_tile_proc(HWND window, UINT message, WPARAM parameter, LPARAM data,
    UINT_PTR identifier, DWORD_PTR reference)
{
    UNREFERENCED_PARAMETER(identifier);
    if (message == WM_LBUTTONUP) {
        HWND pane = reinterpret_cast<HWND>(reference);
        HWND tile = GetParent(window);
        return SendMessage(pane, WM_COMMAND, GetDlgCtrlID(tile), 0);
    }
    return DefSubclassProc(window, message, parameter, data);
}

static HWND pane_control(HWND owner, const wchar_t *type, const wchar_t *text, DWORD style, int id)
{
    HWND control = CreateWindowEx(lstrcmp(type, L"EDIT") == 0 ? WS_EX_CLIENTEDGE : 0, type, text,
        WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, owner, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        reinterpret_cast<HINSTANCE>(GetWindowLongPtr(owner, GWLP_HINSTANCE)), nullptr);
    SendMessage(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    return control;
}

static const wchar_t *symbol_names[] = {
    L"Common Symbols", L"Arrows", L"Mathematics", L"Currency", L"Greek", L"Box Drawing"
};

static const wchar_t *symbol_sets[] = {
    L"\x00A9\x00AE\x2122\x2022\x2026\x00B0\x00A7\x00B6\x2713\x2717\x2605\x2606\x2665\x2666\x2663\x2660\x260E\x2709\x2611\x2610\x203B\x2030\x2116\x00B7",
    L"\x2190\x2191\x2192\x2193\x2194\x2195\x2196\x2197\x2198\x2199\x21D0\x21D1\x21D2\x21D3\x21D4\x21B5\x21BA\x21BB",
    L"\x00B1\x00D7\x00F7\x2260\x2248\x2264\x2265\x221E\x221A\x2211\x220F\x222B\x2202\x2206\x2208\x2209\x2205\x2229\x222A\x2282\x2283\x2200\x2203\x2220",
    L"$\x20AC\x00A3\x00A5\x20A9\x20B9\x20BD\x00A2\x20AB\x20BA\x20BF\x20B1",
    L"\x03B1\x03B2\x03B3\x03B4\x03B5\x03B6\x03B7\x03B8\x03B9\x03BA\x03BB\x03BC\x03BD\x03BE\x03BF\x03C0\x03C1\x03C3\x03C4\x03C5\x03C6\x03C7\x03C8\x03C9",
    L"\x2500\x2502\x250C\x2510\x2514\x2518\x251C\x2524\x252C\x2534\x253C\x2550\x2551\x2554\x2557\x255A\x255D\x2560\x2563\x2566\x2569\x256C"
};

static void pane_text_paste(HWND window, const std::wstring &text);

static BOOL CALLBACK charmap_window_proc(HWND window, LPARAM reference)
{
    CharacterMapWindow *search = reinterpret_cast<CharacterMapWindow *>(reference);
    wchar_t module[MAX_PATH];
    if (GetWindowModuleFileName(window, module, ARRAYSIZE(module)) == 0 ||
        lstrcmpi(PathFindFileName(module), L"charmap.exe") != 0) return TRUE;
    search->window = window;
    GetWindowThreadProcessId(window, &search->process);
    return FALSE;
}

static BOOL CALLBACK charmap_font_combo_proc(HWND control, LPARAM reference)
{
    CharacterMapFontSearch *search = reinterpret_cast<CharacterMapFontSearch *>(reference);
    wchar_t class_name[32];
    DWORD_PTR index = CB_ERR;
    if (GetClassName(control, class_name, ARRAYSIZE(class_name)) == 0 ||
        lstrcmpi(class_name, L"ComboBox") != 0 ||
        !SendMessageTimeout(control, CB_FINDSTRINGEXACT, (WPARAM)-1,
            reinterpret_cast<LPARAM>(search->font_name), SMTO_ABORTIFHUNG, 1000, &index) ||
        static_cast<LRESULT>(index) == CB_ERR) return TRUE;
    search->combo = control;
    search->index = static_cast<int>(index);
    return FALSE;
}

static BOOL charmap_select_wingdings(HWND window, const DWORD process_id)
{
    const wchar_t font_name[] = L"Wingdings";
    HANDLE process = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, process_id);
    LPVOID remote_name;
    SIZE_T written = 0;
    BOOL selected = FALSE;
    if (!process) return FALSE;
    remote_name = VirtualAllocEx(process, nullptr, sizeof(font_name), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remote_name && WriteProcessMemory(process, remote_name, font_name, sizeof(font_name), &written) &&
        written == sizeof(font_name)) {
        CharacterMapFontSearch search = { nullptr, remote_name, CB_ERR };
        EnumChildWindows(window, charmap_font_combo_proc, reinterpret_cast<LPARAM>(&search));
        if (search.combo && SendMessage(search.combo, CB_SETCURSEL, search.index, 0) != CB_ERR) {
            SendMessage(GetParent(search.combo), WM_COMMAND,
                MAKEWPARAM(GetDlgCtrlID(search.combo), CBN_SELCHANGE), reinterpret_cast<LPARAM>(search.combo));
            selected = TRUE;
        }
    }
    if (remote_name) VirtualFreeEx(process, remote_name, 0, MEM_RELEASE);
    CloseHandle(process);
    return selected;
}

static void symbols_open_character_map(HWND owner)
{
    CharacterMapWindow map = {};
    SHELLEXECUTEINFO execute = {};
    wchar_t executable[MAX_PATH];
    UINT length;
    EnumWindows(charmap_window_proc, reinterpret_cast<LPARAM>(&map));
    if (!map.window) {
        length = GetSystemDirectory(executable, ARRAYSIZE(executable));
        if (!length || length >= ARRAYSIZE(executable) || !PathAppend(executable, L"charmap.exe")) return;
        execute.cbSize = sizeof(execute);
        execute.fMask = SEE_MASK_NOCLOSEPROCESS;
        execute.lpVerb = L"open";
        execute.lpFile = executable;
        execute.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteEx(&execute)) {
            MessageBox(owner, L"Windows Character Map could not be opened.", L"ClipMaster", MB_OK | MB_ICONERROR);
            return;
        }
        if (execute.hProcess) {
            WaitForInputIdle(execute.hProcess, 5000);
            CloseHandle(execute.hProcess);
        }
        EnumWindows(charmap_window_proc, reinterpret_cast<LPARAM>(&map));
    }
    if (map.window) {
        charmap_select_wingdings(map.window, map.process);
        ShowWindow(map.window, SW_SHOW);
        SetForegroundWindow(map.window);
    }
}

static LRESULT CALLBACK symbol_button_proc(HWND window, UINT message, WPARAM parameter, LPARAM data,
    UINT_PTR identifier, DWORD_PTR reference)
{
    UNREFERENCED_PARAMETER(identifier);
    UNREFERENCED_PARAMETER(reference);
    if (message == WM_MOUSEWHEEL) return SendMessage(GetParent(window), message, parameter, data);
    return DefSubclassProc(window, message, parameter, data);
}

static void symbols_layout(HWND window, SymbolsPane *pane)
{
    if (pane->layout) return;
    pane->layout = true;
    RECT client;
    GetClientRect(window, &client);
    int margin = pane_scale(window, 16), gap = pane_scale(window, 6), size = pane_scale(window, 66);
    int available = max(1, client.right - margin * 2);
    int columns = max(1, (available + gap) / (size + gap));
    int top = margin;
    size_t symbol_index = 0;
    for (int category = 0; category < ARRAYSIZE(symbol_sets); category++) {
        MoveWindow(pane->headings[category], margin, top - pane->offset, available, pane_scale(window, 28), TRUE);
        top += pane_scale(window, 34);
        int count = lstrlen(symbol_sets[category]);
        for (int index = 0; index < count; index++, symbol_index++) {
            int left = margin + index % columns * (size + gap);
            MoveWindow(pane->symbols[symbol_index].window, left,
                top + index / columns * (size + gap) - pane->offset,
                max(1, min(size, client.right - margin - left)), size, TRUE);
        }
        top += ((count + columns - 1) / columns) * (size + gap) + margin;
    }
    int charmap_gap = pane_scale(window, 20), charmap_height = pane_scale(window, 40);
    MoveWindow(pane->charmap_button, margin, top + charmap_gap - pane->offset, available, charmap_height, TRUE);
    pane->height = top + charmap_gap + charmap_height + margin;
    SCROLLINFO scroll = { sizeof(scroll), SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL };
    scroll.nMax = max(0, pane->height - 1);
    scroll.nPage = client.bottom;
    scroll.nPos = pane->offset;
    SetScrollInfo(window, SB_VERT, &scroll, TRUE);
    int adjusted = GetScrollPos(window, SB_VERT);
    pane->layout = false;
    if (adjusted != pane->offset) { pane->offset = adjusted; symbols_layout(window, pane); }
}

static void symbols_set_font(HWND window, SymbolsPane *pane)
{
    HFONT font = CreateFont(-MulDiv(24, GetDpiForWindow(window), 72), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
    if (!font) return;
    for (const SymbolButton &button : pane->symbols)
        SendMessage(button.window, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    if (pane->font) DeleteObject(pane->font);
    pane->font = font;
}

static LRESULT CALLBACK symbols_proc(HWND window, UINT message, WPARAM parameter, LPARAM data)
{
    SymbolsPane *pane = reinterpret_cast<SymbolsPane *>(GetWindowLongPtr(window, GWLP_USERDATA));
    if (message == WM_CREATE) {
        pane = new(std::nothrow) SymbolsPane;
        if (!pane) return -1;
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pane));
        symbols_set_font(window, pane);
        for (int category = 0; category < ARRAYSIZE(symbol_sets); category++) {
            pane->headings.push_back(pane_control(window, L"STATIC", symbol_names[category], SS_LEFT | SS_CENTERIMAGE,
                3000 + category));
            for (int index = 0; symbol_sets[category][index]; index++) {
                wchar_t label[] = { symbol_sets[category][index], 0 };
                HWND button = pane_control(window, L"BUTTON", label, BS_PUSHBUTTON | WS_TABSTOP,
                    2000 + static_cast<int>(pane->symbols.size()));
                if (pane->font) SendMessage(button, WM_SETFONT, reinterpret_cast<WPARAM>(pane->font), TRUE);
                RECT text_margin = {};
                SendMessage(button, BCM_SETTEXTMARGIN, 0, reinterpret_cast<LPARAM>(&text_margin));
                SetWindowSubclass(button, symbol_button_proc, 1, 0);
                pane->symbols.push_back({ button, symbol_sets[category][index] });
            }
        }
        pane->charmap_button = pane_control(window, L"BUTTON", L"Open Character Map (default Wingdings)",
            BS_OWNERDRAW | WS_TABSTOP, 4000);
        return 0;
    }
    if (!pane) return DefWindowProc(window, message, parameter, data);
    switch (message) {
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *draw = reinterpret_cast<DRAWITEMSTRUCT *>(data);
        if (draw && draw->CtlID == 4000) {
            COLORREF background = (draw->itemState & ODS_SELECTED) ? RGB(190, 83, 25) : RGB(226, 116, 38);
            HBRUSH fill = CreateSolidBrush(background);
            HBRUSH border = CreateSolidBrush(RGB(164, 73, 24));
            RECT text = draw->rcItem;
            int saved = SaveDC(draw->hDC);
            FillRect(draw->hDC, &draw->rcItem, fill);
            FrameRect(draw->hDC, &draw->rcItem, border);
            InflateRect(&text, -pane_scale(window, 8), 0);
            SetBkMode(draw->hDC, TRANSPARENT);
            SetTextColor(draw->hDC, (draw->itemState & ODS_DISABLED) ? GetSysColor(COLOR_GRAYTEXT) : RGB(255, 255, 255));
            SelectObject(draw->hDC, GetStockObject(DEFAULT_GUI_FONT));
            DrawText(draw->hDC, L"Open Character Map (default Wingdings)", -1, &text,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            if (draw->itemState & ODS_FOCUS) {
                InflateRect(&text, -pane_scale(window, 2), -pane_scale(window, 2));
                DrawFocusRect(draw->hDC, &text);
            }
            RestoreDC(draw->hDC, saved);
            DeleteObject(fill);
            DeleteObject(border);
            return TRUE;
        }
        break;
    }
    case WM_SIZE:
        symbols_layout(window, pane); return 0;
    case WM_DPICHANGED_AFTERPARENT:
        symbols_set_font(window, pane);
        symbols_layout(window, pane);
        return 0;
    case WM_MOUSEWHEEL:
    case WM_VSCROLL: {
        SCROLLINFO scroll = { sizeof(scroll), SIF_ALL };
        GetScrollInfo(window, SB_VERT, &scroll);
        if (message == WM_MOUSEWHEEL) scroll.nPos -= GET_WHEEL_DELTA_WPARAM(parameter) / WHEEL_DELTA * pane_scale(window, 72);
        else switch (LOWORD(parameter)) {
            case SB_LINEUP: scroll.nPos -= pane_scale(window, 32); break;
            case SB_LINEDOWN: scroll.nPos += pane_scale(window, 32); break;
            case SB_PAGEUP: scroll.nPos -= scroll.nPage; break;
            case SB_PAGEDOWN: scroll.nPos += scroll.nPage; break;
            case SB_THUMBTRACK: scroll.nPos = scroll.nTrackPos; break;
            case SB_TOP: scroll.nPos = 0; break;
            case SB_BOTTOM: scroll.nPos = scroll.nMax; break;
        }
        scroll.fMask = SIF_POS;
        SetScrollInfo(window, SB_VERT, &scroll, TRUE);
        pane->offset = GetScrollPos(window, SB_VERT);
        symbols_layout(window, pane);
        return 0;
    }
    case WM_COMMAND: {
        int identifier = LOWORD(parameter);
        if (identifier == 4000) {
            symbols_open_character_map(window);
        } else if (identifier >= 2000 && identifier < 2000 + static_cast<int>(pane->symbols.size())) {
            wchar_t symbol[] = { pane->symbols[identifier - 2000].symbol, 0 };
            pane_text_paste(window, symbol);
        }
        return 0;
    }
    case WM_NCDESTROY:
        if (pane->font) DeleteObject(pane->font);
        delete pane;
        SetWindowLongPtr(window, GWLP_USERDATA, 0);
        break;
    }
    return DefWindowProc(window, message, parameter, data);
}

static void pane_layout(HWND window, PastePane *pane)
{
    if (pane->layout) return;
    pane->layout = true;
    RECT client;
    GetClientRect(window, &client);
    int margin = pane_scale(window, 12), gap = pane_scale(window, 12), size = pane_scale(window, 384);
    int width = max(1, min(size, client.right - margin * 2));
    int columns = max(1, (client.right - margin * 2 + gap) / (size + gap));
    MoveWindow(pane->information, margin, margin - pane->offset, max(1, client.right - margin * 2), pane_scale(window, 48), TRUE);
    MoveWindow(pane->status, margin, pane_scale(window, 64) - pane->offset, max(1, client.right - margin * 2), pane_scale(window, 36), TRUE);
    for (size_t index = 0; index < pane->tiles.size(); index++) {
        int left = margin + static_cast<int>(index % columns) * (size + gap);
        int top = pane_scale(window, 108) + static_cast<int>(index / columns) * (size + gap) - pane->offset;
        HWND tile = pane->tiles[index].window;
        MoveWindow(tile, left, top, width, size, TRUE);
        MoveWindow(GetDlgItem(tile, 1), pane_scale(window, 8), pane_scale(window, 8), width - pane_scale(window, 16), pane_scale(window, 28), TRUE);
        MoveWindow(GetDlgItem(tile, 2), pane_scale(window, 8), pane_scale(window, 44), width - pane_scale(window, 16), size - pane_scale(window, 56), TRUE);
    }
    int top = pane_scale(window, 120) + static_cast<int>((pane->tiles.size() + columns - 1) / columns) * (size + gap);
    pane->height = top + margin;
    SCROLLINFO scroll = { sizeof(scroll), SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL };
    scroll.nMax = max(0, pane->height - 1);
    scroll.nPage = client.bottom;
    scroll.nPos = pane->offset;
    SetScrollInfo(window, SB_VERT, &scroll, TRUE);
    int adjusted = GetScrollPos(window, SB_VERT);
    pane->layout = false;
    if (adjusted != pane->offset) { pane->offset = adjusted; pane_layout(window, pane); }
}

static void pane_text_paste(HWND window, const std::wstring &text)
{
    if (text.empty()) return;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
    void *destination = memory ? GlobalLock(memory) : nullptr;
    if (!destination) { if (memory) GlobalFree(memory); return; }
    CopyMemory(destination, text.c_str(), (text.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(memory);
    DATA_INFO *format = data_create_data(CF_UNICODETEXT, const_cast<wchar_t *>(L"UNICODE TEXT"), memory,
        static_cast<DWORD>((text.size() + 1) * sizeof(wchar_t)), FALSE, nullptr);
    if (!format) { GlobalFree(memory); return; }
    SendMessage(GetParent(window), WM_PASTEPLUS_PASTE, 0, reinterpret_cast<LPARAM>(format));
    data_free(format);
}

static LRESULT CALLBACK pane_proc(HWND window, UINT message, WPARAM parameter, LPARAM data)
{
    PastePane *pane = reinterpret_cast<PastePane *>(GetWindowLongPtr(window, GWLP_USERDATA));
    if (message == WM_CREATE) {
        pane = new(std::nothrow) PastePane;
        if (!pane) return -1;
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pane));
        pane->information = pane_control(window, L"STATIC", L"", SS_LEFT, 100);
        pane->status = pane_control(window, L"STATIC", L"", SS_LEFT, 101);
        return 0;
    }
    if (!pane) return DefWindowProc(window, message, parameter, data);
    switch (message) {
    case WM_SIZE:
    case WM_DPICHANGED_AFTERPARENT: pane_layout(window, pane); return 0;
    case WM_MOUSEWHEEL:
    case WM_VSCROLL: {
        SCROLLINFO scroll = { sizeof(scroll), SIF_ALL };
        GetScrollInfo(window, SB_VERT, &scroll);
        if (message == WM_MOUSEWHEEL) scroll.nPos -= GET_WHEEL_DELTA_WPARAM(parameter) / WHEEL_DELTA * pane_scale(window, 72);
        else switch (LOWORD(parameter)) {
            case SB_LINEUP: scroll.nPos -= pane_scale(window, 32); break;
            case SB_LINEDOWN: scroll.nPos += pane_scale(window, 32); break;
            case SB_PAGEUP: scroll.nPos -= scroll.nPage; break;
            case SB_PAGEDOWN: scroll.nPos += scroll.nPage; break;
            case SB_THUMBTRACK: scroll.nPos = scroll.nTrackPos; break;
            case SB_TOP: scroll.nPos = 0; break;
            case SB_BOTTOM: scroll.nPos = scroll.nMax; break;
        }
        scroll.fMask = SIF_POS;
        SetScrollInfo(window, SB_VERT, &scroll, TRUE);
        pane->offset = GetScrollPos(window, SB_VERT);
        pane_layout(window, pane);
        return 0;
    }
    case WM_COMMAND: {
        int identifier = LOWORD(parameter);
        if (identifier >= 1000 && identifier < 1000 + static_cast<int>(pane->tiles.size())) {
            FormatTile &tile = pane->tiles[identifier - 1000];
            if (tile.handler < 0) SendMessage(GetParent(window), WM_PASTEPLUS_PASTE, 0, reinterpret_cast<LPARAM>(tile.format));
            else {
                std::wstring output, error;
                SetWindowText(pane->status, L"Processing...");
                UpdateWindow(pane->status);
                HCURSOR previous = SetCursor(LoadCursor(nullptr, IDC_WAIT));
                bool result = powershell_transform(option.config_handlers[tile.handler].pattern,
                    option.config_handlers[tile.handler].script, format_text(tile.format, nullptr, true), output, error);
                SetCursor(previous);
                SetWindowText(pane->status, result ? L"" : error.c_str());
                if (result) pane_text_paste(window, output);
            }
        }
        return 0;
    }
    case WM_NCDESTROY:
        pane_clear(pane);
        delete pane;
        SetWindowLongPtr(window, GWLP_USERDATA, 0);
        break;
    }
    return DefWindowProc(window, message, parameter, data);
}

static void pane_add_tile(HWND window, PastePane *pane, DATA_INFO *format, int handler,
    const std::vector<BYTE> &bytes)
{
    int identifier = 1000 + static_cast<int>(pane->tiles.size());
    HWND tile = pane_control(window, L"STATIC", L"", WS_BORDER | WS_CLIPCHILDREN, identifier);
    wchar_t title[256];
    if (handler >= 0) wsprintf(title, L"PH%d - %s", handler + 1, format->format_name);
    else lstrcpyn(title, format->format_name ? format->format_name : L"Format", ARRAYSIZE(title));
    HWND label = pane_control(tile, L"STATIC", title, SS_LEFT, 1);
    HWND preview = pane_control(tile, L"EDIT", format_text(format, &bytes).c_str(),
        ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_TABSTOP, 2);
    SetWindowSubclass(label, format_tile_proc, 1, reinterpret_cast<DWORD_PTR>(window));
    SetWindowSubclass(preview, format_tile_proc, 1, reinterpret_cast<DWORD_PTR>(window));
    FormatTile entry = { format, handler, tile, static_cast<DWORD>(bytes.size()), bytes.empty() ? 0 : bytes_hash(bytes) };
    pane->tiles.push_back(std::move(entry));
}

extern "C" void pasteplus_refresh(HWND window, DATA_INFO *item)
{
    PastePane *pane = reinterpret_cast<PastePane *>(GetWindowLongPtr(window, GWLP_USERDATA));
    if (!pane) return;
    pane_clear(pane);
    pane->snapshot = item ? data_create_item(item->title, FALSE, nullptr) : nullptr;
    if (pane->snapshot) {
        pane->snapshot->modified = item->modified;
        DATA_INFO **tail = &pane->snapshot->child;
        for (DATA_INFO *source = item->child; source; source = source->next) {
            if (format_is_unavailable(source) && !format_has_handler(source)) continue;
            DATA_INFO *copy = data_item_copy(source, FALSE, FALSE, nullptr);
            if (!copy) continue;
            *tail = copy;
            tail = &copy->next;
        }
    }
    int total = 0, duplicate = 0, unavailable = 0, pasteable = 0;
    if (pane->snapshot) {
        for (DATA_INFO *format = pane->snapshot->child; format; format = format->next) {
            total++;
            if (option.config_force_unicode && (format->format == CF_TEXT || format->format == CF_OEMTEXT)) { duplicate++; continue; }
            if (format_is_unavailable(format)) {
                unavailable++;
                continue;
            }
            bool graphic = format->format == CF_BITMAP || format->format == CF_DIB || format->format == CF_DIBV5 ||
                format->format == CF_ENHMETAFILE || format->format == CF_METAFILEPICT;
            bool text = format_is_text(format);
            if (graphic || !text) {
                pane_add_tile(window, pane, format, -1, {});
                pasteable++;
                continue;
            }
            DWORD size = 0;
            BYTE *raw = format_data_to_bytes(format, &size);
            if (!raw) raw = clipboard_data_to_bytes(format, &size);
            std::vector<BYTE> bytes;
            if (raw && size) bytes.assign(raw, raw + size);
            mem_free(reinterpret_cast<void **>(&raw));
            ULONGLONG hash = bytes.empty() ? 0 : bytes_hash(bytes);
            bool same = false;
            if (!bytes.empty()) for (auto &previous : pane->tiles)
                if (previous.handler < 0 && previous.byte_size == bytes.size() && previous.byte_hash == hash) { same = true; break; }
            if (same) { duplicate++; continue; }
            pane_add_tile(window, pane, format, -1, bytes);
            pasteable++;
        }
        for (int index = 0; index < option.config_handler_count; index++) {
            if (!*option.config_handlers[index].formats || !*option.config_handlers[index].script) continue;
            for (DATA_INFO *format = pane->snapshot->child; format; format = format->next) {
                if (target_matches(option.config_handlers[index].formats, format)) {
                    pane_add_tile(window, pane, format, index, {});
                    break;
                }
            }
        }
    }
    wchar_t information[512], date[96] = L"";
    if (pane->snapshot) {
        SYSTEMTIME captured;
        if (FileTimeToSystemTime(&pane->snapshot->modified, &captured))
            wsprintf(date, L"%04d-%02d-%02d %02d:%02d:%02d", captured.wYear, captured.wMonth, captured.wDay,
                captured.wHour, captured.wMinute, captured.wSecond);
    }
    wsprintf(information, L"%s\r\nFormats: %d    Duplicates / Unicode aliases: %d    Pasteable: %d    Unavailable: %d",
        *date ? date : L"No captured clip", total, duplicate, pasteable, unavailable);
    SetWindowText(pane->information, information);
    SetWindowText(pane->status, L"");
    pane_layout(window, pane);
}

extern "C" HWND pasteplus_create(HINSTANCE instance, HWND owner, int identifier)
{
    WNDCLASS descriptor = {};
    descriptor.hInstance = instance;
    descriptor.lpfnWndProc = pane_proc;
    descriptor.lpszClassName = L"ClipMasterPastePlus";
    descriptor.hCursor = LoadCursor(nullptr, IDC_ARROW);
    descriptor.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    if (!RegisterClass(&descriptor) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return nullptr;
    return CreateWindowEx(WS_EX_CONTROLPARENT, descriptor.lpszClassName, L"", WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN,
        0, 0, 0, 0, owner, reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifier)), instance, nullptr);
}

extern "C" HWND pasteplus_symbols_create(HINSTANCE instance, HWND owner, int identifier)
{
    WNDCLASS descriptor = {};
    descriptor.hInstance = instance;
    descriptor.lpfnWndProc = symbols_proc;
    descriptor.lpszClassName = L"ClipMasterSymbols";
    descriptor.hCursor = LoadCursor(nullptr, IDC_ARROW);
    descriptor.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    if (!RegisterClass(&descriptor) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return nullptr;
    return CreateWindowEx(WS_EX_CONTROLPARENT, descriptor.lpszClassName, L"", WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN,
        0, 0, 0, 0, owner, reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifier)), instance, nullptr);
}