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
- **Full speed (60 fps)** on a 15 W Intel i7-1355U laptop when plugged in. Battery
  play benefits from the power-mode switch below, but hasn't been measured in
  gameplay.
- **Saves**: Saturn internal backup RAM is kept in `game\backup.bin`.
- **Widescreen**: Baroque's own 16:9 mode, displayed correctly (see below).
- **Action Replay / GameShark codes** (see below).
- Keyboard and SDL-compatible controllers.

## What you need

- Windows 10 or 11, 64-bit, with a Vulkan-capable GPU (any recent Intel, AMD or NVIDIA).
- **Baroque (Japan), product T-33901G**, as a CUE/BIN disc image dumped from your
  own disc. The original Japanese release and the English fan translation
  (tested: v1.0.1) both identify as T-33901G.
- **A Sega Saturn BIOS** dumped from your own console (512 KB). A Japanese BIOS is
  recommended for this Japanese disc. The kit author used JP v1.01.
- About **1.5 GB** of free space and an internet connection for the first launch.

## Install

**Quiver Launcher:** add Baroque from the catalog and press Play.

**By hand:** extract the release zip anywhere and run `Baroque.exe`.

On first launch a setup window opens:

1. It asks for your Saturn BIOS, then your Baroque `.cue` file, and checks both.
   It reads the disc's header to confirm it is Baroque, T-33901G.
2. It downloads a portable copy of MSYS2 (the GCC compiler, about 600 MB, kept in
   `tools\`). The download is verified by checksum.
3. It downloads SaturnRecomp at a fixed commit (`26c9715`), applies this kit's
   patches, and builds the runtime. That takes about 5 to 10 minutes.
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

## Widescreen

Baroque has a built-in anamorphic widescreen mode for 16:9 TVs. To use it:

1. In the game, go to **Settings > TV Type** and choose **Wide**. The game keeps this in your save.
2. In `settings.ini` (in the game folder), set `Widescreen=1`.

The dungeon then shows a wider view at correct proportions. Menus and movies
look stretched in this mode, exactly as they did on a widescreen TV.

## Cheats

Edit `game\cheats.txt`. Change the `-` in front of a cheat's name to `+` to turn
it on. The file is re-read while the game runs. Included, all off by default:

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
- **Action Replay code support** (`cheats.txt`).
- Two small diagnostics improvements.

The runtime is built with a performance profile (PGO) recorded while playing
Baroque. If your compiler version can't use it, setup builds the standard
version instead, which is a little slower.

## Folder layout

```
Baroque.exe            launcher (run this)
settings.ini           runtime settings (created on first launch)
game\                  game.toml, cheats.txt, backup.bin (saves), runtime.log
bios\saturn.bin        a copy of your BIOS
runtime\               the built runtime
tools\, build\         build tools and SaturnRecomp source (can be deleted after
                       setup; they are downloaded again if a rebuild is needed)
kit\                   setup script, patches, profile, templates, setup.log
```

## Troubleshooting

- **Setup failed:** `kit\setup.log` has the details. Run `Baroque.exe` again to
  retry; finished steps are skipped.
- **"Not Baroque (T-33901G)":** choose the `.cue` of the Japanese disc, and keep
  every `.bin` it lists next to it.
- **The game stopped with an error:** see `game\runtime.log`.
- **Slowdown:** close other heavy programs. On a laptop, plug in for the best results.

## Legal

This project is not affiliated with or endorsed by Sega, Sting or the
SaturnRecomp author. Baroque is a trademark of its owners. No BIOS, game data or
SaturnRecomp code is distributed here. SaturnRecomp has no license at the time
of writing, so it is built from the author's public source on your PC, and this
repository contains only patches. Dump your own disc and BIOS.

This kit (launcher, setup, patches, configuration) is MIT licensed; see
`LICENSE`. It was developed with AI assistance (Claude).
