# Presence Guard — start here

This repository **is** a complete OBS Studio plugin (C++17, Qt6, libobs,
official `obs-plugintemplate` layout) that does the three things I promised,
natively. It sits at the repository root, next to the GitHub Actions pipeline
that builds it, so the "no compiler needed" route below works as written.

| Rule | Watches | Does |
|---|---|---|
| **AFK** | system-wide keyboard/mouse idle time (`GetLastInputInfo`) | switches to your BRB scene, restores your real scene when you touch anything |
| **Dead air** | your mic, through a real libobs volume meter | switches to a "technical difficulties" scene when nobody has spoken for *N* seconds, restores it when you talk |
| **Operator override** | your manual scene changes | never fights you — it only undoes a switch *it* made |

Plus a dock with both rules, live status (`idle 84 s · mic −52.3 dB · threshold
−45 dB`), **Test AFK** / **Restore now** buttons, and settings saved per OBS
profile.

---

## Where is the `.dll`?

**Not in the repository, and it never will be.** `presence-guard.dll` is a build
output: it's compiled on a GitHub runner and published as a *workflow artifact*,
which is a separate download from the source files. Browsing the **Code** tab
will never show it. You want:

**Actions → the newest run → scroll to the bottom → Artifacts**

Direct link to the run that already built it:

<https://github.com/lifeemotorola/presence-guard/actions/runs/41464475516>

Look for `presence-guard-0.1.0-windows-x64-650635120` (587 KB). Note the
double-zip: the artifact is a `.zip` that contains
`presence-guard-0.1.0-windows-x64.zip`, which is what holds the `presence-guard`
folder. Extract both.

## Get the `.dll` (no compiler needed, ~10 minutes)

I can't build a Windows binary from this Linux sandbox — MSVC isn't here — so the
project ships with the official OBS CI workflow. GitHub's Windows runners do the
build for you.

1. Push to the repo (the remote is already configured):

   ```bash
   git push origin main
   ```

   Any push to `main` starts the workflow *"main push run 🚀"*. Working on a
   branch instead? Open a pull request against `main`; the PR run builds the
   plugin too.

2. **Actions** tab → the latest run → wait for **Build for Windows 🪟** (green ✔,
   ~6–10 min the first time; it downloads the OBS 31.1.1 headers, obs-deps and
   Qt6).

3. Download the artifact `presence-guard-0.1.0-windows-x64-<commit>`, extract it,
   then extract the inner `presence-guard-0.1.0-windows-x64.zip`.

4. Move the `presence-guard` folder to `C:\ProgramData\obs-studio\plugins\` so you
   end up with:

   ```
   C:\ProgramData\obs-studio\plugins\presence-guard\bin\64bit\presence-guard.dll
   C:\ProgramData\obs-studio\plugins\presence-guard\data\locale\en-US.ini
   ```

5. Restart OBS → **Docks → Presence Guard**. Log check: **Help → Log Files →
   View Current Log** should contain `[presence-guard] loaded (version 0.1.0)`.

Full click-by-click (and the Visual Studio 2022 route, if you'd rather build
locally): **`docs/INSTALL.md`**.

---

## What's in the box

```
.
├─ src/
│  ├─ plugin-main.cpp        module entry, OBS frontend events, dock registration
│  ├─ presence-guard.cpp     the state machine — the actual product
│  ├─ presence-dock.cpp      Qt dock, settings UI, persistence
│  └─ platform-idle.cpp      Windows GetLastInputInfo / macOS CoreGraphics
├─ tests/                    state-machine harness + fake libobs (no OBS needed)
├─ docs/INSTALL.md           install + troubleshooting
├─ .github/workflows/        push / pull-request / dispatch pipelines (official
│                            obs-plugintemplate CI, untouched)
├─ .github/scripts/          the build + package scripts those workflows call
├─ build-aux/                clang-format / gersemi runners used by CI
├─ cmake/                    obs-plugintemplate build modules
├─ CMakeLists.txt            ENABLE_QT + ENABLE_FRONTEND_API wired up
├─ CMakePresets.json         windows-x64, macos, ubuntu-x86_64 (+ *-ci) presets
├─ buildspec.json            targets OBS 31.1.1 headers
├─ local-ai-auto-frame/      the second plugin — see below
├─ auto-director/            the third plugin — see below
├─ obs-beautifier/           the fourth plugin — see below
└─ hype-clip/                the fifth plugin — see below
```

Runtime cost: one 250 ms timer and two atomics. All scene switching happens on
the Qt main thread; the audio callback never touches OBS state.

## The second plugin: Local AI Auto Frame

`local-ai-auto-frame/` is a **separate plugin** that happens to live in this
repository. It is an OBS video filter that auto-frames a camera on detected
faces, using OpenCV 4.10 + the YuNet ONNX model — all local, no cloud call.

It is deliberately independent of Presence Guard:

| | Presence Guard (root) | Local AI Auto Frame (`local-ai-auto-frame/`) |
|---|---|---|
| Targets | OBS **31.1.1** headers | OBS **33.0.1** headers |
| Extra dependency | Qt6 | OpenCV 4.10 (fetched by CI) |
| Build it | **Actions → Push / Pull Request / Dispatch** | **Actions → Local AI Auto Frame** |
| Artifact | `presence-guard-<version>-windows-x64-<sha>` | `obs-local-ai-windows-x64` |

Both build on pushes to `main` and on pull requests that touch their own files;
each has its own `buildspec.json` and `CMakePresets.json`, so bumping one OBS
version never disturbs the other. Build, install and settings for the filter are
documented in [`local-ai-auto-frame/README.md`](local-ai-auto-frame/README.md).

## The third plugin: Auto Director

`auto-director/` is the advanced one: a dock that **runs the camera cuts while
you talk**. Each *take* is a scene plus the microphone of the person on that
camera; the director watches one volume meter per take and cuts to the loudest
speaker, with a hysteresis margin so two similar microphones stop ping-ponging,
a dwell time so a cough cannot steal the shot, an optional lobby scene after
long silence, and a hold that always hands control back to you when you change
the scene yourself.

| | Auto Director (`auto-director/`) |
|---|---|
| Targets | OBS **33.0.1** headers + Qt6 |
| Extra dependency | none |
| Build it | **Actions → Auto Director** |
| Artifact | `obs-auto-director-windows-x64` |
| Tests | `./tests/run-tests.sh` — the policy runs without OBS or Qt, 39 checks |

It is the only one of the three with a **dry run** mode: it logs every cut it
would have made, with the reason, without touching the live scene. Full details
in [`auto-director/README.md`](auto-director/README.md).

## The fourth plugin: OBS Beautifier

`obs-beautifier/` changes how OBS itself looks. It generates a Qt stylesheet and
appends it to the one your active theme installed - accent colour, corner
radius, density, font size and a few details - so the theme keeps working
underneath and **Revert** (or unloading the module) restores OBS exactly. Every
colour is derived from OBS's own palette and checked for WCAG contrast, so it
works on light and dark themes alike.

| | OBS Beautifier (`obs-beautifier/`) |
|---|---|
| Targets | OBS **33.0.1** headers + Qt6 |
| Extra dependency | none |
| Build it | **Actions → OBS Beautifier** |
| Artifact | `obs-beautifier-windows-x64` |
| Tests | `./tests/run-tests.sh` — the look is pure C++, 91 checks |

Details in [`obs-beautifier/README.md`](obs-beautifier/README.md).

## The fifth plugin: Hype Clip

`hype-clip/` clips your hype moments before you can reach for the hotkey. It
listens to every audio source, keeps a running baseline of how loud the stream
normally is, and asks OBS's replay buffer for a clip when something is loud, loud
*for this stream*, and stays that way — so a gunfight does not clip itself twenty
times, but the scream does. A big **Clip that** button and hotkey cover the rest,
and every clip lands in the dock with its score, a copy-path button and a
delete-file button.

| | Hype Clip (`hype-clip/`) |
|---|---|
| Targets | OBS **33.0.1** headers + Qt6 |
| Needs | the replay buffer on (Settings → Output) |
| Build it | **Actions → Hype Clip** |
| Artifact | `obs-hype-clip-windows-x64` |
| Tests | `./tests/run-tests.sh` — the detector is pure C++, 46 checks |

Details in [`hype-clip/README.md`](hype-clip/README.md).

## What I verified here

* `src/*.cpp` compiles clean with `-Wall -Wextra` (g++ 12, Qt 6.8.3).
* `./tests/run-tests.sh` links the **real** `presence-guard.cpp` against a fake
  libobs and drives the real 250 ms poll timer: **27/27 checks pass** — AFK
  park/restore, the only-while-live gate, dead air, a meter that produces no data
  (must *not* be read as silence), operator overrides, a deleted scene, and a
  settings save/load round trip.
* `clang-format` 19 and `gersemi` are clean on every file the CI format check
  looks at, and the workflow YAML parses.
* The pull-request run of this repository's own CI built the plugin on the
  Windows, macOS and Ubuntu runners and uploaded the artifacts.

What I could not do: run the resulting DLL inside a live OBS here. That is the
one step left to you — step 5 above.

## Likely first two snags

1. **Dock missing from the menu** → wrong folder depth. OBS only finds the DLL
   at `plugins\presence-guard\bin\64bit\`, not directly inside `plugins\`.
2. **Dead air fires instantly** → threshold too high for your gain. Talk, read
   the dB number in the status line, set the threshold a few dB below it.

## Obvious next moves

* **Global hotkey** to toggle the guard (next feature I'd add — needs
  `obs_hotkey_register_frontend` plus `obs_frontend_add_save_callback` to
  persist the binding).
* **Reason field in the status line** ("AFK 42 s of 120 s") — trivial now that
  the state machine is separated from the UI.
* **Linux idle support** behind an opt-in `libXss` flag.
* Fade-through-transition instead of a hard cut when parking the scene.
