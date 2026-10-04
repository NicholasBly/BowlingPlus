# BowlingPlus pin picture system: a guide for engineers and AI agents

This explains, precisely, how BowlingPlus puts a player's own picture on the pins in *Bowling by Jason Belmonte* (v1.907, Unity 6000.0.67f1, IL2CPP arm64), and how the bundled preview page (`BowlingPlus-pin-preview.html`) reproduces it. Everything here was verified against the game's files and on a device unless marked otherwise. Read it before changing anything pin-related.

---

## 1. What the game has

### Pin models and materials

- **The real pins** come from the prefab `PinRuntimeObject` (bundle `Data/Raw/AssetBundles/prefabs`).
  - `PinPhysic` is the Rigidbody, with capsule and mesh colliders.
  - Under it sits `PinVisual`, and under that `PinHighQuality` and `PinLowQuality`. Both are MeshRenderers with identity local rotation.
  - `PinMirror` is the lane reflection.
  - `Pin` fields: `_objVisualRoot` `0x28`, `_highQualityRender` `0x30`, `_lowQualityRender` `0x38`, `_physic` `0x40`, `_mirrorRenderer` `0x50`.
- **More pin models** live in the scene (`level1`), all named "keg" (mesh `polySurface62`, material `keg_d`):
  - the pins the pinsetter carries down for a new rack (children of `pinspotter_1:jointNN`)
  - pin deck copies, reflections (`MirrorRender`, `MirrorRenderOnPindeck`) and the Arsenal display (`MirrorRenderArsenal`)
- **`InventaryData`** lists all of them and holds the two materials they share:
  - `pinMaterial` (`0xD8`)
  - `mirrorPinMaterial` (`0xE0`)
  - `InventaryData.ApplyTexturePins` (at each game-mode switch) does `renderer.material = m` for every listed model.
- **The real pins' own material** is `pin_diff` (shader *Legacy Shaders/Reflective/Diffuse*: `_MainTex` + `_Cube`).
- **The reflection's material** is `keg_d` (*Mobile/Diffuse*). Both show the pin picture as `_MainTex`.
- **The shader uses `_MainTex` alpha as its shininess (reflection) mask.** The game's picture is fully opaque, so a custom picture must be opaque too, or pins turn dull or patchy.

### The game's pin picture

- **`pins1`**, 512 × 512, in `Data/Raw/AssetBundles/pins/fulltextures`. That bundle holds 178 pin pictures, the Pin Arsenal designs, all 512 × 512.
- **It's fully opaque** (alpha 255 everywhere).

### The pin mesh and its picture layout

- **Mesh `polySurface060`** (the high-quality pin): 652 corners (each with its own position, UV and normal) and 1150 triangles.
- **The pin stands along +Y**, from `y = 0` (base) to `y = 0.381 m` (15 in, regulation height). Max radius ≈ 0.06 m.
- **UV layout**, found by rasterizing every triangle into a 512² map of (height, angle):
  - **Two half-pin shapes side by side.** Each is a *side projection* of one half of the pin: the left shape is side A (angle 270°), the right shape is side B (angle 90°).
  - **The halves' curved edges meet** on the pin (the seams, at angles 0° and 180°).
  - **The head is at the top of the picture**, the base at the bottom.
  - **The flat underside** (all three corners at `y < 0.002`) is the disc in the middle.
  - **About 36% of the picture is unused.**
- **The other models have the same layout with rougher edges:** low-quality (`polySurface60`) and reflection / pinsetter (`polySurface62`), with 1–2 px differences at 512.
- **Consequence:** near a half-pin's curved edge, the picture squeezes a lot of surface into few pixels. Evenly spaced shapes drawn in the picture are *not* even on the pin, and things drawn at the two edges don't line up. That is why the wrap sheet exists.

### Coordinate conventions (important)

- **Mesh data was exported with UnityPy's OBJ exporter.** It flips X to make a right-handed system, which matches three.js and the software renderers used for checks.
- **Angle** = `atan2(z, x)` in degrees, in that exported space.
- **Verified:** text that reads correctly in the game also reads correctly in a right-handed renderer with these positions, and the wrap formula below produces readable, non-mirrored text in the game.
- **Do not "fix" the handedness** without re-checking with a text test (see §6).
- **UV v=1 is the top row of the picture** (PNG row 0). Unity's `LoadImage` and three.js (`flipY = true`) agree.

---

## 2. The two picture layouts BowlingPlus accepts

BowlingPlus decides by the picture's shape (`OilUI.mm`, `SavePinImage`).

| Picture | Treated as | What happens |
|---|---|---|
| width ≥ 1.5 × height (2:1 is the norm, 2048 × 1024 ideal) | **Wrap sheet** | converted to the game's layout from the 3D mesh (§3) |
| anything else | **Game layout** | stretched to square if needed, then edge-cleaned (§4) |

### Size rules (identical in the preview page)

- **Wrap:**
  - `ww = trunc(min(4096, max(512, w)))`
  - `wh = trunc(min(2048, max(256, h × ww / w)))`
  - output `side = ww ≥ 3000 ? 2048 : 1024`
- **Game layout:** `side = trunc(min(2048, max(512, max(w, h))))`
- **Before processing,** the picture is drawn onto **white** at that size, so transparent parts become pin white.

### The wrap sheet

- **Left → right** is once around the pin. The left and right edges meet.
  - **Side A** (the left half-pin, 270°) is at **¼** of the width.
  - **Side B** (90°) is at **¾**.
  - **The game's seams** are at 0, ½ and 1. They don't matter in the wrap: both halves sample the same sheet.
- **Top → bottom** is height, from the top of the head (0.381 m) to the base (0), linear in height.
  - The neck band is roughly 8–11 in.
  - The dome compresses toward the very top row, like paper on a ball.
- **The pin's flat underside** keeps the game's own look (taken from the bundled template of `pins1`).

---

## 3. Wrap → game layout (`PinWrapToLayout`, `src/PinWrap.h`)

For every triangle of the mesh that is not part of the flat underside:

1. Project its 3 corners to picture pixels: `X = u × side`, `Y = (1 − v) × side`.
2. For each pixel center `(x + 0.5, y + 0.5)` inside the triangle (barycentric weights ≥ −1e-4), interpolate the 3D point `(qx, qy, qz)` on the pin.
3. Wrap coordinates:
   - `ang = atan2(qz, qx)` in degrees
   - `u = ((720 − ang) mod 360) / 360` (the `720 − ang` direction is what makes text read correctly; `ang − 180` mirrors it)
   - `v = 1 − clamp(qy / 0.381, 0, 1)`
4. Sample the sheet **bilinearly** at `(u × ww − 0.5, v × wh − 0.5)`, wrapping horizontally and clamping vertically.

Underside triangles copy the template pixel instead.

The output starts all zero. Pixels no triangle touches are filled in the next step.

**Why seams can't happen:** a 3D point on a seam belongs to triangles in both half-pin shapes, and both compute the same `(u, v)`, so both sides get the same color.

---

## 4. Edge fill (`PinFillAround`, `src/PinWrap.h`): the seam / crack fix

**The problem:** the GPU's filtering and mipmaps blend in pixels from just *outside* a half-pin shape at its edges. This gets worse on small, far pins. Any different color (or transparency) around the shapes shows as a crack down the pin's side.

**The fix:**

1. **The keep mask:** the high-quality mesh's shapes rasterized at 512², **eroded by 2 px**, so the low-quality, reflection and pinsetter models, whose edges stick out 1–2 px, are covered too. It ships as bits in `src/PinMask.h` (32 768 bytes, row-major, MSB first) and is scaled to the output by nearest sampling.
2. **Clean edges (game-layout pictures only).** A hand-drawn outline can sit slightly inside the real shapes, leaving slivers of the drawing's background on the pin.
   - Find the background as the most common color outside the shapes (5 bits per channel histogram).
   - Then breadth-first from the outside inward, up to `side / 64` px (≈ 16 at 1024).
   - Drop from the keep mask every pixel within `side / 170` px (≈ 6, the blended edge band), plus any pixel whose color is within 40 (sum of absolute RGB difference) of the background.
3. **Flood:** multi-source breadth-first from every kept pixel. Each other pixel takes the RGB of the nearest kept pixel (4-neighbour order).
4. **Alpha = 255 everywhere** (the shader's shininess mask; matches `pins1`).

**Measured results:**
- A gray-surrounded test picture loses its seam line.
- The user's hand-drawn picture went from 2165 background pixels in the base band to 1.

---

## 5. At runtime (`src/Game.mm`, "your own pin image")

### Storage

- **The processed PNG:** `Documents/BowlingPlus/pin_image.png`.
- **The original as picked:** `pin_image_source.png`. When a version improves processing, `PinImageUpgrade` (`OilUI.mm`) redoes the PNG from it once per version key.

### Loading

`Texture2D(2, 2)` + `ImageConversion.LoadImage(tex, bytes)`, then `DontUnload`, and a strong GC handle is kept. The texture is reloaded when the file's modification date changes.

### Applying (`PinImageTick`)

- **Every 30 frames**, it sets the texture as `mainTexture` on:
  - `InventaryData.pinMaterial` and `mirrorPinMaterial` (pinsetter, pin deck, reflections, Arsenal pins)
  - the shared material of every real pin's high-quality, low-quality and mirror renderer (all `PinHolder`s, i.e. all visible lanes)
- **The game's texture is kept per material** (up to 16) to put back.
- **Every frame**, the already-changed materials are re-checked, so a reset by the game never shows.
- **When the game swaps pins** (Pin Arsenal), the new texture becomes the one to restore and the player's picture goes on again.
- **Switch off:** the game's textures are put back.
- **Looks only:** physics is untouched, so it's safe in online play.

**Debug info line:** `pin image: on=… materials=… size=… fails=…`

### Bugs found on device, and their fixes

- **Pop-in at every rack** (1.4.6): the pinsetter's "keg" models weren't covered. Fixed in 1.4.7.
- **A crack down the side** (1.4.6): no edge fill. Fixed in 1.4.7.
- **A crack near the base with a hand-drawn layout** (1.4.7): fixed by the clean-edges step in 1.4.8.

---

## 6. How it was verified (redo these after any change)

1. **3D text test.** Make a wrap sheet with "AB" at ¼ and "CD" at ¾ and 8 evenly spaced spikes in the neck band. Convert it, render the mesh, and check:
   - "AB" faces you when side A does (camera yaw 180°) and reads correctly
   - "CD" does the same at yaw 0°
   - the spikes run evenly across both seams (yaw 90° and 270°)
2. **Port check.** The JavaScript in the preview page is a line-for-line port of `PinWrap.h`. Running both on the same input:
   - wrap path: max difference 1 per channel (float vs double rounding)
   - game-layout path: identical
3. **Seam render.** Render a far-away pin with a low mipmap level, before and after the fill. No line after.
4. **Device:** screenshots of a racked lane and of pins mid-flight, plus "Copy debug info".

---

## 7. Mistakes not to repeat

- **Thinking the game draws every pin facing the same way.** It doesn't: the pinsetter turns each pin a random way (`PinHolder.SetupPins`, one of 16 turns of 22.5°), and the visual follows. The default picture just looks identical from every side. A 1.4.5 feature to "fix" this never activated and was removed.
- **Only changing the real pins' material.** The pinsetter's own pin models showed the old picture at every rack.
- **Leaving transparency or background colors around the shapes.** That's the crack.
- **Assuming evenly spaced art in the game layout stays even on the pin.** It doesn't (side projection). Use the wrap sheet.
- **Flipping the angle direction** without the text test (§6.1). One direction is mirrored.

---

## 8. Files

| File | What |
|---|---|
| `src/PinWrap.h` | `PinShapeAt`, `PinWrapToLayout`, `PinFillAround` (plain C, testable on its own) |
| `src/PinMesh.h` | The mesh: per-corner position and UV, and triangles |
| `src/PinMask.h` | The keep mask (512² bits) |
| `src/PinGuide.h` | Embedded PNGs: the game-layout guide and template, the wrap guide and the wrap template |
| `src/OilUI.mm` | Picking (Photos / Files), `SavePinImage`, sharing the guides, the upgrade |
| `src/Game.mm` | Loading the texture and putting it on the materials (`PinImageTick`), status for the menu |
| `pins/` | The guides, templates and 3D checks as PNGs |
| `BowlingPlus-pin-preview.html` | The PC preview: the same pipeline in JavaScript plus a three.js pin (same mesh and normals, mipmaps on) |

## 9. Extending it

- **A new pin model** in a game update: re-export its mesh and redo the UV analysis. If the layout changed, rebuild `PinMesh.h` and `PinMask.h`, and run §6.
- **A different wrap mapping** (e.g. arc length instead of height): change it in **both** `PinWrap.h` and the page, then run §6.1. Keep `u = ¼` = side A so existing pictures don't move.
- **Different pictures per pin, or a randomized pin picture:** the material is shared, so you would need per-renderer material instances (`renderer.material`), which multiplies the per-frame checks.
