# DK64 RetroAchievements — experimental Linux x86-64 smoke test

**Source code:** [GitHub repository](https://github.com/Nikamimi/DK64Rekong-RetroAchievements). This tester ZIP contains the mod binaries and instructions, not the Git repository. Use that link separately if you want to inspect or build the code.

This is an **experimental Linux development build**, published as a GitHub pre-release for testing, not a stable or certified RetroAchievements client. It has passed build and offline checks under Ubuntu 24.04/WSL, but has **not** run in a native Linux game session or confirmed a Linux account award. Share the official pre-release link with these warnings; do not describe it as verified Steam Deck support. The in-game mod name retains its earlier **private beta** label. It uses the existing RA DK64 N64 set (game 10075); Hardcore and leaderboards are disabled.

## Requirements and compatibility

- Native Linux x86-64 **DK64 Rekongpiled 1.0.2** and your own supported US retail DK64 ROM. Do not use the Windows game under Proton with this `.so`, or an ARM64 game build. [Get the game from the upstream 1.0.2 release](https://github.com/Rainchus/Donkey-Kong-64-Recompiled/releases/tag/1.0.2); no game or ROM is in this ZIP.
- This particular companion was built on Ubuntu 24.04 and requires **glibc 2.38 or newer**, `GLIBCXX_3.4.30` or newer, an OpenSSL-flavored `libcurl.so.4` providing `CURL_OPENSSL_4`, and `libSDL2-2.0.so.0`. A newer distro may still have other loader or game dependencies. Do **not** replace your system glibc to make this build load. If it fails to load, tell us your distro and error; we can build against a different baseline.
- For optional Online sign-in, a graphical session and `/usr/bin/kdialog` or `/usr/bin/zenity` are needed. No Linux token/password is saved. Steam Deck Gaming-mode dialog behavior is unknown; first try Desktop mode.

## Install and initial no-award check

1. Close the game. Extract the ZIP. Optionally run `sha256sum -c SHA256SUMS.txt` in the extracted folder to check its listed files. If you already use this mod, keep your old mod files somewhere outside `mods/` before replacing them.
2. Copy **both** `dk64_ra_probe.nrm` and `dk64_ra_probe.so` into the same game `mods/` folder. A portable native-Linux install with `portable.txt` in the game working directory uses that directory's `mods/`; a nonportable install normally uses `~/.config/DK64Recompiled/mods/`. An `APP_FOLDER_PATH` override changes the app folder. Do not put the `.so` in the game's executable directory unless that is also its `mods/` directory. The game's `.nrm` installer by itself does not install the `.so`.
3. Start the native Linux game and enable **DK64 RetroAchievements (private beta)** in Mods. Select **Local Tracking only** for the first smoke test and restart after changing the mode. This never signs in or submits awards.
4. Enter gameplay, then press **F8** to open the browser and **Esc** or controller **B** to close it. On a fresh Linux account, **“Local set unavailable: sign in online once to cache it”** is expected: no RA definitions are bundled. A **“Tracking unavailable: ROM mismatch”** message means the stored `DK64.z64` in the game's app folder was not found or did not match; do not send us the ROM.

If the mod fails to load, run `ldd /path/to/mods/dk64_ra_probe.so` and look for `not found`, or check the game's own mod error. Report the exact error and Linux distro; do not upload the whole game folder.

## Optional Online test — real RA submissions

Only if you personally want to test your RA account: switch to **Online (softcore beta)** and restart. The default is Online, so explicitly select Local first if you do **not** want submissions. Online asks for your RA username/password using the desktop dialog. **Ask each launch** is the correct Linux choice; the Remember setting has no effect here. A successful Online load caches the validated achievement set for later Local Tracking. A canceled/unavailable prompt or failed load falls back to that cached guest set if present; a fresh install has no set to fall back to. Do not treat a local trigger as an RA award: only **Unlocked on RA** means a confirmed account award. Network requests can briefly pause the game, and unconfirmed awards are not queued across restarts. We have not verified any Linux award yet.

Please first confirm the browser shows the RA username, game/set status, and sensible achievement rows before attempting a gameplay trigger. If you choose to test an actual unlock, report its ID and whether it shows **Unlocked on RA** after restart. The earlier Windows DK Rap proof was a **zero-point** achievement only; it does not establish broader memory parity or points earning.

## What to report and safe removal

Tell us the distro/version, native game version, Desktop versus Steam Deck Gaming mode, portable versus nonportable install, whether the `.so` loaded, browser status, F8/Esc/controller behavior, badge/jingle behavior, and any error text. If comfortable, send a screenshot of the in-game browser or the mod's own `mods/dk64_ra_probe.local.log` **after reviewing it**. Never send your ROM, password, login token, save files, account-private data, or an unreviewed general game log.

To uninstall, close the game, disable the mod in Mods, and remove only `dk64_ra_probe.nrm` and `dk64_ra_probe.so` from `mods/`. Do not delete DK64 saves. This mod's local history/cache lives separately at `${XDG_DATA_HOME:-$HOME/.local/share}/Nikamimi/DK64RekongRetroAchievements/` and is not included in this ZIP.
