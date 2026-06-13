# Sharing Clam during development

How to build, install, and share **Clam** (`paxcirlot.clam`) before it is on the Geode Index.

## Package name

After building, the installable file is:

```
build/paxcirlot.clam.geode
```

CI produces the same combined package for all platforms (Windows, macOS, Android, iOS).

---

## Local development (you)

### Build and auto-install

With a Geode CLI profile configured:

```bash
export GEODE_SDK=/Users/Shared/Geode/sdk   # adjust if your SDK path differs
geode build
```

This configures, builds, packages, and installs to your Geometry Dash profile.

### Install an existing build

```bash
geode package install build/paxcirlot.clam.geode
```

### Open GD from your profile

```bash
geode profile run
```

---

## Share with friends (dev builds)

### Option A — GitHub Release (recommended)

Best for cross-platform testing and permanent download links.

1. Bump `version` in `mod.json` (e.g. `1.0.0` → `1.0.1-beta.1`).
2. Commit and push.
3. Create and push a matching git tag:

   ```bash
   git tag v1.0.1-beta.1
   git push origin v1.0.1-beta.1
   ```

4. The **Release Geode Mod** workflow builds all platforms and uploads `paxcirlot.clam.geode` to GitHub Releases.
5. Send friends the release URL.

**Rules:**

- Always create a **new** release/tag for each version.
- **Never replace** assets on an old release — Geode checksums packages; replacing files breaks installs and Index links.
- Tag should match `mod.json`: version `1.0.1-beta.1` → tag `v1.0.1-beta.1`.

Mark early builds as **Pre-release** on GitHub if you want.

### Option B — CI artifact (quick internal test)

1. Push to GitHub (any branch).
2. Open **Actions** → latest **Build Geode Mod** run.
3. Download the **Build Output** artifact.
4. Send the `.geode` file to testers.

Artifacts expire after a while; use releases for anything you want to link to later.

### Option C — Direct file (fastest for 1–2 people)

After `geode build`, share `build/paxcirlot.clam.geode` via Discord, AirDrop, Drive, etc.

---

## How testers install

### In-game

1. Open Geometry Dash → **Geode** mods menu.
2. Use the **folder** button (bottom-left) to open the mods directory, **or** use **install from file** and pick the `.geode`.
3. Drop/select `paxcirlot.clam.geode`.
4. Restart Geometry Dash.

### CLI (same machine)

```bash
geode package install /path/to/paxcirlot.clam.geode
```

### Android

Copy the file to:

```
/storage/emulated/0/Android/media/com.geode.launcher/game/geode/mods/
```

Then restart the game.

### Option D — Mocha Modding (saltmine.me)

After a **GitHub Release** tag (universal cross-platform `.geode`):

```bash
./scripts/fetch-release-geode.sh 1.1.6
PLATFORM=universal ./scripts/deploy-mocha.sh build/release/paxcirlot.clam.geode
```

Or one command (commit, tag, push, wait for CI, download, upload universal):

```bash
./scripts/release-and-deploy.sh
```

Always upload the **universal** build (`PLATFORM=universal`) so Windows, macOS, Android, and iOS are covered in one package. Local `geode build` on Mac only produces a macOS binary — use the release artifact for Mocha.

---

## Geode Index (public / beta)

Use the Index when the mod is stable enough for the in-game Download tab — betas are fine (`1.0.0-beta.1`).

```bash
geode index login
geode index mods create https://github.com/Bit-Byte5/Clam/releases/download/v1.0.0/paxcirlot.clam.geode
```

Updates:

```bash
geode index mods update https://github.com/Bit-Byte5/Clam/releases/download/v1.0.1/paxcirlot.clam.geode
```

The URL must be a **direct download** link to the `.geode` file (GitHub release assets work).

First submission requires Index approval. For broken WIP builds, stick to GitHub Releases or direct `.geode` sharing instead.

---

## Checklist before each share

| Step | Action |
|------|--------|
| 1 | Bump `version` in `mod.json` |
| 2 | Update `changelog.md` (optional but good practice) |
| 3 | `geode build` locally to sanity-check |
| 4 | Push + tag `v<version>` for a release, **or** share the local `.geode` |
| 5 | Tell testers which GD / Geode version you built against (`mod.json` → `gd`, `geode`) |

---

## Do / Don't

| Do | Don't |
|----|-------|
| Share `.geode` packages | Share raw `.dylib` / `.dll` |
| New GitHub release per version | Overwrite old release files |
| Use semver / beta tags | Reuse the same version for different builds |
| Let CI build all platforms | Expect your Mac build to work on Windows |

---

## Related

- [Geode: Creating and building a mod](https://docs.geode-sdk.org/getting-started/create-mod/)
- [Geode: Publishing mods](https://docs.geode-sdk.org/mods/publishing/)
- Repo: https://github.com/Bit-Byte5/Clam
