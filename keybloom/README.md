# KeyBloom

**A little better, one key at a time.** KeyBloom is an original, offline Windows
typing tutor by Emmanuel Suah. It is inspired by the idea of a friendly guided
typing course, but does not use Mavis Beacon's name, content, artwork or code
and is not affiliated with it.

## Get the Windows app

1. Open **Actions → KeyBloom → the latest successful run → Artifacts** and
   download `keybloom-windows-x64`.
2. Right-click the ZIP and **Extract All** into a local folder such as
   `Documents\KeyBloom`.
3. Open that folder and double-click **KeyBloom.exe**. Keep all the other files
   and folders beside it. This is a portable folder, **not a single-file EXE**;
   move the complete folder if you want to move the app.

Requires **64-bit Windows 10 or 11** and a physical keyboard. No Node.js,
compiler, account or internet connection is required after downloading it. The
Windows x64 build is not a native ARM or 32-bit build. `INSTALL.txt` inside the
artifact has these same instructions.

**The executable is unsigned.** Windows may show a publisher or reputation
warning. Check that you trust the download and compare the SHA-256 in the run
summary to `SHA256SUMS.txt`. A matching hash identifies the file from that run;
it does not establish safety or create a digital signature. Do not disable
Windows security globally.

## What is included

- **21 original lessons** covering home, top and bottom rows, capital letters,
  numbers, punctuation and everyday sentences. Start or repeat any lesson.
- **Typing tests** with one- or three-minute choices and original passages.
- **Word Sprint**, a 45-second word game scored by correctly completed words.
- A US QWERTY **on-screen keyboard**, next-key and finger hints, plus live speed,
  accuracy and timer. Pause as needed; the app pauses if its window loses focus.
- **Progress** with recent sessions, speed trend, accuracy, a daily practice
  goal and a day streak. Settings can show/hide keyboard hints and reset data.
- No account, ads or intentional network calls. The app blocks HTTP/WebSocket
  requests in its desktop session. It stores only session summaries and
  settings in the current Windows profile, not what you type.

This is a first release, not feature parity with Mavis Beacon: there are no
separate learner profiles, dictation, cloud sync, adaptive curriculum, 3-D
racing games or languages other than the included English/US QWERTY content.
Hints may not match a different physical keyboard layout. There is no automatic
update service or cloud backup.

## How results work

- The timer starts on the first typed character. A completed lesson stops when
  the practice text has been entered, even if it contains uncorrected mistakes.
  A timed activity stops at its time limit; passages repeat if needed.
- **WPM** is the number of correctly placed characters divided by five, then
  divided by active minutes. This is a simple net speed measure, not a certified
  typing-test score. Speed is shown after one active second.
- **Accuracy** is correct inserted character attempts divided by all inserted
  character attempts. Corrections take time, and a mistake still counts after it
  is corrected. Backspace itself is not a character attempt. Pasting is disabled
  in the practice box so results reflect typing.
- A lesson counts as practiced after it is finished, without an accuracy
  threshold. Daily goals count completed sessions, and the streak counts
  consecutive local calendar days with at least one completed session.
- Up to 500 recent session summaries are kept in local app storage under
  `%APPDATA%\KeyBloom`. Deleting that profile or using **Settings → Reset
  progress** permanently deletes local history; there is no recovery or sync.

## Development and verification

```bash
cd keybloom
npm ci
npm test
```

The Node tests cover the course, scoring, corrections, timed sessions, pause,
calendar streaks, data limits and persistence sanitization. The Windows Actions
job builds the actual portable app, launches it with Playwright, exercises a
lesson, a typing test and Word Sprint, checks saving across a restart, and tests
security settings and a small window. A screenshot is uploaded separately. The
prebuilt download is available in the Actions artifact; users do not need these
developer steps.

## Licence

KeyBloom's source is GPL-2.0-or-later; see `KEYBLOOM-LICENSE.txt` in the
artifact or the repository `LICENSE`. The artifact also contains Electron's
`LICENSE` and Chromium's `LICENSES.chromium.html` notices. Copyright © 2026
Emmanuel Suah. The executable's author metadata is not a code signature.
