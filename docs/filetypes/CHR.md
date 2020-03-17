# CHR File Format

This file format describes one or more MZX graphical characters. A CHR file
contains raw graphical data only. Each character in the CHR file consists of
14 bytes representing each graphical row of the character. Within the byte, each
pixel of the row is encoded either as a single bit (MZX) or as two bits (SMZX).
The first byte of the next character immediately follows the 14th byte of the
previous character.

## MZX

The most significant bit of the byte represents the leftmost pixel in the row
and the least significant bit of the byte represents the rightmost pixel.

Example: setting the third, fifth, and seventh pixels.

| 128 |  64 |  32 |  16 |   8 |   4 |   2 |   1 |
|-----|-----|-----|-----|-----|-----|-----|-----|
| `0` | `0` | `1` | `0` | `1` | `0` | `1` | `0` |

|         |     |
|---------|-----|
| Binary  | `00101010`
| Hex     | `2A`
| Decimal | (128 * 0) + (64 * 0) + (32 * 1) + (16 * 0) + (8 * 1) + (4 * 0) + (2 * 1) + (1 * 0) = 42.

## SMZX

The most significant two bits of the byte represent the leftmost pixel in the
row and the least significant two bits represent the rightmost pixel. The four
colors are encoded as the following bit pairs and quaternary digits:

| Color | Binary | Quaternary |
|-------|--------|------------|
| 1     | `00`   | `0`
| 2     | `01`   | `1`
| 3     | `10`   | `2`
| 4     | `11`   | `3`

Example: setting the first and second pixels to color 2, third to color 4, and
fourth to color 1.

|  64 |  16 |   4 |   1 |
|-----|-----|-----|-----|
| `1` | `1` | `3` | `0` |

|            |      |
|------------|------|
| Quaternary | `1130`
| Binary     | `01011100`
| Hex        | `5C`
| Decimal    | (64 * 1) + (16 * 1) + (4 * 3) + (1 * 0) = 92
