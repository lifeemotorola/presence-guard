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

* OBS Studio **31.x** (built and tested against 31.1.1 headers)
* Windows 10/11 x64 or macOS 12+ for the AFK rule (Linux builds work, but idle
  time is not implemented there, so only the dead-air rule runs)
* The microphone source must be part of an active scene while the rule is armed
  — if it produces no audio at all, the guard deliberately suspends the dead-air
  rule instead of declaring silence it cannot measure

## Install

See **[docs/INSTALL.md](docs/INSTALL.md)** for the click-by-click version
(build it with GitHub Actions, with no compiler on your machine, or build it
locally in Visual Studio 2022).

Short version on Windows: extract the plugin folder so you end up with

```
C:\ProgramData\obs-studio\plugins\presence-guard\bin\64bit\presence-guard.dll
C:\ProgramData\obs-studio\plugins\presence-guard\data\locale\en-US.ini
```

then restart OBS and enable the dock under **Docks → Presence Guard**.

## Using it

1. **Docks → Presence Guard**.
2. Pick a scene for the AFK rule (a BRB / "back in a minute" scene works well).
3. Optionally enable the dead-air rule: pick a scene, pick the microphone and
   set the speech threshold. `-45 dB` is a reasonable starting point; talk
   normally and watch the live dB readout in the status line to tune it.
4. Tick **Enable Presence Guard**, and keep **Only act while streaming or
   recording** on unless you are testing.
5. **Test AFK** forces the AFK scene; **Restore now** cancels it.

Settings live in the current OBS profile (`basic.ini`, section
`[PresenceGuard]`), so each profile can have its own guard.

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
