# Recon: a translucent "Ghost" blitter mode

Read-only research, 2026-11-07. Line numbers are from HEAD at that time.

## Selecting the mode

- **How a mode is chosen now:**
  - `GetBlitterMode(PaletteID)` (`gfx.cpp:986-993`) maps `PAL_NONE` → Normal, `PALETTE_CRASH` → CrashRemap and `PALETTE_ALL_BLACK` → BlackRemap; anything else is ColourRemap.
  - Its callers are `DrawSpriteViewport` (`:1005-1021`) and `DrawSprite` (`:1030-1048`). Transparency is a sprite bit (`PALETTE_MODIFIER_TRANSPARENT`).
  - Viewport sprites carry `img` and `pal` through to drawing unchanged, so nothing on the viewport side needs to change.
- **Palette ID bits 24–30 are free** (`table/sprites.h:1713-1735`).
- **Least invasive route:** add `PALETTE_MODIFIER_GHOST = 30` (2 lines). In `gfx.cpp`, both callers then check the bit, skip the recolour lookup and choose `BlitterMode::Ghost` (about 8 lines). Add one enumerator in `blitter/base.hpp`.
- **Recolour is lost.** A ghost drawn this way ignores the palette, so recoloured sprites such as coloured bridges show their base colours. Combining ghost with recolour would need a second per-pixel path.

## Per-blitter work

| Blitter | Dispatch | Ghost work | Lines |
|---|---|---|---|
| 32bpp_simple | per-pixel switch | `ComposeColourRGBA(r,g,b,a/2,*dst)` | ~4 |
| 32bpp_optimized | template per mode; `NOT_REACHED()` default | Normal's alpha loop with alpha halved; keep advancing `src_n` | ~22 |
| 32bpp_anim | template per mode; falls back to Optimized when anim is disabled | as Optimized, plus `*anim = 0` | ~22 |
| 40bpp_anim | template per mode; falls back to Optimized | `RealizeBlendedColour` then compose, `*anim = 0` | ~22 |
| 32bpp_sse2/ssse3/sse4 (`32bpp_sse_func.hpp`) | template per mode; Normal is the `default:` | **no parent fallback is possible** (different sprite format); new SIMD case or a scalar loop | ~25–40 |
| 32bpp_anim_sse4 | same as SSE | SIMD or scalar, plus the anim buffer | ~25–40 |
| 8bpp_simple | per-pixel switch | checkerboard: draw when `(x+y)&1 == 0` | ~4 |
| 8bpp_optimized | per-run switch | checkerboard from the pointer offset (fiddly) | ~8 |
| null | empty | none | 0 |

## Totals and risks

- **Without SSE:** 9 files and about 90 added lines, all additive.
- **With SSE:** 11 files and about 140 lines.
- **Risks:**
  - **The SSE blitter is what most users run, and without SSE work Ghost silently draws opaque there.** That's the `default:` branch: safe, but not translucent.
  - The `NOT_REACHED()` defaults in Optimized and the anim blitters would crash if a case were missed.
  - Mistakes in the anim buffer show up as colour artefacts.
  - The 8bpp checkerboard may shimmer while scrolling.
  - It needs confirming that no NewGRF path can set palette bit 30. That's likely true, since GRF palettes are 14-bit.
- **Prior art:** none reusable. Transparent only darkens, and TransparentRemap does palette lookups.
