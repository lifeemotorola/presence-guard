# Presence Guard

An OBS Studio plugin that keeps the stream looking alive while you are not:
it parks the output on a safe scene when you walk away or go quiet, and hands
your real scene back the moment you return.

Two rules, one dock:

| Rule | Watches | Fires when |
|---|---|---|
| **AFK** | system-wide keyboard/mouse idle time | nothing has been touched for *N* seconds |
| **Dead air** | the microphone, via a libobs volume meter | the level stays under the speech threshold for *N* seconds |

Both rules remember which scene was live, switch to the scene you picked, and
restore the previous one when the condition clears. If **you** change scenes
yourself in the meantime, the guard leaves your choice alone — it only undoes a
change it made itself.

## Why not just use Advanced Scene Switcher?

You can — it does far more. This is the small, single-purpose version: two
rules, a dock you can read at a glance, no macro engine to learn, and the whole
state machine covered by tests (`tests/run-tests.sh`).

## Requirements

* OBS Studio **31.x** (built against 31.1.1 headers); compatibility with OBS
  33.x has not been verified
* Windows 10/11 x64 or macOS 12+ for the AFK rule (Linux builds work, but idle
  time is not implemented there, so only the dead-air rule runs)
* The microphone source must be part of an active scene while the rule is armed
  — if it produces no audio at all, the guard deliberately suspends the dead-air
  rule instead of declaring silence it cannot measure

## Install

See **[docs/INSTALL.md](docs/INSTALL.md)** for the click-by-click version:
download a prebuilt Windows artifact with no compiler, or build it locally in
Visual Studio 2022.

Printable guides: **[Installation (PDF)](docs/presence-guard-install.pdf)** ·
**[Usage (PDF)](docs/presence-guard-usage.pdf)**. The Windows installation
guide links to a successful prebuilt artifact and explains the two ZIP files.

Short version on Windows: extract the plugin folder so you end up with

```
C:\ProgramData\obs-studio\plugins\presence-guard\bin\64bit\presence-guard.dll
C:\ProgramData\obs-studio\plugins\presence-guard\data\locale\en-US.ini
```

then restart OBS and enable the dock under **Docks → Presence Guard**.

## Using it

Open the dock with **Docks → Presence Guard**. This is the order that works —
the guard has to be switched on before anything else is editable.

### 1. Switch the guard on

Tick **Enable Presence Guard**. Every control below it is greyed out until you
do. There is no Save button: each change is applied and written to your profile
immediately, and it also restarts the silence countdown.

Keep **Only act while streaming or recording** ticked (the default) so the guard
stays idle when you are not live. Untick it to test the rules while offline.

### 2. Configure the AFK rule

In the **AFK rule** group:

* **Show scene** — the scene to park on; a BRB / "back in a minute" scene is the
  usual choice. Leaving it on *(choose a scene)* means the rule never fires.
* **After idle for** — how long the keyboard and mouse must be untouched, 10 s to
  3600 s (default 120 s).

Parking happens on the first poll after the timeout (the guard wakes every
250 ms), and any keyboard or mouse input brings your previous scene back on the
very next poll.

### 3. Configure the dead-air rule (optional)

Tick **Dead-air rule** to unlock the **Dead air** group:

* **Show scene** — leave it on *(same as AFK scene)* to reuse the scene above, or
  pick a dedicated "technical difficulties" scene. If you leave it alone you
  still need an AFK scene set, because that is what it falls back to.
* **Microphone** — any audio-producing source in the scene collection (a mic, a
  capture card, an audio-only source). It *does* have to be in the scene that is
  live, otherwise it produces no level to measure.
* **After silence for** — 5 s to 1800 s (default 45 s).
* **Speech threshold** — −90 dB to 0 dB (default −45 dB). Anything at or above
  this counts as speech.

To tune the threshold: make sure the status line shows a moving dB figure, talk
normally, read the number, then set the threshold a few dB below it. The rule
restores the moment your level crosses back above the threshold.

### 4. Test it

* **Refresh** — re-reads the scene and microphone lists. Do this after adding a
  scene or an audio source; switching scene collections refreshes
  automatically.
* **Test AFK** — parks on the AFK scene *right now* and **stays there** until you
  press **Restore now**. It deliberately does not time out, because it is
  pretending you are away.
* **Restore now** — hands your previous scene straight back.

Both buttons are greyed out while the guard is off, and **Test AFK** silently
does nothing if no AFK scene is selected. If **Only act while streaming or
recording** is ticked and you are not live, the guard is not armed, so a test
park is undone again on the next poll — untick it to test offline.

### 5. Read the status line

The line under the buttons refreshes twice a second, and turns orange while the
guard has taken over:

| Status | Meaning |
|---|---|
| `Watching — idle 84 s · mic −52.3 dB (threshold −45 dB).` | armed, both rules live |
| `Watching — idle 84 s · dead-air rule off.` | only the AFK rule is armed |
| `Watching — no microphone selected, AFK rule only.` | no usable mic attached, so the dead-air rule is suspended |
| `AFK — showing "BRB".` / `Dead air — showing "Technical difficulties".` | parked |
| `Guard is off.` | **Enable Presence Guard** is unticked |

On Linux the dock also warns that idle detection is not implemented in this
build, so only the dead-air rule can fire there.

### What the guard deliberately will not do

* **It never fights you.** It only ever undoes a switch it made itself: if you
  changed scene manually while it was parked, it leaves your choice alone.
* **Studio Mode is ignored.** With a preview/program split you are clearly at the
  controls, so both rules stay idle.
* **It will not invent silence.** If the microphone is muted, is not in the live
  scene, or disappears, the meter stops producing values and the dead-air rule is
  suspended instead of firing on data it cannot measure.
* **It will not thrash on a missing scene.** A renamed or deleted scene that a
  rule points at puts the guard into a five-second back-off and logs a warning,
  rather than retrying four times a second.
* **It restores when you go offline.** Stopping the stream or recording hands the
  previous scene back, so the next session does not start parked.

### Where the settings live

Each OBS profile stores them in `basic.ini`, section `[PresenceGuard]`, and they
are written as you edit. Settings follow you when you switch profiles: whatever
is in the dock is copied into the profile you switch to. Delete that section to
reset the guard to its defaults (off, both rules pointing at no scene, 120 s AFK,
45 s dead air, −45 dB).

## How it works

```
 plugin-main.cpp      module entry, frontend events, dock registration
   └── presence-guard.cpp    the state machine (Qt timer, 250 ms poll)
         ├── platform-idle.cpp   idle time: GetLastInputInfo / CoreGraphics
         └── obs_volmeter        mic level, callback on the audio thread
   └── presence-dock.cpp     Qt dock widget, settings persistence
```

* All rules run on the Qt main thread, so every `obs_frontend_set_current_scene`
  call happens where OBS expects it.
* The audio callback only writes two atomics (`db_`, `lastCallbackMs_`) and never
  touches OBS state.
* The microphone is (re)attached lazily: scene collections are restored *after*
  modules load, so the first attach usually fails and the poll retries.
* Studio mode is left alone — with a preview/program split the operator is
  clearly at the controls.
* A missing or renamed scene puts the guard into a five-second back-off instead
  of retrying (and logging) four times a second.

## Tests

```sh
./tests/run-tests.sh
```

The harness links the real `src/presence-guard.cpp` against a fake libobs
(`tests/stubs`) and drives the actual 250 ms poll timer through 27 checks: AFK
park/restore, the only-while-live gate, dead air, a meter that produces no data,
operator overrides, a missing scene, and a config save/load round trip. It needs
a C++17 compiler and Qt6 Core headers; it does not need OBS.

Qt is found through `pkg-config` (`qt6-base-dev` on Debian/Ubuntu,
`brew install qt6` on macOS), or point the script at an installation yourself:

```sh
QTDIR=$HOME/Qt/6.8.3/gcc_64 ./tests/run-tests.sh
```

## License

GPL-2.0-or-later, like OBS Studio itself. See [LICENSE](LICENSE).
