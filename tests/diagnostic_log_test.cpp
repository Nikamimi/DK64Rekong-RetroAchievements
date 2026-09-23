#include "diagnostic_log.h"

#include <cstdio>
#include <cwchar>
#include <fstream>
#include <filesystem>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (!length || length >= MAX_PATH) {
        return 1;
    }
    wchar_t* filename = wcsrchr(path, L'\\');
    if (!filename ||
        wcscpy_s(filename + 1, MAX_PATH - (filename + 1 - path),
                 L"dk64_ra_probe.local.log") != 0) {
        return 2;
    }

    dk64_ra::diagnostic_begin(true);
    dk64_ra::diagnostic_log("[DK64 RA] %s", "one\nline");
    dk64_ra::diagnostic_begin(false);
    std::ifstream first(path);
    std::string line;
    if (!std::getline(first, line) || line != "[DK64 RA] one line") {
        return 3;
    }
    first.close();

    dk64_ra::diagnostic_begin(true);
    dk64_ra::diagnostic_log("[DK64 RA] second session");
    dk64_ra::diagnostic_begin(false);
    std::ifstream second(path);
    if (!std::getline(second, line) || line != "[DK64 RA] second session" ||
        std::getline(second, line)) {
        return 4;
    }
#elif defined(__linux__)
    if (argc < 1 || !argv[0]) return 1;
    const auto path = std::filesystem::absolute(argv[0]).parent_path() /
                      "dk64_ra_probe.local.log";
    dk64_ra::diagnostic_begin(true);
    dk64_ra::diagnostic_log("[DK64 RA] %s", "one\nline");
    dk64_ra::diagnostic_begin(false);
    std::ifstream first(path);
    std::string line;
    if (!std::getline(first, line) || line != "[DK64 RA] one line") return 3;
    first.close();
    dk64_ra::diagnostic_begin(true);
    dk64_ra::diagnostic_log("[DK64 RA] second session");
    dk64_ra::diagnostic_begin(false);
    std::ifstream second(path);
    if (!std::getline(second, line) || line != "[DK64 RA] second session" ||
        std::getline(second, line)) return 4;
#endif
    return 0;
}
