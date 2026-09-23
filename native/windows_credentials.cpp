#include "windows_credentials.h"

extern "C" {
#include "rc_hash.h"
}

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <wincred.h>
#elif defined(__linux__)
#include <pwd.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <cerrno>
#endif

#ifdef __linux__
extern char** environ;
#endif

namespace dk64_ra {

void clear_secret(std::string& secret) {
    if (secret.empty()) {
        return;
    }
#ifdef _WIN32
    SecureZeroMemory(secret.data(), secret.size());
#else
    std::fill(secret.begin(), secret.end(), '\0');
#endif
    secret.clear();
}

bool verify_stored_rom(std::string& hash) {
    hash.clear();
#if defined(_WIN32) || defined(__linux__)
    // Hash Rekongpiled's normalized, stored ROM, never an arbitrary ROM supplied
    // to this mod. This still needs binding to the game process for release.
#ifdef _WIN32
    std::array<wchar_t, 32768> executable{};
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(),
                                            static_cast<DWORD>(executable.size()));
    if (!length || length >= executable.size()) {
        return false;
    }
    const auto directory = std::filesystem::path(executable.data()).parent_path();
    std::error_code filesystem_error;
    if (!std::filesystem::exists(directory / L"portable.txt", filesystem_error) ||
        filesystem_error) {
        return false;
    }
#else
    std::error_code filesystem_error;
    auto directory = std::filesystem::current_path(filesystem_error);
    if (filesystem_error) return false;
    if (!std::filesystem::exists(directory / "portable.txt", filesystem_error)) {
        if (filesystem_error) return false;
        // Match the game's app-folder resolution, including its Flatpak override.
        const char* app_folder = std::getenv("APP_FOLDER_PATH");
        const char* home = std::getenv("HOME");
        if (!home || !*home) {
            const auto* user = getpwuid(getuid());
            home = user ? user->pw_dir : nullptr;
        }
        if (app_folder && *app_folder) directory = app_folder;
        else if (home && *home) directory = std::filesystem::path(home) / ".config" / "DK64Recompiled";
        else return false;
        if (!directory.is_absolute()) return false;
    }
#endif
    const auto rom_path = directory / "DK64.z64";
    const auto utf8 = rom_path.u8string();
    std::array<char, 33> generated{};
    if (!rc_hash_generate_from_file(generated.data(), RC_CONSOLE_NINTENDO_64,
                                    reinterpret_cast<const char*>(utf8.c_str()))) {
        return false;
    }
    static constexpr char kSupported[] = "9ec41abf2519fc386cadd0731f6e868c";
    if (std::string(generated.data()) != kSupported) {
        return false;
    }
    hash = generated.data();
    return true;
#else
    return false;
#endif
}

#ifdef _WIN32
namespace {
// Unique to this mod, so a different RA client cannot accidentally reuse or
// overwrite its token. CRED_PERSIST_LOCAL_MACHINE does not roam to other PCs.
constexpr wchar_t kCredentialTarget[] = L"Nikamimi_DK64Rekong_RetroAchievements_preview_v1";

std::string utf8_from_wide(const wchar_t* text) {
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1,
                                           nullptr, 0, nullptr, nullptr);
    if (length <= 1) {
        return {};
    }
    std::string value(static_cast<std::size_t>(length), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1,
                             value.data(), length, nullptr, nullptr)) {
        return {};
    }
    value.pop_back();
    return value;
}

std::wstring wide_from_utf8(const char* text) {
    if (!text || !*text) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1,
                                           nullptr, 0);
    if (length <= 1) {
        return {};
    }
    std::wstring value(static_cast<std::size_t>(length), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1,
                             value.data(), length)) {
        return {};
    }
    value.pop_back();
    return value;
}
} // namespace
#endif

#ifdef __linux__
namespace {
// Invoke a trusted GUI helper by absolute path. Passwords travel only through
// its private stdout pipe, never a shell, argv, environment, or a log file.
bool dialog(const char* program, const char* const* arguments, std::string& output) {
    output.clear();
    if (access(program, X_OK) != 0) return false;
    int pipe_fds[2]{};
    if (pipe(pipe_fds) != 0) return false;
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
    posix_spawn_file_actions_addclose(&actions, pipe_fds[1]);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    pid_t child = -1;
    const int spawned = posix_spawn(&child, program, &actions,
                                    nullptr, const_cast<char* const*>(arguments), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipe_fds[1]);
    bool bounded = spawned == 0;
    if (bounded) {
        char buffer[128];
        ssize_t count = 0;
        while ((count = read(pipe_fds[0], buffer, sizeof(buffer))) > 0) {
            if (static_cast<std::size_t>(count) > 512 - output.size()) bounded = false;
            if (bounded) output.append(buffer, static_cast<std::size_t>(count));
        }
        if (count < 0) bounded = false;
    }
    close(pipe_fds[0]);
    int status = 0;
    pid_t waited = -1;
    if (spawned == 0) {
        do { waited = waitpid(child, &status, 0); } while (waited == -1 && errno == EINTR);
    }
    const bool success = bounded && waited == child &&
                         WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (!success) clear_secret(output);
    return success;
}

void strip_newline(std::string& value) {
    if (!value.empty() && value.back() == '\n') value.pop_back();
    if (!value.empty() && value.back() == '\r') value.pop_back();
}

bool valid_dialog_value(const std::string& value) {
    return !value.empty() && value.size() <= 256 &&
           value.find_first_of("\r\n\t") == std::string::npos;
}
} // namespace
#endif

bool read_saved_login(std::string& username, std::string& token) {
    username.clear();
    clear_secret(token);
#ifdef _WIN32
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(kCredentialTarget, CRED_TYPE_GENERIC, 0, &credential)) {
        return false;
    }
    bool valid = false;
    if (credential && credential->UserName && credential->CredentialBlob &&
        credential->CredentialBlobSize > 0 &&
        credential->CredentialBlobSize <= CRED_MAX_CREDENTIAL_BLOB_SIZE) {
        username = utf8_from_wide(credential->UserName);
        const char* begin = reinterpret_cast<const char*>(credential->CredentialBlob);
        token.assign(begin, begin + credential->CredentialBlobSize);
        valid = !username.empty() && !token.empty() &&
            std::all_of(token.begin(), token.end(),
                        [](unsigned char c) { return c >= 0x21 && c <= 0x7e; });
    }
    if (credential) {
        if (credential->CredentialBlob && credential->CredentialBlobSize) {
            SecureZeroMemory(credential->CredentialBlob, credential->CredentialBlobSize);
        }
        CredFree(credential);
    }
    if (!valid) {
        username.clear();
        clear_secret(token);
    }
    return valid;
#else
    return false;
#endif
}

bool store_saved_login(const char* username, const char* token) {
#ifdef _WIN32
    const auto wide_username = wide_from_utf8(username);
    if (wide_username.empty() || wide_username.size() > CRED_MAX_USERNAME_LENGTH ||
        !token || !*token) {
        return false;
    }
    const auto token_length = std::strlen(token);
    if (token_length > CRED_MAX_CREDENTIAL_BLOB_SIZE ||
        !std::all_of(token, token + token_length,
                     [](unsigned char c) { return c >= 0x21 && c <= 0x7e; })) {
        return false;
    }
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<wchar_t*>(kCredentialTarget);
    credential.UserName = const_cast<wchar_t*>(wide_username.c_str());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(token));
    credential.CredentialBlobSize = static_cast<DWORD>(token_length);
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    return CredWriteW(&credential, 0) != FALSE;
#else
    (void)username;
    (void)token;
    return false;
#endif
}

bool forget_saved_login() {
#ifdef _WIN32
    if (CredDeleteW(kCredentialTarget, CRED_TYPE_GENERIC, 0)) {
        return true;
    }
    return GetLastError() == ERROR_NOT_FOUND;
#else
    return true;
#endif
}

bool prompt_for_credentials(std::string& username, std::string& password,
                            bool remember_signin) {
    username.clear();
    clear_secret(password);
#ifdef _WIN32
    CREDUI_INFOW info{};
    info.cbSize = sizeof(info);
    info.hwndParent = GetForegroundWindow();
    info.pszCaptionText = L"DK64 RetroAchievements online sign-in";
    info.pszMessageText = remember_signin ?
        L"Online softcore awards require an RA account. Your password is not saved; a login token will be stored in Windows Credential Manager. Cancel for cached local tracking only." :
        L"Online softcore awards require an RA account. Your password and token are not saved. Cancel for cached local tracking only.";
    std::array<wchar_t, CREDUI_MAX_USERNAME_LENGTH + 1> user{};
    std::array<wchar_t, CREDUI_MAX_PASSWORD_LENGTH + 1> secret{};
    BOOL save = FALSE;
    const DWORD flags = CREDUI_FLAGS_GENERIC_CREDENTIALS |
                        CREDUI_FLAGS_ALWAYS_SHOW_UI | CREDUI_FLAGS_DO_NOT_PERSIST;
    const DWORD result = CredUIPromptForCredentialsW(&info, L"retroachievements.org",
                                                     nullptr, 0, user.data(),
                                                     static_cast<ULONG>(user.size()),
                                                     secret.data(),
                                                     static_cast<ULONG>(secret.size()),
                                                     &save, flags);
    if (result == NO_ERROR) {
        username = utf8_from_wide(user.data());
        password = utf8_from_wide(secret.data());
    }
    SecureZeroMemory(secret.data(), secret.size() * sizeof(wchar_t));
    if (result != NO_ERROR || username.empty() || password.empty()) {
        clear_secret(password);
        username.clear();
        return false;
    }
    return true;
#elif defined(__linux__)
    (void)remember_signin; // Linux tokens are not persisted in this first build.
    if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY")) return false;
    std::string response;
    if (access("/usr/bin/kdialog", X_OK) == 0) {
        const char* const user_args[] = {"kdialog", "--title", "DK64 RetroAchievements",
                                          "--inputbox", "RA username (softcore beta)", nullptr};
        if (!dialog("/usr/bin/kdialog", user_args, response)) return false;
        strip_newline(response);
        if (!valid_dialog_value(response) || response.size() > 64) return false;
        username = std::move(response);
        const char* const password_args[] = {"kdialog", "--title", "DK64 RetroAchievements",
                                              "--password", "RA password (not saved)", nullptr};
        if (!dialog("/usr/bin/kdialog", password_args, password)) {
            username.clear();
            return false;
        }
        strip_newline(password);
    } else {
        const char* const args[] = {"zenity", "--forms", "--title=DK64 RetroAchievements",
                                    "--text=Online softcore beta; password is not saved",
                                    "--add-entry=RA username", "--add-password=RA password",
                                    "--separator=|", nullptr};
        if (!dialog("/usr/bin/zenity", args, response)) return false;
        strip_newline(response);
        const auto separator = response.find('|');
        if (separator == std::string::npos) {
            clear_secret(response);
            return false;
        }
        username.assign(response, 0, separator);
        password.assign(response, separator + 1, std::string::npos);
        clear_secret(response);
    }
    if (!valid_dialog_value(username) || username.size() > 64 ||
        !valid_dialog_value(password)) {
        username.clear();
        clear_secret(password);
        return false;
    }
    return true;
#else
    (void)remember_signin;
    return false;
#endif
}

} // namespace dk64_ra
