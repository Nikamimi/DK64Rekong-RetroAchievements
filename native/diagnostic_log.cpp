#include "diagnostic_log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <filesystem>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#endif

namespace dk64_ra {
namespace {
#ifdef _WIN32
HANDLE local_log = INVALID_HANDLE_VALUE;
#elif defined(__linux__)
std::FILE* local_log = nullptr;
#endif
} // namespace

void diagnostic_begin(bool local_tracking) {
#ifdef _WIN32
    if (local_log != INVALID_HANDLE_VALUE) {
        CloseHandle(local_log);
        local_log = INVALID_HANDLE_VALUE;
    }
    if (!local_tracking) {
        return;
    }

    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&diagnostic_begin), &module)) {
        return;
    }
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
    if (!length || length >= MAX_PATH) {
        return;
    }
    wchar_t* filename = wcsrchr(path, L'\\');
    if (!filename) {
        return;
    }
    ++filename;
    if (wcscpy_s(filename, MAX_PATH - (filename - path),
                 L"dk64_ra_probe.local.log") != 0) {
        return;
    }
    local_log = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
#elif defined(__linux__)
    if (local_log) {
        std::fclose(local_log);
        local_log = nullptr;
    }
    if (!local_tracking) return;
    Dl_info module{};
    if (!dladdr(reinterpret_cast<const void*>(&diagnostic_begin), &module) ||
        !module.dli_fname) return;
    const auto path = std::filesystem::path(module.dli_fname).parent_path() /
                      "dk64_ra_probe.local.log";
    local_log = std::fopen(path.c_str(), "wb");
#else
    (void)local_tracking;
#endif
}

void diagnostic_log(const char* format, ...) {
    char line[1024]{};
    va_list arguments;
    va_start(arguments, format);
    const int formatted = std::vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    if (formatted < 0) {
        return;
    }

    // A server-provided achievement title must not inject extra log lines.
    std::size_t length = std::strlen(line);
    for (std::size_t i = 0; i < length; ++i) {
        if (static_cast<unsigned char>(line[i]) < 0x20) {
            line[i] = ' ';
        }
    }
    if (length >= sizeof(line) - 1) {
        length = sizeof(line) - 2;
    }
    line[length++] = '\n';
    line[length] = '\0';
    std::fputs(line, stdout);
    std::fflush(stdout);

#ifdef _WIN32
    if (local_log != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(local_log, line, static_cast<DWORD>(length), &written, nullptr);
        FlushFileBuffers(local_log);
    }
#elif defined(__linux__)
    if (local_log) {
        std::fwrite(line, 1, length, local_log);
        std::fflush(local_log);
    }
#endif
}

} // namespace dk64_ra
