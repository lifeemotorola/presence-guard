# Auto Director

A vision mixer that listens. Give Auto Director one take per camera — a scene
plus the microphone that belongs to whoever is on that camera — and it cuts to
whoever is speaking, the way a human director would. It is the combination of
the two plugins in this repository and then some:

| | Presence Guard | Local AI Auto Frame | **Auto Director** |
|---|---|---|---|
| What it does | protects the stream when you are away | keeps one face in frame | **runs the camera cuts while you talk** |
| Input | system idle time, one mic | the video frame | **one meter per take** |
| Scene switching | AFK / dead-air scenes | — | **the loudest speaker, with hysteresis and dwell** |
| Operator control | on/off | filter settings | **dock, 4 hotkeys, dry run, hold, lobby** |

| | |
|---|---|
| OBS version | built against **33.0.1** headers (`buildspec.json`) |
| Runtime deps | none beyond the Qt6 libraries OBS already ships |
| Config | `%APPDATA%\obs-studio\plugin_config\obs-auto-director\settings.json` |
| Tests | `tests/run-tests.sh` — the policy needs no OBS and no Qt |

## How it decides

Every 120 ms the director reads one volume meter per take and answers a single
question: *is the take on air still the right one?*

1. **Someone has to speak above the threshold** (default −45 dB). A take with a
   muted, unplugged or missing microphone counts as *unknown*, never as silence,
   so one dead cable cannot park the show.
2. **A challenger has to win by the hysteresis margin** (default 3 dB) while the
   take on air is still speaking. Two similar microphones therefore do not
   ping-pong: whoever is only marginally louder never steals the shot.
3. **A challenger has to keep talking for the dwell time** (default 4 s). A
   cough, a laugh or a door slam does not cut the camera, and automatic cuts
   cannot happen faster than once per dwell.
4. **The operator always wins.** Change the scene yourself and the director
   backs off for the hold time (default 15 s). Enable studio mode and it stops
   entirely — preview/program is yours.
5. **Silence has a floor.** After the lobby delay (default 30 s, and only if at
   least one meter is alive) it cuts to the lobby/BRB scene, and it comes back
   by itself as soon as somebody speaks.

Everything is logged with the reason it acted, and **Dry run**
logs the decision without touching the live scene — the fastest way to tune a
show before going on air.

## Install it

1. **Actions → Auto Director → the newest green run → Artifacts →
   `obs-auto-director-windows-x64`.** The artifact is a zip inside a zip;
   extract twice.
2. Close OBS, then in the extracted folder:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\Install.ps1
   ```

   It installs to `C:\ProgramData\obs-studio\plugins\obs-auto-director\`:

   ```
   C:\ProgramData\obs-studio\plugins\obs-auto-director\obs-auto-director.dll
   C:\ProgramData\obs-studio\plugins\obs-auto-director\data\locale\en-US.ini
   ```

   On OBS **32 or older** add `-LegacyLayout`; `Uninstall.ps1` removes either
   layout. OBS 33 does not scan `%APPDATA%\obs-studio\plugins` unless
   `OBS_PLUGINS_PATH` is set, which is why this script does not use it.
3. Start OBS → **Docks → Auto Director**.

Check **Help → Log Files → View Current Log** for `[obs-auto-director] loaded`
if the dock does not appear.

## Set it up

1. In the dock press **Add all scenes**. One take per scene appears, with the
   microphone found inside that scene already attached. Scenes you already added
   are untouched.
2. Fix the microphones: this is the only part OBS cannot guess. Each take needs
   *the microphone of the person on that camera* — a webcam's own audio, a USB
   mic, a wireless receiver, or the mixed audio of that scene.
3. Press **Dry run** and run a rehearsal. The status line and the OBS log show
   every cut it would have made, and why.
4. Untick **Dry run**, tick **Direct the show automatically**, go live.

### The rules of thumb

- **Dwell** is your anti-flapping control: 3–5 s for conversation, 8–10 s for a
  panel where people interrupt each other.
- **Hysteresis** is your anti-crosstalk control: raise it when two microphones
  can hear the same room, so only the clearly loudest one wins.
- **Hold** is your escape hatch: press the *Hold* hotkey or change the scene and
  the director stops cutting long enough for you to make your point.
- **Only while streaming or recording** keeps it quiet while you build the show.

## Hotkeys

Set them in **Settings → Hotkeys**, search for "Auto Director":

| Hotkey | Effect |
|---|---|
| Auto Director: switch directing on or off | toggles the big checkbox |
| Auto Director: next take | cuts to the next usable take now, ignoring the dwell |
| Auto Director: hold the current scene | stops cutting for the hold time |
| Auto Director: go to the lobby scene | cuts to the lobby scene and holds |

The dock buttons do the same thing, so nothing is hotkey-only.

## Build it

This plugin is self-contained: its own `buildspec.json`, `CMakePresets.json` and
workflow.

**In the cloud (no compiler needed)** — push, then **Actions → Auto Director**.
The `auto-director-windows.yml` workflow builds the policy tests first, then
builds the plugin on a Windows runner with OBS 33.0.1 headers and Qt6, and
uploads the `obs-auto-director-windows-x64` artifact.

**Locally** — from this folder, in a Visual Studio 2022 prompt:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
cmake --install build_x64 --config RelWithDebInfo --prefix build_x64\install
```

The directing policy is ordinary C++17 with no OBS or Qt in it, so the tests run
anywhere a compiler exists:

```bash
./tests/run-tests.sh
```

They drive the policy with synthetic microphones and a fake clock: speaker
detection, dwell, hysteresis, lobby fallback, dead meters, studio mode, operator
lock and the edge cases around ties and unknown scenes.

## Why it is safe to leave on

- Nothing happens in studio mode, and nothing happens while the director is off
  or while you are off air (if that option is set).
- A take whose meter stopped reporting is never treated as silence, and the
  lobby rule only fires while at least one meter is alive.
- Your manual scene changes always win, for a configurable hold.
- **Dry run** exists so the first session can be a rehearsal.
- Every decision is logged with its reason, and the dock repeats the last one.

## Layout

```
auto-director/
├─ src/
│ ├─ director-core.hpp/.cpp   the policy: pure C++17, no libobs, no Qt
│ ├─ auto-director.hpp/.cpp   meters, clock, scene switching (libobs + Qt)
│  ├─ director-dock.hpp/.cpp   the dock widget
│  └─ plugin-main.cpp          module load/unload, dock, hotkeys
├─ data/locale/en-US.ini
├─ tests/                      the policy tests and their runner
└─ windows/                    Install.ps1, Uninstall.ps1
```
