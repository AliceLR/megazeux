# PAL File Format

This file format describes an MZX palette. A PAL file contains raw palette data
only. Each color in the PAL file consists of 3 bytes representing the red,
green, and blue components of the color. Each RGB component of a color is a
value from 0 to 63 (inclusive). A PAL file contains 16 (MZX) or 256 (SMZX)
colors, though MZX also allows loading partial palettes.

The PAL file does not encode any SMZX 3 color index information (see PALIDX).

## Examples

| Color               |  R |  G |  B | Hex representation in-file: |
|---------------------|----|----|----|-----------------------------|
| Black               |  0 |  0 |  0 | `00 00 00`
| White               | 63 | 63 | 63 | `3F 3F 3F`
| "Medium Slate Blue" | 30 | 26 | 58 | `1D 1A 3A`

MZX's default palette (hex):
```
00 00 00     Black
00 00 2A     Dk. Blue
00 2A 00     Dk. Green
00 2A 2A     Dk. Cyan
2A 00 00     Dk. Red
2A 00 2A     Dk. Magenta
2A 15 00     Brown
2A 2A 2A     Lt. Grey
15 15 15     Dk. Grey
15 15 3F     Lt. Blue
15 3F 15     Lt. Green
15 3F 3F     Lt. Cyan
3F 15 15     Lt. Red
3F 15 3F     Lt. Magenta
3F 3F 15     Yellow
3F 3F 3F     White
```

# PALIDX File Format

This file format describes SMZX mode 3 subpalette indices. A PALIDX file
contains 256 sets of indices (one for each subpalette). Each set of indices
is a sequence of four bytes; the first indicates color 1 of the subpalette,
the second color 2, and so on.
