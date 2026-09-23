#pragma once

#include <string>

namespace dk64_ra {

// Platform-specific ROM verification and sign-in. Only Windows optionally
// persists a token in Credential Manager; passwords and ROMs are never stored here.
bool verify_stored_rom(std::string& hash);
bool prompt_for_credentials(std::string& username, std::string& password, bool remember_signin);
bool read_saved_login(std::string& username, std::string& token);
bool store_saved_login(const char* username, const char* token);
bool forget_saved_login();
void clear_secret(std::string& secret);

} // namespace dk64_ra
