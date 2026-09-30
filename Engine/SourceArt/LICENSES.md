# Engine source art licenses

The third-party files of the engine's source art, with their origin and license; their license texts sit next to
them.

| Files | Origin | License |
| --- | --- | --- |
| `EngineFonts/DejaVuSansCondensed.ttf` | DejaVu fonts 2.37 (https://dejavu-fonts.github.io/, the release `dejavu-fonts-ttf-2.37.zip`), unmodified | the Bitstream Vera fonts license, the DejaVu changes in the public domain, and the Arev fonts license for the glyphs from Arev: `EngineFonts/LICENSE.txt` (the release's `LICENSE`) |
| `EngineFonts/DejaVuSansCondensed-Bold.ttf` | The same release's `ttf/DejaVuSansCondensed-Bold.ttf`, unmodified (ps2-polish P6: the HUD's numbers and headings) | the same license: `EngineFonts/LICENSE.txt` |

The DejaVu fonts' license allows using, copying, modifying and redistributing the fonts, alone or with software; the
fonts may not be sold by themselves, and a modified font must not keep the Bitstream, Vera or DejaVu names. The
engine redistributes the TrueType files unmodified and the glyph pages rasterized from them (`/Engine/EngineFonts`,
[ASSET_FORMATS.md](../../Docs/ASSET_FORMATS.md#fonts)).

The font is also compiled into the engine's binaries ([ps2-polish](../../Docs/PLANS/ps2-polish.md) P5b): `LeonCook
-run=EmbedFont` rasterizes it at 10 and 14 pixels into `Engine/Source/Runtime/GSCore/Private/GSDebugFontData.inl`
(glyph bitmaps, metrics and kerning pairs, no outlines), which every program that links GSCore carries (the PS2's error
screen and GSConformance draw with it: `FGSDebugDraw`). The generated file repeats the Bitstream copyright and
trademark notice and names `EngineFonts/LICENSE.txt`; a binary distribution of the engine or a game ships that notice
with the other third-party notices.
