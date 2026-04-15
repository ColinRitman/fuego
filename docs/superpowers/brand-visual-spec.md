# Fuego (XFG) Visual Brand Spec
# AzorAhai Release

> Extracted from: swapxfg/app/styles.go, swapxfg/app/splash.go
> Consumer: web team, social media templates
> Output format: This document is the design token reference.

---

## Core Palette

| Role | Hex | Variable (styles.go) | Usage |
|------|-----|----------------------|-------|
| Base background | `#000000` | — | All surfaces. Dark-first. No light mode. |
| Fire base (near-black) | `#180400` | `FirePalette[0]` | Fire-tinted black for cards, panels, depth |
| Accent (Fuego orange) | `#FF5500` | `ColorAccent` | CTAs, active states, key data, fire fills |
| Accent mid | `#CC3300` | — | Hover states, secondary accents |
| Bullish | `#00CC66` | `ColorBullish` | Positive price / balance changes |
| Bearish / Error | `#FF3344` | `ColorBearish` | Negative changes, error states |
| Spread | `#FFAA00` | `ColorSpread` | Bid/ask spread, yield display |
| Muted | `#555555` | `ColorMuted` | Supporting copy, metadata, inactive elements |
| Own position | `#00CCCC` | `ColorOwn` | User's own orders in orderbook |
| Escrow (pulsing) | `#FFDD00` | `ColorEscrow` | Escrow/locked state indicator |
| Conn OK | `#00FF00` | `ColorConnOK` | Network connected indicator |
| Conn lost | `#FF0000` | `ColorConnLost` | Network disconnected indicator |
| Text primary | `#FFFFFF` | `ColorActiveTab` | Body text on black |
| Text secondary | `#999999` | — | Supporting copy, metadata |
| Text inactive | `#777777` | `ColorInactive` | Disabled / inactive labels |

---

## Fire Gradient (11 stops)

Source: `FirePalette` in `swapxfg/app/styles.go`

| Stop | Hex | FireChars glyph |
|------|-----|-----------------|
| 0 (darkest) | `#180400` | ` ` (space) |
| 1 | `#3D0A00` | `.` |
| 2 | `#6D1400` | `:` |
| 3 | `#9E2000` | `*` |
| 4 | `#D43800` | `░` (U+2591) |
| 5 | `#FF5500` | `▒` (U+2592) |
| 6 | `#FF7F11` | `▓` (U+2593) |
| 7 | `#FFA940` | `█` (U+2588) |
| 8 | `#FFD066` | `█` (U+2588) |
| 9 | `#FFE899` | `█` (U+2588) |
| 10 (lightest) | `#FFFADD` | `█` (U+2588) |

**CSS gradient (left to right, dark to light):**
```css
background: linear-gradient(
  to right,
  #180400, #3D0A00, #6D1400, #9E2000, #D43800,
  #FF5500, #FF7F11, #FFA940, #FFD066, #FFE899, #FFFADD
);
```

---

## Tab / Navigation Styles

| State | Foreground | Background | Bold |
|------|-----------|------------|------|
| Active tab | `#FFFFFF` | `#FF5500` | Yes |
| Inactive tab | `#777777` | transparent | No |
| Input text | `#FFFFFF` | — | Yes |

---

## Fire Symbol (Triangle)

Source: `swapxfg/app/splash.go`

- Geometry: ASCII triangle rendered in terminal with fire simulation
- Always rendered on black background
- Fill: fire gradient (bottom dark, top light) or white outline on black
- Never render on non-black background
- Never use orange fill behind body text

---

## Design Principles

1. **Dark-first.** `#000000` is the only valid background color. No light mode.
2. **Black canvas, orange fire.** Every composition is dark surface + Fuego orange accent.
3. **High contrast required.** The fire must be visible against the night.
4. **No grey-wash neutrals as primaries.** Muted (`#555555`) is for supporting copy only.
5. **Consistent data semantics.** Bullish = `#00CC66`, Bearish = `#FF3344`, Spread = `#FFAA00`. Never repurpose these.

---

## Typography

SwapXFG is a terminal application — all type is monospace. For web/social:

| Role | Recommendation |
|------|---------------|
| Display / headline | Bold monospace or geometric sans-serif |
| Body | System monospace or geometric sans |
| Code / addresses | Monospace, Fuego orange or white on black |
| Never | Serif fonts, light weights on dark background |
