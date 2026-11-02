# Presence Guard — start here

You said *build it*, so I built the plugin instead of writing you a macro list.
**`presence-guard/`** is a complete OBS Studio plugin project (C++17, Qt6,
libobs, official `obs-plugintemplate` layout) that does the three things I
promised earlier, natively:

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

1. Create an empty repo: <https://github.com/new> (name it `presence-guard`, no
   README, no .gitignore).
2. In a terminal, inside `presence-guard/`:

   ```bash
   git branch -M main
   git remote add origin https://github.com/YOUR-USERNAME/presence-guard.git
   git push -u origin main
   ```

   (It's already a git repo with two commits — you're just adding the remote.)
3. **Actions** tab → the *"main push run 🚀"* run → wait for **Build for
   Windows 🪟** (green ✔, ~6–10 min the first time).
4. Download the artifact `presence-guard-0.1.0-windows-x64-<commit>`, extract it,
   then extract the inner `presence-guard-0.1.0-windows-x64.zip`.
5. Move the `presence-guard` folder to `C:\ProgramData\obs-studio\plugins\` so
   you end up with:

   ```
   C:\ProgramData\obs-studio\plugins\presence-guard\bin\64bit\presence-guard.dll
   C:\ProgramData\obs-studio\plugins\presence-guard\data\locale\en-US.ini
   ```

6. Restart OBS → **Docks → Presence Guard**. Log check: **Help → Log Files →
   View Current Log** should contain `[presence-guard] loaded (version 0.1.0)`.

Full click-by-click (and the Visual Studio 2022 route, if you'd rather build
locally): **`presence-guard/docs/INSTALL.md`**.

---

## What's in the box

```
presence-guard/
├─ src/
│  ├─ plugin-main.cpp        module entry, OBS frontend events, dock registration
│  ├─ presence-guard.cpp     the state machine — the actual product
│  ├─ presence-dock.cpp      Qt dock, settings UI, persistence
│  └─ platform-idle.cpp      Windows GetLastInputInfo / macOS CoreGraphics
├─ tests/                    state-machine harness + fake libobs (no OBS needed)
├─ docs/INSTALL.md           install + troubleshooting
├─ CMakeLists.txt            ENABLE_QT + ENABLE_FRONTEND_API wired up
├─ buildspec.json            targets OBS 31.1.1 headers
└─ .github/workflows/        the official build pipeline, untouched
```

Runtime cost: one 250 ms timer and two atomics. All scene switching happens on
the Qt main thread; the audio callback never touches OBS state.

## What I verified here

* `src/*.cpp` compiles clean with `-Wall -Wextra` (Qt 6.8 + minimal libobs
  headers).
* `./tests/run-tests.sh` links the **real** `presence-guard.cpp` against a fake
  libobs and drives the real poll timer: 22 checks pass — AFK park/restore, the
  only-while-live gate, dead air, a meter that produces no data (must *not* be
  read as silence), operator overrides, a deleted scene, and a settings
  save/load round trip.
* Behaviour matches OBS internals I read in the 31.1.1 source: plugin search
  path `plugins/<name>/bin/64bit`, module-load timing vs. scene-collection
  restore, and volume-meter threading.

What I could not do: compile the Windows/MVS binary and click through a live
OBS. That's the one step left for you, and step 3 above is where it happens.

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
