# DK64 RetroAchievements

Adds RetroAchievements to DK64 Rekongpiled, with achievement popups, progress tracking, and an in-game badge browser.

<img src="thumb.png" alt="DK64 RetroAchievements trophy" width="260">

**[Download the latest version](https://github.com/Nikamimi/DK64Rekong-RetroAchievements/releases/latest/download/dk64-ra.zip)** | [All downloads](https://github.com/Nikamimi/DK64Rekong-RetroAchievements/releases/latest)

Recommended to play with Vanilla DK Rekongpiled. Compatibility with other mods is not guaranteed, but feel free to try.

## Install

You'll need DK64 Rekongpiled 1.0.2 and your own supported US retail ROM.

1. Close the game and extract the ZIP.
2. Copy `dk64_ra_probe.nrm` and the file for your platform into the game's `mods` folder: `dk64_ra_probe.dll` on Windows, or `dk64_ra_probe.so` on native Linux.
3. Enable **DK64 RetroAchievements** in the Mods menu, start the game, and sign in to RA.

Both files are required. Linux support is experimental. [Windows help](docs/private-tester-guide.md) | [Linux help](docs/private-linux-tester-guide.md)

## Playing

Press **F8** to browse achievements. Hover a badge or use the arrow keys, D-pad, or left stick to see its details. Open **Account** to sign in or log out, and check **Save** in the Windows sign-in window to remember your login.

Achievements pop up as you unlock them, with an optional jingle. Local tracking is also available after signing in once to load the set; it doesn't award RA points. Hardcore and leaderboards aren't supported.

## What's new in v0.2.2

- A badge grid with mouse, keyboard, and controller navigation.
- An Account panel and a simpler sign-in experience.
- New trophy artwork and a cleaner interface.
- Fixed ROM detection for standard Windows installs.

Found a problem? [Open an issue](https://github.com/Nikamimi/DK64Rekong-RetroAchievements/issues).

<details>
<summary>Mod-list entry and development docs</summary>

Merge this entry into the website's existing `github_sources` array:

```json
{
  "github_sources": [
    {
      "enabled": true,
      "repo": "Nikamimi/DK64Rekong-RetroAchievements",
      "zip_containing_nrm": "dk64-ra.zip",
      "tags": ["feature"]
    }
  ]
}
```

[Mod-list details](docs/mod-list.md) | [Build instructions](docs/building.md) | [Technical notes](docs/integration.md) | [Third-party notices](THIRD_PARTY_NOTICES.md)

</details>
