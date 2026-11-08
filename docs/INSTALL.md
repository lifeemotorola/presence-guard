# Installing Presence Guard on Windows

A printable **[installation guide (PDF)](presence-guard-install.pdf)** and
**[usage guide (PDF)](presence-guard-usage.pdf)** are also available. The PDFs
are prebuilt; maintainers can regenerate them with
`python -m pip install reportlab==4.4.10` followed by
`python docs/generate-guides.py` from the repository root.

**Route A needs no compiler or build step**: download a prebuilt Windows
artifact. Routes B and C are for developers who want to build the plugin.

> **OBS 33.x:** this binary is built against OBS 31.1.1 headers. Compatibility
> with OBS 33 has not been verified; check the OBS log if the plugin does not
> load and obtain a compatible build before relying on it.

---

## Route A — download the prebuilt Windows plugin (recommended)

### 1. Open a successful build

Open [this successful main-branch Actions run](https://github.com/lifeemotorola/presence-guard/actions/runs/42463108613).
You may need to sign in to GitHub to download artifacts. If the artifact has
expired, open [Actions](https://github.com/lifeemotorola/presence-guard/actions),
choose a newer successful **main push run**, and confirm its **Build for Windows**
job succeeded.

### 2. Download the artifact

> **The `.dll` is not in the repository.** It is a build output under
> **Actions → run → Artifacts**, at the bottom of the run summary, not in the
> **Code** tab.

In that successful run, scroll down to **Artifacts** and download:

```text
presence-guard-0.1.0-windows-x64-6956a758a
```

A newer successful run will have a different commit suffix. The downloaded
Actions ZIP contains a second ZIP named `presence-guard-0.1.0-windows-x64.zip`.
Extract **both** to obtain the `presence-guard` folder. Do not use the separate
ScreenForge artifact; it is a screen recorder, not this OBS plugin.

### 3. Install it

1. Quit OBS completely, including any copy left in the system tray.
2. In File Explorer, paste `%ProgramData%\obs-studio\plugins` into the address
   bar. Create the `plugins` folder if needed. Windows may ask for administrator
   permission.
3. Move the **entire** extracted `presence-guard` folder there, keeping its
   `bin` and `data` folders together. A typical installation looks like:

   ```text
   C:\ProgramData\obs-studio\plugins\presence-guard\bin\64bit\presence-guard.dll
   C:\ProgramData\obs-studio\plugins\presence-guard\data\locale\en-US.ini
   ```

### 4. Open and verify the dock

Start OBS normally (not in Safe Mode) and open **Docks → Presence Guard**. The
dock may start hidden or floating. Configure it using the
[usage guide (PDF)](presence-guard-usage.pdf) or [README](../README.md#using-it).

To confirm the plugin loaded: **Help → Log Files → View Current Log**, then
search for:

```text
[presence-guard] loaded (version 0.1.0)
```

---

## Route B — build locally with Visual Studio 2022

Requirements:

* **Visual Studio 2022** with the *Desktop development with C++* workload
* **Windows 11 SDK 10.0.22621** (the preset pins this exact version)
* **CMake 3.28+** (bundled with VS is fine: *"C++ CMake tools for Windows"*)
* **Git**
* ~2 GB of free disk for the OBS headers, prebuilt dependencies and Qt

From a *x64 Native Tools Command Prompt for VS 2022* (or ordinary PowerShell), in
the project folder:

```powershell
# 1. configure - this downloads the OBS 31.1.1 headers, obs-deps and Qt6
cmake --preset windows-x64

# 2. compile
cmake --build --preset windows-x64 --config RelWithDebInfo

# 3. install straight into the shared plugin folder
cmake --install build_x64 --config RelWithDebInfo
```

Step 3 writes to `C:\ProgramData\obs-studio\plugins\presence-guard\` by default.
Then restart OBS and enable the dock.

**If configure fails with an SDK error**, open `CMakePresets.json` and change the
`windows-x64` preset's architecture to the SDK you actually have installed, e.g.

```json
"architecture": "x64,version=10.0.26100"
```

(check `C:\Program Files (x86)\Windows Kits\10\Include\` for the version numbers
you have). For a one-off, `cmake --preset windows-x64 -A x64` also works with the
default SDK.

---

## Route C — build and test it on Linux

The Linux build works too (the AFK rule is a no-op there, because idle time is
only implemented for Windows and macOS, so only the dead-air rule runs).

```bash
sudo add-apt-repository --yes ppa:obsproject/obs-studio
sudo apt update
sudo apt install build-essential cmake ninja-build qt6-base-dev obs-studio

cmake --preset ubuntu-x86_64
cmake --build --preset ubuntu-x86_64
sudo cmake --install build_x86_64
```

The plugin lands in `/usr/lib/<arch>-linux-gnu/obs-plugins/` with its data in
`/usr/share/obs/obs-plugins/presence-guard/`.

You do not need OBS, or a full plugin build, to run the state machine tests:

```bash
./tests/run-tests.sh            # uses pkg-config, or QTDIR=/path/to/qt6
```

---

## Troubleshooting

**The dock isn't in the Docks menu.** The plugin didn't load. Check the current
log file for `presence-guard` — common causes are a wrong folder layout (the
`bin\64bit` level is required), the DLL being blocked (right-click →
*Properties* → *Unblock*), or OBS having started in **Safe Mode**, which skips
third-party plugins.

**A rule never fires.** In order:

* *Enable Presence Guard* is ticked,
* *Only act while streaming or recording* is unticked, or you are actually live,
* the chosen scene still exists (rename it and the guard backs off for five
  seconds at a time and logs a warning),
* for dead air: the microphone is in the scene that is live, is not muted, and
  the status line shows a moving dB value — if it shows "no microphone selected"
  the meter never attached,
* Studio Mode is off (the guard deliberately does nothing in Studio Mode).

**Dead air fires instantly.** Your speech threshold is too high for the mic's
gain. Talk normally, read the dB value in the status line, and set the threshold
a few dB below it.

**It switched to the AFK scene but never came back.** Any keyboard or mouse
movement brings it back; *Restore now* forces it. If the previous scene was
deleted while you were away, there is nothing to restore to and the guard leaves
whatever is live on air.

**After a major OBS update** plugin binaries can become incompatible. The
current build uses OBS 31.1.1 headers and has not been verified against OBS 33.
Check the OBS log and obtain a compatible build; maintainers may need to update
the `obs-studio` version and hashes in `buildspec.json` before rebuilding.

## Uninstalling

Delete `C:\ProgramData\obs-studio\plugins\presence-guard`. The settings stay in
your profile's `basic.ini`; delete the `[PresenceGuard]` section there to remove
them too.
