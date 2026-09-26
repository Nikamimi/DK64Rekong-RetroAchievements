# DK64 RetroAchievements — Windows x86-64 installation

**Source code:** [GitHub repository](https://github.com/Nikamimi/DK64Rekong-RetroAchievements). This tester ZIP contains the mod binaries and instructions, not the Git repository. Use that link separately if you want to inspect or build the code.

This preview package is for testing, not a stable or certified RA client.
It uses the existing RetroAchievements DK64
N64 set (game 10075), not a separate achievement set. Only the DK Rap
achievement has been confirmed live in Online mode so far; it awards zero
points. Other achievements and game-state edge cases are still being checked.

## Install

This x86-64 build requires the Microsoft Visual C++ v14 x64 runtime
(`MSVCP140.dll`, `VCRUNTIME140.dll`, and `VCRUNTIME140_1.dll`). If Windows reports
a missing runtime DLL, install the official Microsoft runtime, not individual
DLLs from a third-party download site. ARM64-native game builds are not supported.

1. Close DK64 Rekongpiled. Use the supported US retail DK64 ROM in your own
   Windows Rekongpiled 1.0.2 installation. The mod checks the game's stored
   `DK64.z64` against the supported hash before connecting. With a portable
   install (one that already has `portable.txt` beside `DK64Recompiled.exe`),
   the stored ROM is beside the executable; otherwise it is under
   `%LOCALAPPDATA%\DK64Recompiled`. Do not create `portable.txt` just for this
   mod: doing so changes where the game looks for saves, settings, and mods.
2. Put `dk64_ra_probe.nrm` and `dk64_ra_probe.dll` together directly in the
   game's active `mods` folder: beside the executable for a portable install,
   or in `%LOCALAPPDATA%\DK64Recompiled\mods` for a standard install. The
   game's `.nrm` installer does **not** install the companion DLL on its own.
3. Enable **DK64 RetroAchievements** in Mods. Configure the
   mode before starting the game, and restart the game after any mode change.
4. For Online awards, sign in using the Windows credential dialog yourself.
   Select **Save** in that sign-in window to remember an RA login token (not
   your password) in Windows Credential Manager. Leave it unchecked for this
   session only. **F8 > Account > Log Out** removes the token and switches to
   cached local tracking without changing RA awards or game saves. You can
   sign in again from Account. Configure points to this panel; RA sign-in
   does not expose email addresses. Local mode never prompts automatically.

**Online** is the default and submits real softcore achievement
unlocks to your RA account. Choose **Local Tracking only** if you want to
observe triggers without submitting anything. If Online sign-in or loading
fails, a previously cached set automatically takes over as Local Tracking.
The RA set must have been loaded Online on this PC at least once; on a fresh
installation with no account and no cached set, tracking cannot evaluate the
full set and the browser says so. The set cache remains on this PC and is not
included in the mod or tester ZIP. Hardcore and leaderboards are off in both
modes. Online requires a connection; there is no durable queue to replay
unconfirmed awards after restarting the game.

If the browser says **stored DK64.z64 not found**, check which game installation
and data folder you launched; this is distinct from **ROM mismatch**, which
means a stored ROM was hashed but is not the supported US retail copy. A file
that cannot be read or hashed has its own error. Do not send anyone a ROM or
move game saves to troubleshoot the mod.

## Controls and notifications

- **F8** or **R + D-pad Up** opens the achievement browser.
- **F8**, **Esc**, or controller **B** closes it. Controller shortcuts still
  need physical-device testing.
- Hover badges or browse the grid with arrow keys, D-pad, or left stick.
  The detail card shows the selected title, description, points, progress, and status.
  Enter / controller A activates All / Locked / Unlocked and other controls;
  Tab moves between sections, Home / End selects the first / last badge.
  Larger sets scroll by row with the wheel or navigation; there is no page slider. The browser header shows the current RA
  username or "RA: not signed in"; the game's built-in Configure page cannot
  update its static settings with a live username. **Unlocked on RA** means
  the account award was confirmed; **Tracked locally** does not award RA points.
- Achievement toasts show just the unlocked badge, title and description, plus an original, short
  percussion/marimba cue. Set **Achievement jingle → Off** to mute this cue.
- **Reset local progress** opens a Confirm/Cancel modal. It clears only this
  mod's local trigger history for the current account or Guest profile and ROM; it does not reset
  RA awards or game saves. Cancel was live-tested; Confirm is unit-tested but
  has not been invoked against the owner's live record.

This is an unofficial integration, not a certified RA client. Network calls
can briefly pause the game, other mods and platform builds are unverified,
and the check of the game's stored ROM file is not yet bound to the exact ROM bytes
selected by the running game. Please report the achievement name/ID, game
state, what the browser showed, and any toast behavior. Never send your ROM,
password, token, saves, or account-private data. The owner approved the v0.2.2 Windows build for publication after a final test.
The mod-list package includes both native companions; install the pair for your game.
Publication does not establish official RetroAchievements client recognition.
