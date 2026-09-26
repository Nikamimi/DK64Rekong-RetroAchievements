# Rekongpiled mod-list entry

Merge this entry into the existing `github_sources` array in the website's `config.json`:

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

The combined download contains the `.nrm`, Windows `.dll`, Linux `.so`, and platform installation guides. Its stable filename avoids changing the entry for each version. The `.nrm` embeds `mod.json` and `thumb.png`, which the [website fetcher](https://github.com/Killklli/DK64RecompWebsite/blob/main/fetch_mods.py) reads from the ZIP. It uses GitHub's latest published release.

**Latest release:** [v0.2.2](https://github.com/Nikamimi/DK64Rekong-RetroAchievements/releases/latest). **Stable download link:** [dk64-ra.zip](https://github.com/Nikamimi/DK64Rekong-RetroAchievements/releases/latest/download/dk64-ra.zip).

Users must extract the ZIP and install the `.nrm` with the native companion for their game. The in-game `.nrm` installer alone does not install the companion. Native Linux has only build and offline-test validation so far.
