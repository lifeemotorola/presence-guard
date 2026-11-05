# Auto Scene Adjust

**The plugin that puts your scene back the way it should be.**

Every streamer has done it: the camera is half off the frame, the game capture
is squashed into a square, the webcam is a postage stamp, or you went vertical
for a TikTok clip and every source stayed where it was. Auto Scene Adjust looks
at the scene on screen, says plainly what is wrong, and fixes it — once, or
automatically from then on.

| | |
|---|---|
| OBS version | built against **33.0.1** headers |
| Runtime deps | none beyond the Qt6 libraries OBS already ships |
| Config | `%APPDATA%\obs-studio\plugin_config\obs-auto-scene\settings.json` |
| Tests | `tests/run-tests.sh` — 120 checks, no OBS or Qt needed |

## What it says about your sources

Every source in the current scene is measured against the canvas and reported
as one of:

| State | Means |
|---|---|
| **Fits** | on screen, correctly proportioned, big enough |
| **Cut off** | hangs off an edge |
| **Off canvas** | parked entirely outside — invisible, and usually a mistake |
| **Tiny** | on screen, but a speck |
| **Stretched** | the drawn shape does not match the source (a 4:3 camera dragged to fill 16:9) |
| **Rotated** | rotated: reported, never touched — that is a decision, not a bug |
| **Locked** | locked in OBS: reported, never touched |
| **No size** | a source with nothing to draw yet |

Ordered by the problems that ruin a stream first: being invisible beats being
squashed, which beats being small.

## The four fixes

Pick one per source in the **Automatically** column:

- **Fit** — the whole source, letterboxed if it has to be. Nothing is lost.
- **Fill** — covers the frame; the overflow is cropped. For a camera in a
  vertical canvas this is usually what you want.
- **Centre** — back to its own size, in the middle. Shrinks only if it cannot
  fit; never enlarges. The "put my webcam back how it was" button.
- **Stretch** — takes the frame exactly, shape be damned.

**Margin** keeps a border around the canvas, and **Anchor** decides where the
result sits (nine positions). Rotated and locked sources are left alone, and a
source that is fine is not touched just because you looked at it.

## The automatic half

Adopting a source (anything but **Off**) means it is placed again whenever:

- **the canvas changes size** — 16:9 → 9:16 in Settings → Video is the classic
  disaster, and every adopted source in *every* scene is re-fitted, not just the
  one you are looking at;
- **the source itself changes size** — a browser source that reloaded at another
  resolution, a slideshow frame, a window capture that came back different.

Sources you have not adopted are never moved. Opening a scene never rearranges
it. Untick **Keep adopted sources fitted** to stop all of it without losing your
choices.

## Layout presets

Three one-click arrangements of everything in the scene that OBS lets the plugin
move:

| Button | Result |
|---|---|
| **Grid** | even cells, a short last row centred |
| **Side by side** | equal columns, full height |
| **Main + cam** | the first source fills the frame, the rest stack in the corner |

**Gap** spaces the cells. Every preset can be undone with **Undo** — a button
that rearranges your scene and cannot be undone is a trap.

## Get it

1. **Actions → Auto Scene Adjust → the newest green run → Artifacts →
   `obs-auto-scene-windows-x64`.** The artifact is a zip inside a zip; extract
   twice.
2. Close OBS, then in the extracted folder:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\Install.ps1
   ```

   It installs to `C:\ProgramData\obs-studio\plugins\obs-auto-scene\`:

   ```
   C:\ProgramData\obs-studio\plugins\obs-auto-scene\obs-auto-scene.dll
   C:\ProgramData\obs-studio\plugins\obs-auto-scene\data\locale\en-US.ini
   ```

   On OBS **32 or older** add `-LegacyLayout`; `Uninstall.ps1` removes either
   layout.
3. Restart OBS → **Docks → Auto Scene Adjust**.
4. Press **Fit this scene** once. Then arrange a scene with the presets and
   watch the State column: if it still says something is wrong, the dock is
   telling you what OBS is doing, not what the plugin wanted.

Hotkeys live in **Settings → Hotkeys**: fit this scene, undo, and automatic
fitting on or off.

## Good to know

- **It writes ordinary OBS transforms.** There are no hidden settings in your
  scene collection: uninstall the plugin and your scenes stay exactly as it left
  them.
- **It takes over the transform of a source it adopts**, including clearing a
  bounding box — a bounding box overrides the scale the plugin computes, so
  leaving it on would make the button do nothing. Rotation and crop are kept.
- **Groups are left alone.** Nested transforms are relative to the group, and
  quietly moving things inside someone's group is how plugins get uninstalled.
- **Everything happens on the OBS UI thread**, 700 ms apart. Nothing runs during
  a transition.
- **A scene with a rotated camera will keep saying "Rotated".** That is the
  plugin being honest rather than guessing which way you wanted it.

## Build it

Self-contained: its own `buildspec.json`, `CMakePresets.json` and workflow.

**In the cloud (no compiler needed)** — push, then **Actions → Auto Scene
Adjust**. The workflow runs the geometry tests on Ubuntu and builds on a Windows
runner with OBS 33.0.1 headers and Qt6.

**Locally** — from this folder, in a Visual Studio 2022 prompt:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
cmake --install build_x64 --config RelWithDebInfo --prefix build_x64\install
```

The geometry is pure C++17 — no OBS, no Qt — so its tests run anywhere:

```bash
./tests/run-tests.sh
```

120 checks: fit, fill, centre and stretch against 16:9, 4:3, vertical, square
and 4K sources; all nine anchors; margins; scale limits; the audit deciding
between fine, cut off, off canvas, tiny, stretched, rotated and no size; the
exact pixel splits behind every layout; and the rule for when a source deserves
a second placement.

## Layout

```
auto-scene/
├─ src/
│ ├─ scene-core.hpp/.cpp    rectangles, fitting, auditing, layouts (pure C++)
│ ├─ scene-adjust.hpp/.cpp  reads the scene, applies fixes, watches sizes
│ ├─ scene-dock.hpp/.cpp    the dock
│ └─ plugin-main.cpp        module load/unload, dock, hotkeys
├─ data/locale/en-US.ini
├─ tests/                   the geometry tests and their runner
└─ windows/                 Install.ps1, Uninstall.ps1
```
