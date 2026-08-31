#include "VxWindowFunctions.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#if defined(_MSC_VER)
#include <float.h>
#endif

#include "VxImageDescEx.h"

namespace {
namespace fs = std::filesystem;

bool CopyString(const std::string &value, char *destination, size_t capacity) {
    if (!destination || capacity == 0 || value.size() + 1 > capacity) {
        if (destination && capacity) {
            destination[0] = '\0';
        }
        return false;
    }
    std::memcpy(destination, value.c_str(), value.size() + 1);
    return true;
}

bool WildcardMatches(const char *pattern, const char *text) {
    if (!pattern || !*pattern) {
        return true;
    }
    const char *star = NULL;
    const char *retry = NULL;
    while (*text) {
        if (*pattern == '?' || std::tolower(static_cast<unsigned char>(*pattern)) ==
                std::tolower(static_cast<unsigned char>(*text))) {
            ++pattern;
            ++text;
        } else if (*pattern == '*') {
            star = pattern++;
            retry = text;
        } else if (star) {
            pattern = star + 1;
            text = ++retry;
        } else {
            return false;
        }
    }
    while (*pattern == '*') {
        ++pattern;
    }
    return *pattern == '\0';
}

std::string ModulePath(HMODULE module) {
    std::vector<char> buffer(512);
    for (;;) {
        const DWORD written = GetModuleFileNameA(module, buffer.data(),
                                                static_cast<DWORD>(buffer.size()));
        if (written == 0) {
            return {};
        }
        if (written < buffer.size() - 1) {
            return std::string(buffer.data(), written);
        }
        if (buffer.size() >= 32768) {
            return {};
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::string LocalPathFromUrl(const char *value) {
    if (!value) {
        return {};
    }
    const char prefix[] = "file://";
    if (_strnicmp(value, prefix, sizeof(prefix) - 1) != 0) {
        return value;
    }
    value += sizeof(prefix) - 1;
    if (_strnicmp(value, "localhost/", 10) == 0) {
        value += 9;
    }
    XString decoded(value);
    VxUnEscapeUrl(decoded);
    return decoded.CStr();
}
}

char VxScanCodeToAscii(XDWORD scancode, unsigned char keystate[256]) {
    static const char scanCodeLetters[] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0,
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
        'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
    };
    if (scancode >= sizeof(scanCodeLetters)) {
        return '\0';
    }
    char result = scanCodeLetters[scancode];
    if (result >= 'a' && result <= 'z' && keystate &&
            (keystate[0x2A] || keystate[0x36])) {
        result = static_cast<char>(std::toupper(static_cast<unsigned char>(result)));
    }
    return result;
}

int VxScanCodeToName(XDWORD scancode, char *keyName) {
    if (!keyName) {
        return 0;
    }
    const int written = std::snprintf(keyName, 32, "ScanCode_%u",
                                      static_cast<unsigned int>(scancode));
    return written < 0 ? 0 : std::min(written + 1, 32);
}

int VxShowCursor(XBOOL show) {
    static int displayCount = 0;
    displayCount += show ? 1 : -1;
    return displayCount;
}

XBOOL VxSetCursor(VXCURSOR_POINTER) { return TRUE; }

XWORD VxGetFPUControlWord() {
#if defined(_MSC_VER)
    unsigned int value = 0;
    _controlfp_s(&value, 0, 0);
    return static_cast<XWORD>(value & 0xffffu);
#else
    XWORD value = 0;
    __asm__ __volatile__("fnstcw %0" : "=m"(value));
    return value;
#endif
}

void VxSetFPUControlWord(XWORD value) {
#if defined(_MSC_VER)
    unsigned int ignored = 0;
#if defined(_M_IX86)
    const unsigned int mask = _MCW_DN | _MCW_EM | _MCW_RC | _MCW_PC;
#else
    const unsigned int mask = _MCW_DN | _MCW_EM | _MCW_RC;
#endif
    _controlfp_s(&ignored, value, mask);
#else
    __asm__ __volatile__("fldcw %0" : : "m"(value));
#endif
}

void VxSetBaseFPUControlWord() {
#if defined(_MSC_VER)
    unsigned int ignored = 0;
#if defined(_M_IX86)
    const unsigned int mask = _MCW_DN | _MCW_EM | _MCW_RC | _MCW_PC;
#else
    const unsigned int mask = _MCW_DN | _MCW_EM | _MCW_RC;
#endif
    _controlfp_s(&ignored, _CW_DEFAULT, mask);
#else
    const XWORD value = 0x037f;
    VxSetFPUControlWord(value);
#endif
}

void VxAddLibrarySearchPath(const char *path) {
    if (!path || !*path) {
        return;
    }
    XString oldPath;
    VxGetEnvironmentVariable("PATH", oldPath);
    std::string value(path);
    if (!value.empty() && value.back() != ';') {
        value.push_back(';');
    }
    value += oldPath.CStr();
    VxSetEnvironmentVariable("PATH", value.c_str());
}

XBOOL VxGetEnvironmentVariable(const char *name, XString &value) {
    value = "";
    if (!name || !*name) {
        return FALSE;
    }
    const DWORD required = GetEnvironmentVariableA(name, NULL, 0);
    if (!required) {
        return FALSE;
    }
    std::vector<char> buffer(required);
    if (!GetEnvironmentVariableA(name, buffer.data(), required)) {
        return FALSE;
    }
    value = buffer.data();
    return TRUE;
}

XBOOL VxSetEnvironmentVariable(const char *name, const char *value) {
    return name && *name && SetEnvironmentVariableA(name, value) ? TRUE : FALSE;
}

WIN_HANDLE VxWindowFromPoint(CKPOINT) { return NULL; }

XBOOL VxGetClientRect(WIN_HANDLE, CKRECT *rect) {
    if (rect) {
        rect->left = rect->top = rect->right = rect->bottom = 0;
    }
    return FALSE;
}

XBOOL VxGetWindowRect(WIN_HANDLE window, CKRECT *rect) {
    return VxGetClientRect(window, rect);
}
XBOOL VxScreenToClient(WIN_HANDLE, CKPOINT *) { return FALSE; }
XBOOL VxClientToScreen(WIN_HANDLE, CKPOINT *) { return FALSE; }
WIN_HANDLE VxSetParent(WIN_HANDLE child, WIN_HANDLE) { return child; }
WIN_HANDLE VxGetParent(WIN_HANDLE) { return NULL; }
XBOOL VxMoveWindow(WIN_HANDLE, int, int, int, int, XBOOL) { return FALSE; }

XString VxGetTempPath() {
    std::vector<char> buffer(MAX_PATH + 1);
    const DWORD length = GetTempPathA(static_cast<DWORD>(buffer.size()), buffer.data());
    return length && length < buffer.size() ? XString(buffer.data()) : XString("");
}

XBOOL VxMakeDirectory(const char *path) {
    if (!path || !*path) {
        return FALSE;
    }
    std::error_code error;
    return (fs::create_directory(fs::path(path), error) || fs::is_directory(fs::path(path), error))
            && !error ? TRUE : FALSE;
}

XBOOL VxRemoveDirectory(const char *path) {
    if (!path || !*path) {
        return FALSE;
    }
    std::error_code error;
    return fs::remove(fs::path(path), error) && !error ? TRUE : FALSE;
}

XBOOL VxDeleteDirectory(const char *path) {
    if (!path || !*path) {
        return FALSE;
    }
    std::error_code error;
    fs::remove_all(fs::path(path), error);
    return !error ? TRUE : FALSE;
}

XBOOL VxFileExists(const char *path) {
    std::error_code error;
    return path && fs::is_regular_file(fs::path(path), error) && !error ? TRUE : FALSE;
}

XBOOL VxDirectoryExists(const char *path) {
    std::error_code error;
    return path && fs::is_directory(fs::path(path), error) && !error ? TRUE : FALSE;
}

XBOOL VxCopyFile(const char *source, const char *destination, XBOOL failIfExists) {
    if (!source || !destination) {
        return FALSE;
    }
    std::error_code error;
    const fs::copy_options options = failIfExists
            ? fs::copy_options::none
            : fs::copy_options::overwrite_existing;
    return fs::copy_file(source, destination, options, error) && !error ? TRUE : FALSE;
}

XBOOL VxDeleteFile(const char *path) {
    if (!path) {
        return FALSE;
    }
    std::error_code error;
    return fs::remove(fs::path(path), error) && !error ? TRUE : FALSE;
}

XBOOL VxListDirectory(const char *directory, const char *mask, XBOOL includeDirectories,
                      VxDirectoryEntryCallback callback, void *userData) {
    if (!directory || !callback) {
        return FALSE;
    }
    std::error_code error;
    fs::directory_iterator iterator(fs::path(directory), error);
    if (error) {
        return FALSE;
    }
    for (const fs::directory_entry &item : iterator) {
        const std::string name = item.path().filename().string();
        if (!WildcardMatches(mask, name.c_str())) {
            continue;
        }
        const bool isDirectory = item.is_directory(error);
        if (error) {
            return FALSE;
        }
        if (isDirectory && !includeDirectories) {
            continue;
        }
        VxDirectoryEntry entry;
        entry.Name = name.c_str();
        entry.IsDirectory = isDirectory ? TRUE : FALSE;
        entry.Size = isDirectory ? 0 : static_cast<size_t>(item.file_size(error));
        if (error || !callback(&entry, userData)) {
            return FALSE;
        }
    }
    return TRUE;
}

XBOOL VxGetCurrentDirectory(char *path, size_t pathSize) {
    if (!path || pathSize == 0 || pathSize > MAXDWORD) {
        return FALSE;
    }
    const DWORD written = GetCurrentDirectoryA(static_cast<DWORD>(pathSize), path);
    return written && written < pathSize ? TRUE : FALSE;
}

XString VxGetCurrentDirectory() {
    const DWORD required = GetCurrentDirectoryA(0, NULL);
    if (!required) {
        return "";
    }
    std::vector<char> buffer(required);
    return GetCurrentDirectoryA(required, buffer.data()) ? XString(buffer.data()) : XString("");
}

XBOOL VxGetApplicationBasePath(char *path, size_t pathSize) {
    const std::string executable = ModulePath(NULL);
    if (executable.empty()) {
        return FALSE;
    }
    std::string directory = fs::path(executable).parent_path().string();
    if (!directory.empty() && directory.back() != '\\') {
        directory.push_back('\\');
    }
    return CopyString(directory, path, pathSize) ? TRUE : FALSE;
}

XBOOL VxGetUserConfigPath(const char *appName, XString &path) {
    XString root;
    if (!VxGetEnvironmentVariable("LOCALAPPDATA", root) &&
            !VxGetEnvironmentVariable("APPDATA", root)) {
        path = "";
        return FALSE;
    }
    fs::path config(root.CStr());
    config /= appName && *appName ? appName : "Ballance";
    std::error_code error;
    fs::create_directories(config, error);
    if (error) {
        path = "";
        return FALSE;
    }
    std::string value = config.string();
    if (!value.empty() && value.back() != '\\') {
        value.push_back('\\');
    }
    path = value.c_str();
    return TRUE;
}

XBOOL VxGetUserConfigPath(const char *appName, char *path, size_t pathSize) {
    XString value;
    return VxGetUserConfigPath(appName, value) &&
           CopyString(value.CStr(), path, pathSize) ? TRUE : FALSE;
}

XBOOL VxSetCurrentDirectory(const char *path) {
    return path && SetCurrentDirectoryA(path) ? TRUE : FALSE;
}

XBOOL VxMakePath(char *fullPath, size_t fullPathSize, const char *path, const char *file) {
    if (!fullPath || !fullPathSize || !path || !file) {
        return FALSE;
    }
    std::string result(path);
    if (!result.empty() && result.back() != '\\' && result.back() != '/') {
        result.push_back('\\');
    }
    result += file;
    return CopyString(result, fullPath, fullPathSize) ? TRUE : FALSE;
}

XBOOL VxMakePath(XString &fullPath, const char *path, const char *file) {
    if (!path || !file) {
        fullPath = "";
        return FALSE;
    }
    std::string result(path);
    if (!result.empty() && result.back() != '\\' && result.back() != '/') {
        result.push_back('\\');
    }
    result += file;
    fullPath = result.c_str();
    return TRUE;
}

XBOOL VxTestDiskSpace(const char *directory, size_t size) {
    ULARGE_INTEGER available = {};
    return directory && GetDiskFreeSpaceExA(directory, &available, NULL, NULL) &&
           available.QuadPart >= size ? TRUE : FALSE;
}

int VxMessageBox(WIN_HANDLE, const char *text, const char *caption, XDWORD) {
    std::fprintf(stderr, "%s: %s\n", caption ? caption : "VxMath",
                 text ? text : "");
    return 0;
}

XString VxGetModuleFileName(INSTANCE_HANDLE handle) {
    const std::string path = ModulePath(static_cast<HMODULE>(handle));
    return path.empty() ? XString("") : XString(path.c_str());
}

size_t VxGetModuleFileName(INSTANCE_HANDLE handle, char *path, size_t pathSize) {
    const std::string value = ModulePath(static_cast<HMODULE>(handle));
    return CopyString(value, path, pathSize) ? value.size() : 0;
}

INSTANCE_HANDLE VxGetModuleHandle(const char *fileName) {
    return reinterpret_cast<INSTANCE_HANDLE>(GetModuleHandleA(fileName));
}

XBOOL VxCreateFileTree(const char *file) {
    if (!file || !*file) {
        return FALSE;
    }
    const fs::path parent = fs::path(file).parent_path();
    if (parent.empty()) {
        return TRUE;
    }
    std::error_code error;
    fs::create_directories(parent, error);
    return !error ? TRUE : FALSE;
}

XDWORD VxURLDownloadToCacheFile(const char *file, char *cachedFile, int cachedFileSize) {
    if (!file || !*file || !cachedFile || cachedFileSize <= 0) {
        return 0x80070057u;
    }
    cachedFile[0] = '\0';
    if (std::strstr(file, "://") && _strnicmp(file, "file://", 7) != 0) {
        return 0x80004001u;
    }

    char temporaryDirectory[MAX_PATH + 1] = {};
    char temporaryFile[MAX_PATH + 1] = {};
    if (!GetTempPathA(MAX_PATH, temporaryDirectory) ||
            !GetTempFileNameA(temporaryDirectory, "vxh", 0, temporaryFile)) {
        return 0x80004005u;
    }
    const std::string source = LocalPathFromUrl(file);
    if (!CopyFileA(source.c_str(), temporaryFile, FALSE)) {
        DeleteFileA(temporaryFile);
        return 0x80004005u;
    }
    if (!CopyString(temporaryFile, cachedFile, static_cast<size_t>(cachedFileSize))) {
        DeleteFileA(temporaryFile);
        return 0x8007007au;
    }
    return 0;
}

BITMAP_HANDLE VxCreateBitmap(const VxImageDescEx &) { return NULL; }
void VxDeleteBitmap(BITMAP_HANDLE) {}
XBYTE *VxConvertBitmap(BITMAP_HANDLE, VxImageDescEx &) { return NULL; }
BITMAP_HANDLE VxConvertBitmapTo24(BITMAP_HANDLE) { return NULL; }
XBOOL VxCopyBitmap(BITMAP_HANDLE, const VxImageDescEx &) { return FALSE; }
VX_OSINFO VxGetOs() { return VXOS_WINSEVEN; }

XBOOL VxGetMemoryStatus(VxMemoryStatus *status) {
    if (!status) {
        return FALSE;
    }
    MEMORYSTATUSEX memory = {};
    memory.dwLength = sizeof(memory);
    if (!GlobalMemoryStatusEx(&memory)) {
        return FALSE;
    }
    status->MemoryLoad = memory.dwMemoryLoad;
    status->TotalPhysical = memory.ullTotalPhys;
    status->AvailablePhysical = memory.ullAvailPhys;
    status->TotalVirtual = memory.ullTotalVirtual;
    status->AvailableVirtual = memory.ullAvailVirtual;
    return TRUE;
}

FONT_HANDLE VxCreateFont(const char *, int, int, XBOOL, XBOOL) { return NULL; }

XBOOL VxGetFontInfo(FONT_HANDLE, VXFONTINFO &description) {
    description.FaceName = "";
    description.Height = 0;
    description.Weight = 0;
    description.Italic = FALSE;
    description.Underline = FALSE;
    return FALSE;
}

XBOOL VxDrawBitmapText(BITMAP_HANDLE, FONT_HANDLE, const char *, CKRECT *,
                       XDWORD, XDWORD, XDWORD) {
    return FALSE;
}

void VxDeleteFont(FONT_HANDLE) {}
