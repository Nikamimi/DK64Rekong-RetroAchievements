# DK64 RetroAchievements — experimental Windows x86-64 preview

**Source code:** [GitHub repository](https://github.com/Nikamimi/DK64Rekong-RetroAchievements). This tester ZIP contains the mod binaries and instructions, not the Git repository. Use that link separately if you want to inspect or build the code.

This GitHub pre-release is for testing, not a stable or certified RA client.
The in-game mod name retains its earlier **private beta** label.
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
   portable Windows Rekongpiled 1.0.2 installation. The mod checks the nearby
   `DK64.z64` against the supported hash before connecting.
2. Put `dk64_ra_probe.nrm` and `dk64_ra_probe.dll` together directly in the
   game's `mods` folder. The game's `.nrm` installer does **not** install the
   companion DLL on its own.
3. Enable **DK64 RetroAchievements (private beta)** in Mods. Configure the
   mode before starting the game, and restart the game after any mode change.
4. For Online awards, sign in using the Windows credential dialog yourself.
   **Remember token on this PC** optionally stores an RA login token (not your
   password) in Windows Credential Manager. **Ask each launch** removes it next
   launch. Local mode never prompts for a login.

**Online (softcore beta)** is the default and submits real softcore achievement
unlocks to your RA account. Choose **Local Tracking only** if you want to
observe triggers without submitting anything. If Online sign-in or loading
fails, a previously cached set automatically takes over as Local Tracking.
The RA set must have been loaded Online on this PC at least once; on a fresh
installation with no account and no cached set, tracking cannot evaluate the
full set and the browser says so. The set cache remains on this PC and is not
included in the mod or tester ZIP. Hardcore and leaderboards are off in both
modes. Online requires a connection; there is no durable queue to replay
unconfirmed awards after restarting the game.

## Controls and notifications

- **F8** or **R + D-pad Up** opens the achievement browser.
- **F8**, **Esc**, or controller **B** closes it. Controller shortcuts still
  need physical-device testing.
- Filter All / Locked / Unlocked, page through the set, and inspect badge art,
  measured progress, and status. The browser header shows the current RA
  username or "RA: not signed in"; the game's built-in Configure page cannot
  update its static settings with a live username. **Unlocked on RA** means
  the account award was confirmed; **Tracked locally** does not award RA points.
- Achievement toasts show just the unlocked badge, title and description, plus an original, short
  percussion/marimba cue. Set **Achievement jingle → Off** to mute this cue.
- **Reset local progress** opens a Confirm/Cancel modal. It clears only this
  mod's local trigger history for the current account or Guest profile and ROM; it does not reset
  RA awards or game saves. Cancel was live-tested; Confirm is unit-tested but
  has not been invoked against the owner's live record.

This is a feasibility beta, not a certified or supported stable client. Network calls
can briefly pause the game, other mods and platform builds are unverified,
and the check of the nearby ROM file is not yet bound to the exact ROM bytes
selected by the running game. Please report the achievement name/ID, game
state, what the browser showed, and any toast behavior. Never send your ROM,
password, token, saves, or account-private data. Share the official GitHub
pre-release link with its warnings rather than presenting it as approved RA
support. This release is not a submission to the Rekongpiled mod website.
