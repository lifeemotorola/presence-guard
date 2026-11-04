# Hype Clip

**Your best moments, clipped before you can reach for the hotkey.**

Hype Clip listens to every audio source in your scene collection and saves a
clip from OBS's replay buffer when the stream gets loud in a way it has not been
all stream — the scream, the goal, the jump scare, the argument. There is also a
big **Clip that** button (and hotkey) for when you know it was good.

| | |
|---|---|
| OBS version | built against **33.0.1** headers |
| Runtime deps | none beyond the Qt6 libraries OBS already ships |
| Needs | the **replay buffer** switched on (Settings → Output → Replay Buffer) |
| Config | `%APPDATA%\obs-studio\plugin_config\obs-hype-clip\settings.json` |
| Tests | `tests/run-tests.sh` — the detector is pure C++, 46 checks |

## Why it is not just "loud = clip"

Loud is every gunfight, every music drop, every notification. A clip of the
twentieth gunfight is not a clip. Hype Clip asks three questions and needs all
three answers to be yes:

1. **Is it loud in absolute terms?** (`Louder than`, default −12 dB)
2. **Does it stand out from the room the stream is already in?** It keeps a
   running baseline of how loud things usually are and requires a `margin`
   (default 6 dB) above it. A stream that is loud *all the time* — a shooter, a
   concert — lifts its own baseline within a minute and stops clipping itself.
3. **Does it stay loud?** Not for a single sample, for `sustain` (default
   700 ms). A cough, a door slam or a Discord ping is not a moment.

Plus a **cooldown** (default 20 s) so one long scream is one clip, and the
promise that a clip you asked for by hand is never followed by an automatic
duplicate.

All four are adjustable in the dock, and the meter above them shows the live
level and the learned baseline — so when nothing gets clipped, you can see why
instead of guessing.

## Get it

1. **Actions → Hype Clip → the newest green run → Artifacts →
   `obs-hype-clip-windows-x64`.** The artifact is a zip inside a zip; extract
   twice.
2. Close OBS, then in the extracted folder:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\Install.ps1
   ```

   It installs to `C:\ProgramData\obs-studio\plugins\obs-hype-clip\`:

   ```
   C:\ProgramData\obs-studio\plugins\obs-hype-clip\obs-hype-clip.dll
   C:\ProgramData\obs-studio\plugins\obs-hype-clip\data\locale\en-US.ini
   ```

   On OBS **32 or older** add `-LegacyLayout`; `Uninstall.ps1` removes either
   layout.
3. Restart OBS → **Docks → Hype Clip**.

## Set it up (once)

1. **Settings → Output → Replay Buffer → Enable**, and set the length you want
   to keep (30 s is a good start, 60 s if you talk a lot before the payoff).
   Without the replay buffer there is nothing to clip — the dock says so
   plainly, and starting it when a moment is missed is automatic.
2. **Docks → Hype Clip**, then:
   - **Clip that** — try it once, to prove the whole chain works.
   - Tick **Clip hype moments automatically**.
   - Watch the meter for a round. If it clips too much, raise `Louder than`
     (make it less negative) or the `margin`; if it never clips, lower them.
3. **Settings → Hotkeys → search "Hype Clip"** and bind:
   - *Hype Clip: clip that* — the one you will actually press.
   - *Hype Clip: automatic clipping on or off* — for when you want it quiet.

## What you get

Every clip lands in the dock with its time, its score (how loud the moment was)
and why it was taken. From there:

| Button | Does |
|---|---|
| **Open folder** (or double-click a row) | opens the folder the clip was saved to |
| **Copy path** | puts the file path on the clipboard, ready to paste into Discord |
| **Delete file** | removes the clip **and** its file, after you decide it was not good |
| **Clear list** | empties the list, keeps the files |

The list survives restarts, because the good one from yesterday is usually the
one you want to find.

Clips are saved wherever OBS's replay buffer points — that is OBS's setting, not
the plugin's, so the files are also where OBS's own "Show Recordings" expects
them.

## Good to know

- **It listens to everything**, microphones and desktop audio alike, because a
  game's own scream can be the moment. Scenes are skipped: a scene is the sum of
  the sources inside it, and those are metered on their own.
- **Nothing is recorded twice**: a clip is whatever the replay buffer already
  holds in memory, saved to disk.
- **No upload, no cloud, no account.** Clips stay on your disk; where they go
  afterwards is your call (this README would love a "copy to clipboard and paste
  into your Discord" button — that is exactly what **Copy path** is for).

## Build it

Self-contained: its own `buildspec.json`, `CMakePresets.json` and workflow.

**In the cloud (no compiler needed)** — push, then **Actions → Hype Clip**. The
workflow runs the detector tests on Ubuntu, then builds on a Windows runner with
OBS 33.0.1 headers and Qt6 and uploads `obs-hype-clip-windows-x64`.

**Locally** — from this folder, in a Visual Studio 2022 prompt:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
cmake --install build_x64 --config RelWithDebInfo --prefix build_x64\install
```

The detector is pure C++17 — no OBS, no Qt — so its tests run anywhere:

```bash
./tests/run-tests.sh
```

46 checks with synthetic microphones and a fake clock: a quiet room, a shout,
the threshold and the margin, the baseline adapting up *and* back down, breath
-sized drop-outs, real pauses, the cooldown, manual clips not being duplicated,
several microphones with one of them dead, and the status the dock reads.

## Layout

```
hype-clip/
├─ src/
│ ├─ hype-core.hpp/.cpp    when is it worth clipping (pure C++)
│ ├─ hype-clip.hpp/.cpp    meters, replay buffer, clip list (libobs + Qt)
│ ├─ hype-dock.hpp/.cpp    the dock
│ └─ plugin-main.cpp       module load/unload, dock, hotkeys
├─ data/locale/en-US.ini
├─ tests/                  the detector tests and their runner
└─ windows/                Install.ps1, Uninstall.ps1
```
