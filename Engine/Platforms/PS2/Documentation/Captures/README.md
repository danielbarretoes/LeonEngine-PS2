# PCSX2 captures

The fixtures of the manual GS parity check (gate G8, [TESTING.md](../../../../../Docs/TESTING.md#gs-parity-g8)):
screenshots of `GSConformance.elf` taken in PCSX2 with the software renderer, which the next captures are compared
with.

- `GSConformance.png`: the 21 scenes (4 x 6 cells), PCSX2 2.8.2, software renderer, the window's 640 x 480 client area
  (the 448-line frame scaled to it), taken at [ps2-polish](../../../../../Docs/PLANS/ps2-polish.md) P5 when the
  `TexturedCanvas` scene was added; that cell shows what `System.GSReference.Texture.TexturedCanvas` checks (the
  glyph's diagonal ramp, its copy turned in V and tinted, the bilinear scale, the translucent rotated quad). The
  reference and the desktop's emulator are compared pixel by pixel by `System.Renderer.GSEmulator.Conformance`.
  Retaken at [ps2-polish](../../../../../Docs/PLANS/ps2-polish.md) P5b: the labels in the debug font compiled in
  (DejaVu Sans Condensed at 10 pixels, proper case), the grid from y = 2.
