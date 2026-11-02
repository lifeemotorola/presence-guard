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
└─ buildspec.json            targets OBS 31.1.1 headers
```

Runtime cost: one 250 ms timer and two atomics. All scene switching happens on
the Qt main thread; the audio callback never touches OBS state.

## What I verified here

* `src/*.cpp` compiles clean with `-Wall -Wextra`.
* `./tests/run-tests.sh` links the **real** `presence-guard.cpp` against a fake
  libobs and drives the real poll timer: every check passes — AFK park/restore,
  the only-while-live gate, dead air, a meter that produces no data (must *not*
  be read as silence), operator overrides, a deleted scene, and a settings
  save/load round trip.
* `clang-format` and `gersemi` are clean on every file the CI format check looks
  at, and the workflow YAML parses.

What I could not do: compile the Windows/MSVC binary and click through a live
OBS here. That is exactly what the GitHub Actions run above does.

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
