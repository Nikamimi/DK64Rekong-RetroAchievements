# Linux development and test notes

**Development build only.** The Linux x86-64 companion compiles and passes offline tests on Ubuntu 24.04 under WSL. We have not run it inside native Linux DK64 Rekongpiled 1.0.2, on Steam Deck, or with a real RA account. Do not share it as a verified Linux release or claim RA support/certification.

Rekongpiled's Linux loader loads `dk64_ra_probe.so` next to `dk64_ra_probe.nrm` and checks `recomp_api_version = 1`. The same MIPS `.nrm` is used across platforms; never pair a Windows `.dll` with the Linux game. Build natively on Linux x86-64 (WSL is sufficient to compile, but not to verify the game integration):

```sh
# Install your distro's equivalents of cmake, clang, lld, make,
# a C++17 compiler, libcurl >= 7.84 development headers, and SDL2 headers.
git clone --depth 1 --branch 1.0.2 https://github.com/Rainchus/Donkey-Kong-64-Recompiled.git reference/dk64
make
RecompModTool mod.toml build
cmake -S native -B build/native-linux -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build/native-linux -j 4
ctest --test-dir build/native-linux --output-on-failure
```

The game must be closed when installing. For a portable Linux install with `portable.txt` in the game working directory, put the `.nrm` and `.so` in its `mods/` folder beside each other. For a nonportable install use `~/.config/DK64Recompiled/mods/`, or `$APP_FOLDER_PATH/mods/` if the game was launched with that override. The mod hashes the game's stored `DK64.z64` in that app folder before any login/tracking; it accepts only the supported US retail hash. It never copies a ROM. The in-game `.nrm` installer alone does not install the companion `.so`.

Local history and the on-device cached set go under `${XDG_DATA_HOME:-$HOME/.local/share}/Nikamimi/DK64RekongRetroAchievements/`; these are independent from DK64 saves. The `.so` writes its own non-secret diagnostic lines to `mods/dk64_ra_probe.local.log`. Do not bundle the cache, log, history, ROM, credentials or saves with a mod package. Local Tracking needs a set cached by a successful Online load **on this Linux user account**, unless a trusted, compatible cache is deliberately migrated; a fresh offline install cannot evaluate the set.

Online uses libcurl HTTPS to only `retroachievements.org` with TLS verification, no redirects and a bounded response; badges come from the allowed RA media path on a background worker. Online asks for credentials via `/usr/bin/kdialog` (preferred) or `/usr/bin/zenity` on a graphical desktop; neither a shell nor a command-line password is used. No Linux login token or password is saved. Without a graphical session or either helper, canceling the dialog, or a failed sign-in, it falls back to the cached guest set and makes no awards. The Remember option is a Windows-only feature. The HTTPS/login path still blocks a game frame, as on Windows. Only the Windows Online award path has live proof.

For the initial invited smoke group, follow the [private Linux tester guide](private-linux-tester-guide.md) and collect the first native-game evidence. Before expanding beyond that group, verify `.so` loading and runtime shared-library dependencies (`ldd`), stored-ROM detection in both portable and nonportable installs, the actual active ROM identity, the password dialog in Desktop and Steam Deck Gaming modes, authenticated set load, guest fallback and cache persistence, F8/Esc/controller input, badge rendering, audio, reset confirmation, and frame/trigger parity. Keep Hardcore and leaderboards disabled. Do not submit speculative or repeated achievements during integration testing.
