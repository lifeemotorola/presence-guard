# TypeBloom

A small, offline, floating typing-speed meter for Windows. It estimates how fast
you are typing **in other desktop apps**, without turning your work into a typing
test. TypeBloom is an ordinary desktop window—not a Windows Widgets panel item.

## Get started

1. Download the `typebloom-windows-x64` ZIP from **Actions → TypeBloom → a
   successful run → Artifacts**, and extract it to a local folder. A public
   rolling download may also be listed in the Creator Toolkit after the build is
   merged and published.
2. Run **TypeBloom.exe**. It is a small portable native executable; no installer,
   account, separate runtime, or internet connection is required on Windows 11
   x64 (Windows 10 x64 should also work). No administrator rights are required.
3. Drag the top of the widget where you want it. The widget floats above normal
   windows by default; **Pinned on top** toggles this. Find it again from the
   taskbar if you unpin or minimize it. Position and pin preference are remembered.
4. It **starts paused every time**. Click **Start tracking**, read the privacy
   prompt, and confirm. Then focus another app and type. You can **Pause**,
   **Resume**, **Reset** or close it at any time. Pause and Reset clear the
   current reading. Minimizing also pauses tracking. The X exits completely;
   there is no tray process or automatic startup. A second launch brings the
   existing widget forward.
5. Use the **Privacy** button for the same data and limitations disclosure.

The executable is **unsigned**. Windows SmartScreen can show an unknown
publisher or reputation warning. Check that you trust the download and compare
`TypeBloom.exe` with the SHA-256 in `SHA256SUMS.txt` and the matching Actions
run summary. A matching hash identifies the build; it does not prove the
publisher's identity, safety, or a digital signature. Do not disable Windows
security globally.

## Privacy and safety

While tracking is on, a Windows low-level keyboard hook observes keystroke events
in the current desktop to classify typing-like keys. **TypeBloom does not decode,
retain, log, or transmit the key codes or typed text**, read the active app's
contents, window title, or clipboard, or make network requests. It holds only
recent event timestamps and aggregate counts in memory, for up to 15 seconds;
they are discarded on pause, reset, idle expiry and exit. The hook does not
block or change input, and is removed when paused or closed. If Windows reports
a session lock, remote disconnect or sleep, TypeBloom pauses and does not
resume automatically; these notifications are best-effort, not a security
boundary.

**Password fields are not detected.** Their key presses can contribute to the
visible number while live, even though no characters are retained. Pause before
entering passwords, payment information, or anything else sensitive. The speed
is visible to anyone who can see the screen. Use this on your own computer or
only with the permission of the person typing. Security software may flag apps
that use a global keyboard hook; review the source and follow your organization's
rules rather than bypassing security.

The only persistent data written by TypeBloom are window X/Y position and the
always-on-top setting at `HKEY_CURRENT_USER\Software\TypeBloom`. No typing
history is saved. Close the app and delete that registry key to remove the
preferences. Windows itself may retain its normal application history; the app
does not control that.

## What the number means

- WPM is an **estimate**, not actual words, net speed or accuracy: five counted
  typing-like key-downs make an estimated word. Space, letters, numbers,
  punctuation and numpad typing keys count; Backspace, Enter, navigation keys
  and most Ctrl/Alt/Windows shortcuts do not. Held-key repeats and synthetic
  input may count. Right Alt + Ctrl (AltGr) is allowed for some layouts, so
  certain shortcuts with that combination may also count. The app does not
  examine characters or know whether anything was inserted or corrected.
- The count and graph show the trailing 15 seconds. WPM is
  `count / 5 / minutes`, using the elapsed window since the first key after idle,
  capped at 15 seconds and floored at 5 seconds during warm-up. This prevents
  enormous initial spikes. The reading falls to zero when the window empties;
  the fixed memory buffer caps exceptionally large synthetic bursts at 4,096
  events. No accuracy percentage is claimed.
- Non-US layouts, dead keys, IMEs, voice dictation, accessibility tools and
  on-screen keyboards can behave differently from ordinary key presses.
  Secure desktops, UAC prompts, some elevated programs, remote sessions, games
  and full-screen/exclusive apps may not deliver input or display the widget
  above their content. Windows can also remove a stalled hook. If it stops
  updating, pause and resume, or restart. There is no guarantee of counting
  every character or excluding every shortcut. Do not use it as a productivity
  or surveillance record.

## Build and test

On Windows with Visual Studio's C++ workload and the Windows SDK:

```powershell
cmake -S typebloom -B typebloom/out/build -A x64
cmake --build typebloom/out/build --config Release
ctest --test-dir typebloom/out/build -C Release --output-on-failure
./typebloom/tests/windows-smoke.ps1 -App typebloom/out/build/Release/TypeBloom.exe -Target typebloom/out/build/Release/typebloom_test_target.exe
```

The Windows smoke test opens a separate disposable input window, injects dummy
keys, exercises the actual hook and controls, and restores existing TypeBloom
preferences. Close any running TypeBloom instance before starting it. It also
checks Windows accessible names; automated checks are not a substitute for
hands-on screen-reader or Windows 11 testing. The portable C++ meter tests also
run on Linux and cover shortcuts, AltGr, window boundaries, idle, reset,
backwards clocks, and bounded bursts.

Source is GPL-2.0-or-later; see `TYPEBLOOM-LICENSE.txt` or the repository
`LICENSE`. Copyright © 2026 Emmanuel Suah. The executable's author metadata is
not a code signature.
