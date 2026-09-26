# Building from source

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

The first two build commands need a Clang MIPS target, `ld.lld`, `make`, and a matching `RecompModTool`; the Windows native build needs Visual Studio 2022. For Linux dependencies, native build commands, and packaging, see [Linux development notes](linux-development.md). CMake fetches a pinned `rcheevos` revision. Never commit or bundle a ROM, account data, local cache, or diagnostic logs.

The root `thumb.png` is embedded in the `.nrm` by `mod.toml`. Replace it before rebuilding to update the artwork. See the [mod-list configuration](mod-list.md) for website integration.

After building and testing both platforms from a clean checkout, run `./scripts/package_release.ps1 -Tag v0.2.2` in PowerShell to create separate Windows/Linux ZIPs, the combined `dk64-ra.zip`, and an outer checksum file under `build/releases/`. This packages only allowlisted files and records the source commit; it does not publish anything.

See [integration notes](integration.md) for test evidence and known limits, [RA coordination](ra-coordination.md) for project history, and [third-party notices](../THIRD_PARTY_NOTICES.md) for attribution.
