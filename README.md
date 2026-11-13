# Baroque (Sega Saturn) on SaturnRecomp

Play Sting's **Baroque** (Sega Saturn, 1998) natively on Windows, using
[SaturnRecomp](https://github.com/sonsegajp/SaturnRecomp) by sonsegajp, an
experimental clean-room Sega Saturn runtime. The English fan translation is
supported.

SaturnRecomp runs the game's original SH-2 code through a fast interpreter on
real Saturn firmware. Despite the project's name it is **not** a finished static
recompilation (see its README). This kit adds the fixes Baroque needs, a
launcher, and settings that make it pleasant to play on a laptop.

> **You need your own game and BIOS.** This kit contains no Sega BIOS, no game
> data, and no SaturnRecomp code. On first launch it downloads SaturnRecomp's
> public source from the author's GitHub and builds it on your PC.

## What works

- Boots through the real Saturn BIOS into the opening movie, title, menus and
  full gameplay in the dungeon. Tested by the author through the opening area,
  not a full playthrough.
- **Full speed (60 fps)** on a 15 W Intel i7-1355U laptop when plugged in, at internal resolutions up to 3x. Battery
  play benefits from the power-mode switch below, but hasn't been measured in
  gameplay.
- **Saves**: Saturn internal backup RAM is kept in `game\backup.bin` in your
  [data folder](#where-your-files-live), outside the app folder.
- **Widescreen**: Baroque's own 16:9 mode, displayed correctly (see below).
- **Action Replay / GameShark codes** (see below).
- Keyboard and SDL-compatible controllers.

## What you need

- Windows 10 or 11, 64-bit, with a Vulkan-capable GPU (any recent Intel, AMD or NVIDIA).
- **Baroque (Japan), product T-33901G**, as a CUE/BIN disc image or a `.chd`,
  dumped from your own disc. The original Japanese release and the English fan translation
  (tested: v1.0.1) both identify as T-33901G.
- **A Sega Saturn BIOS** dumped from your own console (512 KB). A Japanese BIOS is
  recommended for this Japanese disc. The kit author used JP v1.01.
- About **1.5 GB** of free space and an internet connection for the first launch.

## Install

**Quiver Launcher:** add Baroque from the catalog and press Play.

**By hand:** extract the release zip anywhere and run `Baroque.exe`.

On first launch a setup window opens:

1. It asks for your Saturn BIOS, then your Baroque `.cue` (or `.chd`) file, and checks both.
   It reads the disc's header to confirm it is Baroque, T-33901G.
2. It downloads a portable copy of MSYS2 (the GCC compiler, about 1 GB, kept in
   `tools\` in the data folder). The download is verified by checksum.
3. It downloads SaturnRecomp at a fixed commit (`26c9715`), applies this kit's
   patches, and builds the runtime. That takes about 5 to 10 minutes. It also
   fetches libchdr (about 1 MB, checked file by file) so a `.chd` can be read directly.
4. The game starts. Later launches start straight away.

To choose a different disc or BIOS, run `Baroque.exe --setup`. To rebuild the
runtime, run `Baroque.exe --rebuild`.

## Controls

| Saturn | Keyboard | Xbox-style controller |
| --- | --- | --- |
| D-pad | Arrow keys or WASD | D-pad or left stick |
| Start | Enter | Start |
| A / B / C | Z / X / C | A / B / RB |
| X / Y / Z | R / T / Y | X / Y / LB |
| L / R | Q / E | LT / RT |

**F1** opens the runtime's settings (resolution, fullscreen, filtering, volume).
**Esc** quits. The game starts fullscreen. Turn that off in F1 or with
`Fullscreen=0` in `settings.ini`.

## Higher resolution

**F1 > Graphics > Internal resolution** renders the game at up to **8x** the
Saturn's resolution. Plain SaturnRecomp stops at 4x. **6x** draws Baroque at
2112 x 1344, about 1440p, with much sharper walls and polygon edges. The 2D
HUD keeps its original artwork. You can also set `InternalScale=6` in
`settings.ini`. **Texture filtering** and **FXAA** in the same menu only take
effect above 1x.

Before v0.1.4, these settings did nothing in the dungeon itself, only in the
BIOS logo and menus. The dungeon was always drawn at the native 352 x 224.
v0.1.4 fixes that, so higher scales now really cost GPU time during play.

How high to go depends on your graphics. On a 15 W laptop with Intel Iris Xe
graphics, **3x** (1056 x 672) stayed at full speed in the dungeon, with 0.7%
of frames late, the same as 1x. 4x had 15% late frames and 5x had 29%. A
dedicated graphics card should manage 6x or more. If the dungeon stutters,
lower the scale.

## Widescreen

Baroque has a built-in anamorphic widescreen mode for 16:9 TVs. To use it:

1. In the game, go to **Settings > TV Type** and choose **Wide**. The game keeps this in your save.
2. In `settings.ini` (in the data folder), set `Widescreen=1`.

The dungeon then shows a wider view at correct proportions. Menus and movies
look stretched in this mode, exactly as they did on a widescreen TV.

## Audio buffer

`Baroque.exe` keeps 2048 frames (about 46 ms) of sound queued, twice
SaturnRecomp's default. In testing, that cut audio dropouts during the Sega
Saturn boot logo from 12 to 1. To change it, set `AudioBuffer=` under `[Audio]`
in `settings.ini`. Lower values (down to 256) reduce latency, and higher values
(up to 8192) help a slower PC.

## Cheats

Edit `game\cheats.txt` in the data folder. Change the `-` in front of a cheat's
name to `+` to turn it on. The file is re-read while the game runs. Included, all off by default:

- Infinite HP, Infinite VT (checked against the game's memory)
- Faster level ups / max EXP, Max status (from almarsguides.com)

You can paste in other Saturn Action Replay codes. Types `1` (16-bit write),
`3` (8-bit write) and `D` (16-bit compare) are supported. Master codes (`F`/`B`)
are not needed and are skipped.

## Performance and your power settings

Saturn emulation leans on one CPU core, so `Baroque.exe`:

- keeps the game on your fastest CPU cores at high priority, and
- **while the game runs**, switches Windows' power mode (Settings > System >
  Power) to **Best performance** for the power source in use, then **puts your
  original setting back when you quit**.

If you plug in or unplug mid-game, the other source is restored the next time
you switch to it. A pending restore is kept in `power-restore.txt` until it's
done.

## What this kit changes in SaturnRecomp

All of these are general fixes and features, not Baroque-specific hacks. They
are in `kit\patches` and were offered back to the author in
[sonsegajp/SaturnRecomp#1](https://github.com/sonsegajp/SaturnRecomp/issues/1).

- **CD block `GetSectorInfo` (0x54) was missing.** Baroque re-read its sound
  driver forever and never left the BIOS.
- **The master SH-2's on-chip DMA interrupt was starved.** Baroque waits for it
  when entering the dungeon, which gave a black screen. (The slave CPU had
  already been fixed the same way upstream.)
- **Backup RAM was never saved.** In-game saves were lost on exit.
- **Keyboard focus** when the game is started from a launcher.
- **A `Widescreen` setting** for 16:9 presentation.
- **Internal resolution up to 8x** (was 4x).
- **Upscaling in the dungeon.** Baroque clears the screen there with a full-screen
  polygon instead of the VDP1 erase, so upscaling, texture filtering and FXAA
  fell back to the native picture. A full-screen opaque polygon now counts as a clear.
- **`.chd` disc images** (`0003-chd-disc-support.patch`). The disc layer reads a MAME CD
  `.chd` through [libchdr](https://github.com/rtissera/libchdr) (BSD-3-Clause, pinned in
  `kit\libchdr.sha256`) as if it were one BIN, so `.cue`/`.bin` and `.iso` behave exactly as
  before. Only raw 2352-byte CD images are supported, which is what Saturn CHDs are.
- **Action Replay code support** (`cheats.txt`).
- Two small diagnostics improvements.

The runtime is built with a performance profile (PGO) recorded while playing
Baroque. If your compiler version can't use it, setup builds the standard
version instead, which is a little slower.

## Where your files live

Since v0.1.2, your saves, settings and the build cache are kept **outside the
app folder**, in a per-user data folder:

```
%LOCALAPPDATA%\Baroque-SaturnRecomp
```

Paste that into File Explorer's address bar to open it. A launcher update
(Quiver replaces the whole app folder) therefore no longer touches your saves,
and the runtime doesn't have to be rebuilt from a fresh 1 GB download.

```
App folder (replaced by updates)
  Baroque.exe          launcher (run this)
  kit\                 setup script, patches, profile, templates
  portable.txt         optional, see below

Data folder (%LOCALAPPDATA%\Baroque-SaturnRecomp)
  game\backup.bin      your saves
  game\cheats.txt      Action Replay codes
  game\game.toml       which disc to play (written by setup)
  game\console.bin     Saturn clock/language settings
  game\runtime.log     log of the last game session
  settings.ini         runtime settings (F1 menu)
  bios\saturn.bin      a copy of your BIOS
  runtime\             the built runtime
  tools\, build\       build tools and SaturnRecomp source, about 1 GB (can be
                       deleted; they are downloaded again if a rebuild is needed)
  setup.log            log of the last setup
  launcher.log         what the launcher moved, and errors
  power-restore.txt    only while a power-mode restore is pending
```

**Backing up:** copy `game\backup.bin` from the data folder. To restore, quit
the game and put it back in the same place.

**Updating from v0.1.1 or older:** the first launch of v0.1.2 moves your files
from the app folder to the data folder: saves, cheats, settings, BIOS copy and
the build tools. Anything already in the data folder is never overwritten; the
old copy is then left where it was and `launcher.log` says so. If an update
already replaced the app folder and you saved `backup.bin` by hand, put it in
the data folder's `game\` folder (create it if needed), or in the new app
folder's `game\` folder before the first launch.

**Portable mode:** create an empty file named `portable.txt` next to
`Baroque.exe` to keep everything in the app folder instead, as v0.1.1 did.
Updates that replace the app folder then delete your saves, so back them up.

The disc image is never copied. Keep it outside the app folder too, or setup
will ask for it again after an update.

## Troubleshooting

- **Setup failed:** `setup.log` in the data folder has the details. Run
  `Baroque.exe` again to retry; finished steps are skipped.
- **"Not Baroque (T-33901G)":** choose the `.cue` (or `.chd`) of the Japanese disc, and keep
  every `.bin` a `.cue` lists next to it.
- **"That CHD is not a Sega Saturn disc image":** the `.chd` is not a raw Saturn CD
  (a DVD or PS2 image, say). Use a CD `.chd` or the `.cue`.
- **The game stopped with an error:** see `game\runtime.log` in the data folder.
- **Slowdown:** lower the internal resolution in F1 (3x is smooth on Intel Iris Xe), close other heavy programs, and on a laptop, plug in.

## RecompLauncher

`recomplauncher.json` and `prepare-baroque.cmd` let RecompLauncher ask for the disc (`.cue`,
`.chd` or `.iso`) and the BIOS on its setup screen, run setup without windows, and start
the game. They call the same `kit\setup.ps1` as `Baroque.exe`.

The launcher's game page also shows the options from `settings.ini`: window mode, internal
resolution (1x to 8x), texture filtering, FXAA, model smoothing, widescreen, frame interpolation,
volume, mute and the audio buffer. It edits the same file F1 does, in the data folder (or beside
`Baroque.exe` with `portable.txt`), so both always agree.

## Legal

This project is not affiliated with or endorsed by Sega, Sting or the
SaturnRecomp author. Baroque is a trademark of its owners. No BIOS, game data or
SaturnRecomp code is distributed here. SaturnRecomp has no license at the time
of writing, so it is built from the author's public source on your PC, and this
repository contains only patches. Dump your own disc and BIOS.

This kit (launcher, setup, patches, configuration) is MIT licensed; see
`LICENSE`. It was developed with AI assistance (Claude).
