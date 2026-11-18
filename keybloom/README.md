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

If you are updating from version **1.0 or 1.1**, close KeyBloom first, extract
1.2 into a separate folder, and start its `KeyBloom.exe`. Your progress is in
your Windows user profile; do not delete `%APPDATA%\KeyBloom` if you want to
keep it. The original 21 lessons and their IDs are unchanged. Version 1.2
imports lesson completions still present in your old recent-session history;
completions already dropped from that history before upgrading cannot be
recovered. Version 1.0 did not collect per-letter counts, so older sessions
cannot contribute to letter insights.

After upgrading, **continue using 1.2 or newer**. If you reopen an older version
and it saves the shared profile, it can discard the new separate completion
list (and 1.0 can also discard sessions from modes introduced in 1.1). There is
no automatic update or cloud backup.

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

- **500 distinct guided practices**, including the unchanged original 21. Begin
  with the home, top and bottom rows, then explore letter warm-ups, word flows,
  capitals, numbers, punctuation, everyday sentences, practical messages and
  longer paragraphs. Each is an untimed exercise you can start or repeat.
- In **Lessons**, search by title, topic, key or lesson number; filter by topic
  or practiced/not practiced; and move between pages or jump to a page. Twelve
  cards appear at a time. Long passages scroll with the current character as
  you type. Your practiced-lesson count is saved separately from the rolling
  history, so repeats will not erase earlier course completions.
- **Typing tests** with 30-second, one-, three- and five-minute choices and
  original passages.
- **Word Sprint**, a 45-second word game scored by correctly completed words.
- **Focus on a key** lets you choose any letter for a short, untimed drill.
  Recent error counts can suggest letters to practice; select an insight in
  My progress to start a matching drill.
- **Practice your own text** turns a passage you type or paste into an untimed
  exercise. Passages are kept only in memory and are not saved. Use 10–500
  printable US keyboard characters; extra spaces and line breaks become single
  spaces. Emoji and accented characters are not supported in custom text yet.
- A US QWERTY **on-screen keyboard**, next-key and finger hints, plus live speed,
  accuracy and timer. Pause as needed; the app pauses if its window loses focus.
- **Progress** with recent sessions, speed trend, accuracy, a daily practice
  goal, day streak, tricky-letter insights and six milestones. Insights and
  non-lesson milestones use the most recent 500 retained sessions, not lifetime
  totals, so they can change as older sessions fall off or after reset. The
  lesson milestone uses the separately saved course completions.
- **Settings** to show/hide keyboard hints, choose a larger practice text size,
  change a daily goal or reset local data.
- No account, ads or intentional network calls. The app blocks HTTP/WebSocket
  requests in its desktop session. It stores practiced lesson IDs, session
  summaries and settings locally, plus aggregate expected-letter attempt/error
  counts from built-in exercises. It never stores what you typed or any letter
  counts from custom passages.

This is not feature parity with Mavis Beacon: there are no
separate learner profiles, dictation, cloud sync, adaptive curriculum, 3-D
racing games or languages other than the included English/US QWERTY content.
Hints may not match a different physical keyboard layout. There is no automatic
update service or cloud backup.

## How results work

- The timer starts on the first typed character. A completed lesson stops when
  the practice text has been entered, even if it contains uncorrected mistakes.
  Focus drills and custom exercises also stop when their text has been entered.
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
- **Letter insights** count each attempt at an expected A–Z letter in completed
  built-in exercises, including corrected errors. Letters are ranked by error
  rate; small samples may not reflect your overall skill. Custom passages are
  excluded, as are records created by version 1.0.
- Up to 500 recent session summaries are kept in local app storage under
  `%APPDATA%\KeyBloom`. The IDs of completed course lessons are stored
  separately, so all 500 can remain practiced even if old session details roll
  off. Session totals, speed, accuracy, time, streaks, insights and non-lesson
  milestones reflect the retained history. Deleting that profile or using
  **Settings → Reset progress** permanently removes both course completions and
  history; there is no recovery or sync.

## Development and verification

```bash
cd keybloom
npm ci
npm test
```

The Node tests cover all 500 distinct exercises, their stable IDs, scoring,
corrections, insights, custom-text privacy, milestones, timed sessions, calendar
streaks, and completion retention after more than 500 repeated sessions. The
Windows Actions job builds the actual portable app and uses Playwright to
exercise paging, search, filters, the final paragraph, old-profile migration,
reset, Word Sprint, focused drills, custom text, test lengths, and text-size
settings. It checks the sandbox, a 900 × 640 layout and all 500 completion IDs
after restarting from a different folder. Screenshots are uploaded separately.
Automated tests do not replace human usability testing. The prebuilt download
is available in the Actions artifact; users do not need these developer steps.

## Licence

KeyBloom's source is GPL-2.0-or-later; see `KEYBLOOM-LICENSE.txt` in the
artifact or the repository `LICENSE`. The artifact also contains Electron's
`LICENSE` and Chromium's `LICENSES.chromium.html` notices. Copyright © 2026
Emmanuel Suah. The executable's author metadata is not a code signature.
