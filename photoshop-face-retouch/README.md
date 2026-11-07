# Face Retouch — a portrait panel for Photoshop

Portrait retouching that leaves the photograph intact. One press builds a stack of
adjustment layers above your picture — skin evened, shadows lifted, warmth put back,
texture restored — and then leaves every one of them adjustable, maskable, and
removable. Nothing is flattened, nothing is painted over, and the background layer
is never touched.

![The panel and the layer stack it builds](docs/ui.png)

A UXP plugin: HTML, CSS and JavaScript. No C++ SDK, no build step, no compiler. It
loads in Photoshop through the free UXP Developer Tool and appears under
**Plugins → Face Retouch**.

---

## What you need

| | |
|---|---|
| Photoshop | **2023 (24.0) or newer** — UXP panels are built in from 22.0 |
| UXP Developer Tool | free, from the Creative Cloud desktop app |
| Anything else | nothing. No Node, no Python, no compiler |

The panel is tested against Photoshop 2023's scripting API and uses no feature
newer than manifest version 5, which 2023 supports.

---

## Install

1. Open the **UXP Developer Tool**.
2. **Add Plugin** → choose `photoshop-face-retouch/manifest.json`.
3. Press **Load**. The panel appears in Photoshop under
   **Plugins → Face Retouch**.
4. To keep it after restarting Photoshop, use **… → Load on start** in the
   Developer Tool.

If Photoshop is older than 24.0 the Developer Tool refuses the plugin and says so;
that is the version gate in `manifest.json`, not a broken file.

---

## The one button

Open a photo, pick a look, press **Retouch the face**. You get six layers:

| Layer | Blend mode | What it is |
|---|---|---|
| `FR · Detail` | Overlay | A high pass of the photo — pores, eyelashes, hair, the structure the smoothing would otherwise eat |
| `FR · Warmth` | Soft Light | A solid colour fill in a skin tint, at a few percent |
| `FR · Depth` | Soft Light | The photo blended with itself: contrast without clipping |
| `FR · Glow` | Screen | A heavily blurred copy — soft light on the high points |
| `FR · Lift` | Lighten | A blurred copy, pulled back to a low opacity — opens the shadow under the eyes and beside the nose |
| `FR · Smooth` | Color | A blurred copy in **Colour**, the layer this whole thing is built around |

They are collected into one group called **Face Retouch**. Toggle the group off to
compare; toggle it back on. **Compare** in the panel does the same thing; **Remove**
deletes everything the plugin made — or just press Ctrl+Z, because all of it is
normal Photoshop history.

### Why the smoothing is in *Colour*

A blurred copy in Colour blend mode averages the **hue and saturation** of the skin
and leaves the luminosity — pores, stubble, freckles, the actual texture — exactly
where it was. That is the difference between "retouched" and "plastic". The
`FR · Detail` layer on top then puts back the high-frequency structure, and the
**Texture** slider decides how much of it survives.

---

## The looks

| Preset | For | Smoothing | Character |
|---|---|---|---|
| **Natural** | when nobody should notice | 5.5 px | keeps pores and freckles |
| **Soft** | magazines, social | 8 px | smooth, texture in the shadows |
| **Glamour** | events, head shots | 12 px | maximum smoothing, warm, glowing |
| **Editorial** | fashion, documentary | 3.5 px | barely smooths, keeps the character |
| **Studio** | commercial, clean | 7 px | even tone, flat light, no glow |

Every preset also carries its own opacities and skin tint, so choosing **Editorial**
does not just blur less — it restores more texture and adds almost no glow.

---

## The two sliders

- **Strength** (0–100) multiplies every opacity and blur radius. At 0 the stack is
  still built, at a whisper; the switches below are how you turn a layer off.
- **Texture** (0–100) decides how much of the high pass comes back. At 0 there is no
  detail layer at all; at 100 skin keeps everything the smoothing would have eaten.

Beneath them, six switches: even the skin tone, lift the shadows, glow, depth,
warmth, give the texture back. Off means the layer is not created — not created and
then hidden.

The line under the switches is live: it is the exact plan for the current document,
before anything is made.

---

## The radii follow the document

Blur radii are quoted at a document **1800 px on the short side** and scaled from
there, clamped between 0.45× and 2.6×. A 600 px web crop gets less blur than a
45-megapixel raw, because 8 px of blur is a lot on one and invisible on the other.

The numbers the tests pin down, for **Natural, strength 60, on an 1800 px photo**:

| Layer | Radius | Opacity |
|---|---|---|
| Detail | 1.9 px high pass | 23 % |
| Warmth | — | 6 % |
| Depth | — | 12 % |
| Glow | 13.6 px | 6 % |
| Lift | 8.8 px | 10 % |
| Smooth | 4 px | 40 % |

---

## Retouching one area only

Make a selection first — the eyes, the cheek, the jaw — and the **blur and the high
pass** are confined to it, because Photoshop filters respect the active selection. The
new layers arrive with that selection as their mask, so you can feather or paint it in
afterwards. That turns the panel into a local dodge-and-burn tool:

1. Lasso the shadows under the eyes, feather it (Shift+F6, 20 px).
2. Turn **Glow**, **Depth** and **Warmth** off, leave **Lift** on.
3. Press Retouch. You get one lift layer, masked to the area you picked.

The panel tells you when a selection is active in the line under the title.

---

## What it deliberately does not do

This is a **tone, texture and light** retoucher. It does not find the face for you,
and it does not reshape it.

Facial-landmark detection and automatic slimming are not in Photoshop's UXP
scripting API. A plugin that claims to "slim the jaw" without seeing the face can only
guess, and a guessed warp on somebody's portrait is worse than no feature at all.
Shape work stays where it belongs — in Photoshop's own tools, non-destructively:

1. Select the face, **Ctrl+J** it to a new layer.
2. Convert it to a smart object (so the warp stays editable).
3. **Filter → Liquify** to taste, or **Edit → Puppet Warp** for the jaw and shoulders.
4. Retouch the tone with this panel afterwards.

Also out of scope, for the same honesty: blemish detection (use the Spot Healing
Brush — it is better at it than any script), teeth whitening and eye colour (a Hue/
Saturation layer and a mask, made in four clicks and *already* editable), and hair
masking.

---

## Testing

```
cd photoshop-face-retouch
npm test
```

31 tests, no Photoshop and no network. They cover the arithmetic that decides the
radii and opacities, the presets, the document-size scaling, the clamps on absurd
input, the plan's layer order, the fill descriptors — including the fact that the
green channel of a solid colour is called `grain` — and that every descriptor that
reaches `batchPlay` is plain JSON. The other half checks the things Photoshop checks
before it will load a plugin: the manifest, the entry points the panel registers, the
icon sizes straight out of the PNG headers, and that every file the page references
is in the folder.

The split exists so that the parts worth testing do not need Photoshop:

| File | Runs where | What is in it |
|---|---|---|
| `src/core.js` | node | presets, scaling, radii, opacities, the plan |
| `src/descriptors.js` | node | blur, high pass, colour fill, group |
| `src/retouch.js` | Photoshop | `executeAsModal`, DOM, `batchPlay`, the report |
| `src/panel.js` | Photoshop | the DOM and the buttons |
| `tests/core.test.js` | node | 15 checks: presets, scaling, radii, the plan |
| `tests/descriptors.test.js` | node | 8 checks: the payloads that reach `batchPlay` |
| `tests/manifest.test.js` | node | 8 checks: the manifest, icons, files, permissions, the element ids the panel looks up |

---

## When something refuses

Every layer is built independently and every failure is caught, recorded and shown in
the panel with a red dot — "6 layers built, 1 refused". The retouch continues. The
usual causes:

- **The base layer is a smart object** and the filter cannot be applied to it
  directly — Photoshop offers a smart filter instead, which the DOM accepts on 24.x
  but not on every build. The copy is still made and blended.
- **A layer is locked** or part of a linked set, so the copy cannot be moved into the
  group. It stays above the photo, correctly ordered, and **Remove** still finds it
  by name.
- **The document has no layers** other than a previous retouch. The panel says so
  instead of guessing.

Everything the plugin makes is named `FR · …`, which is how **Compare** and
**Remove** find it again after you have renamed nothing and moved everything.

---

## Licence

GPL-2.0-or-later. Copyright © 2026 Emmanuel Suah and the Presence Guard
contributors.
