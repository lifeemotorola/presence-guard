# Installing Presence Guard on Windows

Two routes. **Route A needs no compiler** — GitHub builds the `.dll` for you on
a Windows runner. Use it unless you specifically want a local toolchain.

---

## Route A — build it in the cloud (recommended, ~10 minutes)

You need a GitHub account. Nothing else — this repository already contains the
workflows (`.github/workflows/`) and the build spec (`buildspec.json`) the
runners need.

### 1. Push the repository

In Git Bash (or PowerShell, with Git installed), from the repository root:

```bash
git push origin main
```

If Git asks for a password, use a Personal Access Token
(<https://github.com/settings/tokens> → *Fine-grained tokens* → repository
access: *Contents: Read and write*), or use GitHub Desktop instead.

To build from a branch without merging, push the branch and open a pull request
against `main`; the pull-request run builds Windows, macOS and Ubuntu as well.

### 3. Wait for the build

Open the **Actions** tab. A run called *"main push run 🚀"* starts
automatically. The **Build for Windows 🪟** job takes roughly 6–10 minutes (it
downloads the OBS headers, prebuilt deps and Qt the first time).

* Green ✔ on `build-project` → you're done.
* If a job called *Check Formatting 🔍* fails, ignore it; the Windows build is a
  separate job and still produces artifacts.

### 4. Download the artifact

> **The `.dll` is not in the repository.** It is a build output, so it exists
> only as a workflow artifact on a finished Actions run — never in the **Code**
> tab and never in a clone. Artifacts sit at the **bottom of the run summary
> page**, under the *Artifacts* heading.

On the finished run page, scroll to **Artifacts** and download

```
presence-guard-0.1.0-windows-x64-<commit>
```

It arrives as a `.zip`; inside it is `presence-guard-0.1.0-windows-x64.zip` —
that is the second zip you have to extract before you see the folder.

### 5. Install it

1. Extract the inner `presence-guard-0.1.0-windows-x64.zip`.
2. It contains a single folder, `presence-guard`. Move **that folder** into:

   ```
   C:\ProgramData\obs-studio\plugins\
   ```

   (paste that path into the Explorer address bar; the folder may not exist yet
   — create it if not. No administrator rights are needed.)

3. Verify the result looks exactly like this:

   ```
   C:\ProgramData\obs-studio\plugins\presence-guard\bin\64bit\presence-guard.dll
   C:\ProgramData\obs-studio\plugins\presence-guard\data\locale\en-US.ini
   ```

4. Restart OBS completely (not just minimise to tray).

### 6. Turn the dock on

OBS starts the dock hidden and floating. Open **Docks → Presence Guard**. Then
configure it as described in the [README](../README.md#using-it).

To confirm the plugin loaded at all: **Help → Log Files → View Current Log** and
search for:

```
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

**After a major OBS update (e.g. 31 → 32)** plugin binaries can become
incompatible. Rebuild (Route A: push any commit; Route B: rerun the three
commands) — and if OBS bumped the API, update the `obs-studio` version and hashes
in `buildspec.json` first.

## Uninstalling

Delete `C:\ProgramData\obs-studio\plugins\presence-guard`. The settings stay in
your profile's `basic.ini`; delete the `[PresenceGuard]` section there to remove
them too.
