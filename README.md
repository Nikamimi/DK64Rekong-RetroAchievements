# DK64 Rekongpiled × RetroAchievements

A work-in-progress mod that brings the [existing DK64 N64 achievement set](https://retroachievements.org/game/10075) to [DK64 Rekongpiled](https://github.com/Rainchus/Donkey-Kong-64-Recompiled). It uses the official `rcheevos` client and does not include a game, ROM, or achievement definitions.

![DK64 RetroAchievements](thumb.png)

**Source code:** [DK64Rekong-RetroAchievements on GitHub](https://github.com/Nikamimi/DK64Rekong-RetroAchievements). Tester ZIPs contain the mod files and a short guide, not this Git repository. If you received a ZIP and want the code, use the GitHub link separately.

> **Unofficial RetroAchievements integration.** The owner completed the final Windows test for v0.2.2. The Linux x86-64 companion has passed build and offline tests but has **not** been tested in a native Linux game session or on Steam Deck.

Recommended to play with Vanilla DK Rekongpiled. Compatibility with other mods is not guaranteed, but feel free to try.

## Trying the mod

Download a platform ZIP from [GitHub Releases](https://github.com/Nikamimi/DK64Rekong-RetroAchievements/releases). The current release is **v0.2.2**; GitHub's automatic "Source code" downloads are not installable packages. Each platform ZIP includes installation notes and checksums. The combined `dk64-ra.zip` includes both native companions and is intended for the mod-list download.

You need DK64 Rekongpiled 1.0.2 and your own supported US retail DK64 ROM. The mod checks the game's stored `DK64.z64` before tracking or connecting; it does not copy the ROM. On Windows, a portable install stores it beside the executable; a standard install uses `%LOCALAPPDATA%\DK64Recompiled`. Close the game and place **both** mod files directly in the game's active `mods` folder:

| Platform | Files to install together | Instructions |
| --- | --- | --- |
| Windows | `dk64_ra_probe.nrm` + `dk64_ra_probe.dll` | [Windows tester guide](docs/private-tester-guide.md) |
| Native Linux x86-64 (experimental) | `dk64_ra_probe.nrm` + `dk64_ra_probe.so` | [Linux smoke-test guide](docs/private-linux-tester-guide.md) |

The game's `.nrm` installer alone will not install the native companion. Do not use the Linux `.so` with the Windows game under Proton. Enable the mod in the game's Mods menu, select your achievement mode, and restart after changing modes.

- **Online (default)** signs in to RetroAchievements and submits real **softcore** unlocks. Hardcore and leaderboards are disabled. Only one zero-point achievement, DK Rap, has been confirmed on the RA server in a Windows test; broader achievement accuracy is unverified.
- **Local Tracking only** never signs in or submits awards. It requires this user account to have cached the DK64 set during a previous successful Online load. On a fresh install without that cache, it cannot evaluate achievements. Local triggers are not RA awards and will not later be uploaded automatically.

Press **F8** to open the achievement browser; **F8** or **Esc** closes it. The browser shows a badge grid: hover a badge or browse with arrow keys, D-pad, or left stick to see its title, description, points, progress, and award status. Enter / controller A activates filters and controls; Tab moves between sections. Controller shortcuts still need physical testing. Open **F8 > Account** to see the signed-in username, sign in, or log out. Configure links to this panel because Rekongpiled 1.0.2 only supports static mod settings. RA sign-in does not provide an email address. Choose **Save** in the Windows sign-in window to remember a login token (not a password) in Credential Manager. Log Out removes that token and resumes cached local tracking, preserving RA awards and game saves. Linux asks on each Online launch. Never send your ROM, credentials, token, save files, or unreviewed logs to a tester or issue thread.

## Building from source

The root `thumb.png` is embedded in the `.nrm` by `mod.toml`, so the game and the [DK64 mod website's fetcher](https://github.com/Killklli/DK64RecompWebsite/blob/main/fetch_mods.py) can read it. Replace `thumb.png` to update the artwork before rebuilding. A website listing also needs a repository entry in that site's `config.json` and an eligible GitHub release; adding the thumbnail alone does not publish a listing.

The repository contains the MIPS mod in `src/`, the native Windows/Linux companion in `native/`, and offline tests in `tests/`. Build outputs, ROMs, local caches, and credentials are not tracked. Building requires symbols from the upstream Rekongpiled 1.0.2 tag in the ignored `reference/dk64` directory; the mod does not require a ROM to compile.

```sh
git clone https://github.com/Nikamimi/DK64Rekong-RetroAchievements.git
cd DK64Rekong-RetroAchievements
git clone --depth 1 --branch 1.0.2 https://github.com/Rainchus/Donkey-Kong-64-Recompiled.git reference/dk64
make
RecompModTool mod.toml build
cmake -S native -B build/native-win -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build/native-win --config Release
ctest --test-dir build/native-win -C Release --output-on-failure
```

The first two build commands need a Clang MIPS target, `ld.lld`, `make`, and a matching `RecompModTool`; the Windows native build needs Visual Studio 2022. For Linux dependencies, native build commands, and packaging, see [Linux development notes](docs/linux-development.md). CMake fetches a pinned `rcheevos` revision. Never commit or bundle a ROM, account data, local cache, or diagnostic logs.

After building and testing both platforms from a clean checkout, run `./scripts/package_release.ps1 -Tag v0.2.2` in PowerShell to create separate Windows/Linux ZIPs, the combined `dk64-ra.zip`, and an outer checksum file under `build/releases/`. This packages only allowlisted files and records the source commit; it does not publish anything. See the [v0.2.2 release notes](docs/releases/v0.2.2.md) for compatibility and known limits, and the [mod-list entry](docs/mod-list.md) for the website configuration.

## Project notes

- [Integration and outstanding verification](docs/integration.md) — technical behavior, test evidence, and release gates.
- [RetroAchievements coordination](docs/ra-coordination.md) — client and set coordination.
- [Third-party notices](THIRD_PARTY_NOTICES.md) — dependencies and attribution.

Authenticated network calls can currently pause a game frame, and there is no durable queue for unconfirmed online awards across restarts. The verified-ROM check is not yet bound to the exact bytes selected by the running game. These and broader gameplay parity remain gates for a stable, supported release; publishing an experimental preview does not resolve them.
