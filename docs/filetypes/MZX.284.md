# MZX File Format Specification (2.00 through 2.84X)

This file describes the old MZX 2.xx world format, including save files and
MZB files. Save format modifications between versions are somewhat of a mess,
so please report any inaccuracies in this document.

| data type | description  |
|-----------|--------------|
| b         | byte (8-bit)
| bs        | byte (8-bit, signed)
| w         | word (16-bit, little endian)
| ws        | word (16-bit, little endian, signed)
| d         | dword (32-bit, little endian)
| ds        | dword (32-bit, little endian, signed)
| sN        | string of N size (ie N bytes)
| s...      | string of variable size

## Header

All MZX worlds (including 2.90+ worlds) begin with the same 29 byte header.

| pos | size | description |
|-----|------|-------------|
| 0   | s25  | World title
| 25  | b    | Protection method (always zero)
| 26  | s3   | Magic

The following magic values are currently known:

| magic       | world and file format version |
|-------------|-------------------------------|
| `MZX`       | 1.00
| `MZ2`       | 2.00 through 2.51
| `MZA`       | 2.51s1
| `M\x02\x09` | 2.51s2 through 2.61 (including MZXAk, SMZX 1.00a)
| `M\x02\x11` | Used for decrypted worlds by newer MZX versions. Likely derived from misreading 2.51s2 magic `M\002\011` (octal) as hex.
| `M\x02\x32` | 2.62 and 2.62b (from octal `M\002\062`)
| `M\x02\x41` | 2.65 (from decimal `65`)
| `M\x02\x44` | 2.68 (from decimal `68`) (see notes)
| `M\x02\x45` | 2.69 (from decimal `69`)
| `M\x02\x46` | 2.69b
| `M\x02\x48` | 2.69c
| `M\x02\x49` | 2.70
| `M\x02\x50` | 2.80X (from decimal `80`) (first port releases)
| `M\x02\x51` | 2.81X (and so on...)
| `M\x02\x52` | 2.82X
| `M\x02\x53` | 2.83
| `M\x02\x54` | 2.84X
| `M\x02\x5A` | 2.90X
| `M\x02\x5B` | 2.91X
| `M\x02\x5C` | 2.92X

Encrypted worlds from earlier MZX versions begin with a different header:

| pos | size | description |
|-----|------|-------------|
| 0   | s25  | World title
| 25  | b    | Protection method (always non-zero)
| 26  | s15  | Password (encrypted)
| 42  | s3   | Magic (not encrypted)

The password can be decrypted with the following algorithm:
```C
/**
 * This code is copied from the MZX source code, which is GPL 2+ licenced.
 */
char magic_code[16] = "\xE6\x52\xEB\xF2\x6D\x4D\x4A\xB7\x87\xB2\x92\x88\xDE\x91\x24";

for(i = 0; i < 15; i++)
{
  password[i] ^= magic_code[i];
  password[i] -= 0x12 - protection_method;
  password[i] ^= 0x8D;
}
```
The rest of the MZX file follows XORed with a byte calculated from the password.
In practice, this byte can be determined by using the character set starting at
the 44th byte of the encrypted world, the ID chars space damage value at the
3955th byte of the encrypted world (which is almost always 0 unencrypted) or by
using recognizable colors in the palette (offsets 4197 through 4244).

Save files use the following header instead:

| pos | size | description |
|-----|------|-------------|
| 0   | s5   | Magic
| 5   | w    | World version (separate from file format version)
| 7   | b    | Current board number

The following save file magic values are currently known:

| magic         | file format version |
|---------------|---------------------|
| `MZSV2`       | 2.00 through 2.51
| `MZXSA`       | 2.51s1
| `MZS\x02\x09` | 2.51s2 through 2.61 (including MZXAk, SMZX 1.00a)
| `MZS\x02\x32` | 2.62 and 2.62b
| `MZS\x02\x41` | 2.65
| `MZS\x02\x44` | 2.68
| `MZS\x02\x45` | 2.69
| `MZS\x02\x46` | 2.69b
| `MZS\x02\x48` | 2.69c
| `MZS\x02\x49` | 2.70
| `MZS\x02\x50` | 2.80X (first port releases)
| `MZS\x02\x51` | 2.81X
| `MZS\x02\x52` | 2.82X
| `MZS\x02\x53` | 2.83
| `MZS\x02\x54` | 2.84X
| `MZS\x02\x5A` | 2.90X
| `MZS\x02\x5B` | 2.91X
| `MZS\x02\x5C` | 2.92X

Notes:
* The world title is a duplicate of the name of the title board.
* The world title and password are null terminated within their fixed buffers.
* The password may be 15 chars long, in which case it will NOT be null
  terminated. The null terminator must be added by the decryption algorithm.
* The MZX 2.68 magic was erroneously never saved with world files, meaning
  worlds from MZX 2.68 relying on MZX 2.68 features won't work (as they are
  versioned for MZX 2.65).

## Global Data

World data immediately follows the header.

### World Block 1

Total block length is 4129 bytes.

| pos  | size    | description |
|------|---------|-------------|
| 0    |   s3584 | Character set (see CHR.md)
| 3584 |    s128 | ID Chars block 1 (thing chars)
| 3712 |    s195 | ID Chars block 2 (animations and colors)
| 3907 |      s1 | ID Chars missile color
| 3908 |      s3 | ID Chars bullet colors
| 3911 |    s128 | ID Chars block 3 (damage)
| 4039 | s15 * 6 | Status counter names (null terminated)

### Save Block 1

This block is only present in save files.
Total block length is 73 bytes plus the length of the current playing mod name.

| pos  | size  | description |
|------|-------|-------------|
| 0    | b * 16| Keys
| 16   | b     | Blind timer
| 17   | b     | Firewalker timer
| 18   | b     | Freeze time timer
| 19   | b     | Slow time timer
| 20   | b     | Wind timer
| 21   | w * 8 | Saved player X positions
| 37   | w * 8 | Saved player Y positions
| 53   | b * 8 | Saved player board numbers
| 61   | b     | Player color
| 62   | b     | Under player ID (see notes)
| 63   | b     | Under player color
| 64   | b     | Under player param
| 65   | b     | Message edges enabled?
| 66   | b     | Scroll base color
| 67   | b     | Scroll corner color
| 68   | b     | Scroll pointer color
| 69   | b     | Scroll title color
| 70   | b     | Scroll arrow color
| 71   | w     | Mod playing length (see notes)
| 73   | s...  | Mod playing (NOT null terminated)

Notes:
* The under player fields are used as an extra "layer" beneath where the player
  is standing. This is so e.g. the player can be placed on top of something that
  is on top of a floor and the floor won't be erased when the thing immediately
  beneath the player is pushed to the under layer.
* Prior to MZX 2.83, the mod playing length and mod playing fields were a single
  13 byte null terminated buffer. Prior to MZX 2.51s2, the current playing mod
  was not saved at all (as it was always the same as the current board mod).

### World Block 2

Total block length is 72.

| pos  | size    | description |
|------|---------|-------------|
| 0    | b       | Viewport edge color
| 1    | b       | First board number
| 2    | b       | Endgame board number (255: no endgame board)
| 3    | b       | Death board number (254: same position, 255: restart board)
| 4    | w       | Endgame board teleport X
| 6    | w       | Endgame board teleport Y
| 8    | b       | Game over SFX enabled (1) or disabled (0)
| 9    | w       | Death board teleport X
| 11   | w       | Death board teleport Y
| 13   | ws      | Starting lives
| 15   | ws      | Lives limit
| 17   | ws      | Starting health
| 19   | ws      | Health limit
| 21   | b       | Enemies' bullets hurt other enemies
| 22   | b       | Clear messages and projectiles on exit
| 23   | b       | Can only play world from a 'SWAP WORLD'
| 24   | s3 * 16 | Palette (see PAL.md)

### Save Block 2

FIXME

### Global Robot Offset

In an unencrypted world file this will always be at offset 4230.

| pos  | size | description |
|------|------|-------------|
| 0    | d    | Offset of global robot within the file
| 4    | b    | Number of boards (1-250) or 0 for SFX table

### Custom SFX Table

If the board count byte is 0, a custom SFX table is present. If there is not a
custom SFX table in the world file, the world will use the MZX default SFX
instead. The SFX table begins with:

| pos  | size | description |
|------|------|-------------|
| 0    | w    | Length of the SFX table in bytes

Then, for each SFX (0 through 49):

| pos  | size | description |
|------|------|-------------|
| 0    | b    | Length of SFX string, including null terminator (69 max)
| 1    | s... | SFX string

Finally:

| pos  | size | description |
|------|------|-------------|
| 0    | b    | Number of boards in the world (1-250)

### Board Names, Sizes, and Offsets

Following the board count byte is the board names list and the board sizes and
offsets table.

For each board:
| pos  | size | description |
|------|------|-------------|
| 0    | s25  | Board names (null terminated)

For each board:

| pos  | size | description |
|------|------|-------------|
| 0    | d    | Size of board data within the file
| 4    | d    | Offset of board data within the file

Typically, board data immediately follows these tables, and the global robot
is placed at the very end of the world file.

## Boards

Each board begins with the following fields:

| pos | size | description |
|-----|------|-------------|
| 0   | b    | Board mode (see notes)
| 1   | b    | Varies (see notes)

Notes:
* Boards in DOS MZX had a fixed size of 100x100. DOS MZX used the board mode
  field to determine what the maximum dimensions of the board were within the
  board space. MegaZeux 2.80+ supports boards of any size and ignores this field.
* If the second byte of the board non-zero, it is the lower byte of the level_id
  plane's width field. If this byte is `00`, the board has an overlay, and has
  the following extra field. As a consequence of this, the board width in these
  versions is not allowed to be a multiple of 256 (which would have a lower byte
  of `00`).

| pos | size | description |
|-----|------|-------------|
| 2   | b    | Overlay mode (1: enabled, 2: static, 3: transparent)

### Board RLE Planes

Board and overlay contents are compressed with run length encoding. Each board
has either 6 or 8 planes (depending on whether the overlay is enabled), which
immediately follow the mode fields and are stored in the following order:

| name              | description |
|-------------------|-------------|
| overlay_char      | Overlay char data (only if overlay is enabled)
| overlay_color     | Overlay color data (only if overlay is enabled)
| level_id          | Board thing IDs
| level_color       | Board thing colors
| level_param       | Board thing parameters
| level_under_id    | Board thing IDs for floors beneath non-floor things.
| level_under_color | Board thing colors for floors beneath non-floor things.
| level_under_param | Board thing parameters for floors beneath non-floor things.

Each compressed plane starts with the following fields:

| pos | size | description |
|-----|------|-------------|
| 0   | w    | Width
| 2   | w    | Height

The RLE data follows after. Values 0 through 127 represent a single literal
value in the uncompressed data. If bit 7 is set, instead a run of length
`(byte & 127)` should be filled with the next byte in the stream. The RLE plane
can be decompressed with the following algorithm:

```C
int i = 0;
while(i < width * height)
{
  current_char = *(stream++);
  if(!(current_char & 0x80))
  {
    plane[i++] = current_char;
  }
  else
  {
    int runsize = current_char & 0x7F;
    current_char = *(stream++);

    for(int j = 0; j < runsize; j++)
      plane[i++] = current_char;
  }
}
```

The RLE stream ends when the board data has been filled, and the next compressed
plane or the board parameters block immediately follows.

### Board Parameters

#### MegaZeux 2.00 through 2.82X

The following is the board parameters format for MZX versions prior to 2.83.

| pos | size | description |
|-----|------|-------------|
| 0   | s13  | Mod playing (null terminated)
| 13  | b    | Viewport X
| 14  | b    | Viewport Y
| 15  | b    | Viewport width
| 16  | b    | Viewport height
| 17  | b    | Can shoot?
| 18  | b    | Can bomb?
| 19  | b    | Fire burns brown?
| 20  | b    | Fire burns space?
| 21  | b    | Fire burns fakes?
| 22  | b    | Fire burns trees?
| 23  | b    | Explosions leave (0: space, 1: ash, 2: fire)
| 24  | b    | Save mode (0: enabled, 1: disabled, 2: sensor only)
| 25  | b    | Forest to floor?
| 26  | b    | Collect bombs?
| 27  | b    | Fire burns forever?
| 28  | b    | Board # to north
| 29  | b    | Board # to south
| 30  | b    | Board # to east
| 31  | b    | Board # to west
| 32  | b    | Restart if zapped?
| 33  | w    | Time limit
| 35  | b    | Last key pressed
| 36  | ws   | Input (numeric value)
| 38  | b    | Input length
| 39  | s81  | Input (string value) (null terminated)
| 120 | b    | Last player direction
| 121 | s81  | Bottom message (null terminated)
| 202 | b    | Bottom message timer
| 203 | b    | Lazerwall timer
| 204 | b    | Bottom message row
| 205 | b    | Bottom message column
| 206 | w    | Scroll X
| 208 | w    | Scroll Y
| 210 | w    | Scroll locked X
| 212 | w    | Scroll locked Y
| 214 | b    | Player locked NS?
| 215 | b    | Player locked EW?
| 216 | b    | Player attack locked?
| 217 | b    | Volume
| 218 | b    | Volume increment
| 219 | b    | Volume target

Notes:
* Some MZX worlds created with an alpha version of MZX 2.83 have boards in this
  format. Since these were saved with the 2.83 magic, they can't be loaded in
  newer versions of MZX, but they can be fixed by hex editing the version to
  MZX 2.82.

#### MegaZeux 2.83 and 2.84X

The following is the board format for MZX versions 2.83 and 2.84X.

| pos | size | description |
|-----|------|-------------|
| 0   | w    | Mod playing length
| 2   | s... | Mod playing (NOT null terminated)

This block immediately follows the playing mod:

| pos | size | description |
|-----|------|-------------|
| 0   | b    | Viewport X
| 1   | b    | Viewport Y
| 2   | b    | Viewport width
| 3   | b    | Viewport height
| 4   | b    | Can shoot?
| 5   | b    | Can bomb?
| 6   | b    | Fire burns brown?
| 7   | b    | Fire burns space?
| 8   | b    | Fire burns fakes?
| 9   | b    | Fire burns trees?
| 10  | b    | Explosions leave (0: space, 1: ash, 2: fire)
| 11  | b    | Save mode (0: enabled, 1: disabled, 2: sensor only)
| 12  | b    | Forest to floor?
| 13  | b    | Collect bombs?
| 14  | b    | Fire burns forever?
| 15  | b    | Board # to north
| 16  | b    | Board # to south
| 17  | b    | Board # to east
| 18  | b    | Board # to west
| 19  | b    | Restart if zapped?
| 20  | w    | Time limit

The following block is only present in save files.

| pos     | size | description |
|---------|------|-------------|
| 0       | b    | Last key pressed
| 1       | ws   | Input (numeric value)
| 3       | w    | Input length = x
| 5       | s... | Input (string value) (NOT null terminated)
| 5+x     | b    | Last player direction
| 6+x     | w    | Bottom message length = y
| 8+x     | s... | Bottom message (NOT null terminated)
| 10+x+y  | b    | Bottom message timer
| 11+x+y  | b    | Lazerwall timer
| 12+x+y  | b    | Bottom message row
| 13+x+y  | b    | Bottom message column
| 14+x+y  | w    | Scroll X
| 16+x+y  | w    | Scroll Y
| 18+x+y  | w    | Scroll locked X
| 20+x+y  | w    | Scroll locked Y

The following are in both world files and save files.

| pos | size | description |
|-----|------|-------------|
| 0   | b    | Player locked NS?
| 1   | b    | Player locked EW?
| 2   | b    | Player attack locked?

The final block is only present in save files.

| pos | size | description |
|-----|------|-------------|
| 0   | b    | Volume
| 1   | bs   | Volume increment
| 2   | b    | Volume target

### Board Robots, Scrolls, and Sensors

The board robot list, scroll list, and sensor lists follow the board parameters.
Each list is stored as a single byte `N` indicating the number of robots, scrolls,
or sensors on the board. This byte is followed by `N - 1` stored robots/scrolls/sensors
(as ID 0 is the global robot or is invalid for scrolls/sensors). Each robot,
scroll, or sensor in the list immediately follows the prior, and the next list
starts immediately after the last object in the previous list. The board data
ends after all three lists have been read.

## Robots

Several of these fields are only used during runtime and thus initial values
had to be saved into the world data.

| pos | size | default   | description |
|-----|------|-----------|-------------|
| 0   | w    |           | Program length
| 2   | w    |           | unused
| 4   | s15  |           | Robot name (null-terminated)
| 19  | b    | 2         | Robot char
| 20  | w    | 1         | Current position in program
| 22  | b    | 0         | Position within current line (for `WAIT`/`GO`/etc)
| 23  | b    | 1         | Robot cycle
| 24  | b    | 0         | Cycle counter
| 25  | b    | 1         | Bullet type
| 26  | b    | 0         | Locked status (0: unlocked, 1: locked)
| 27  | b    | 0         | Can lavawalk (0: no, 1: yes)
| 28  | b    | 0         | Walk direction (0: idle)
| 29  | b    | 0         | Last touch direction
| 30  | b    | 0         | Last shot direction
| 31  | w    | see notes | X position
| 33  | w    | see notes | Y position
| 35  | b    | 0         | Status (0: not run, 1: has run, 2: not run and has been sent a label)
| 36  | ws   | 0         | Local (see notes)
| 38  | b    | 1         | Used (1) or unused (0)
| 39  | ws   | 0         | Loopcount (see notes)

Notes:
* The unused 2-byte field at offset 2 was the program memory pointer in DOS
  versions.
* The X/Y position variables are generally initialized to 0 in world files
  from DOS versions. Versions 2.80+ typically save the actual X and Y position
  in these variables in world files.
* The local field in this block is completely ignored in versions 2.80+ and
  is saved in the robot save block instead.
* The loopcount field in this block is completely ignored in 2.84X save files
  and is saved in the robot save block instead.

In save files from MegaZeux 2.80 and onward, an extra data block follows:

| pos | size                   | description |
|-----|------------------------|-------------|
| 41  | ds                     | Loopcount
| 45  | ds (32 times)          | Local#
| 173 | d                      | Stack length
| 177 | d                      | Current position in stack
| 181 | d (stack length times) | Robot stack

Notes:
* The loopcount field in this block was not present prior to 2.84 (i.e. the
  block started with the local counters instead and was 4 bytes shorter).

In both world and save files, the robot program immediately follows the above
block(s).

### Robot Programs

* Valid Robotic programs begin with the byte `FF`.
* A sequence of commands in the following format follows:
  `LL XX ... LL`, where `LL` is the length of the bytecode command (0-255) and
  XX is the command number.
* Between the command number and terminating length byte, a number of params in
  one of two formats follows: `00 XX XX`, where `XX XX` is a little-endian word,
  or `LL [string] 00`, where `LL` is the length of the null-terminated param
  string that follows (including the null terminator). The number of required
  params varies between commands.
* The next command's prefixed length byte immediately follows the previous
  command's trailing length byte.
* A final byte of `00` (indicating a command of length zero) terminates the
  program.
* The shortest valid Robotic program is `FF 00` with a length of 2.
* In older worlds, robots marked unused may have uninitialized memory saved
  where their program would usually go (the global robot from *Slave Pit* is an
  example).

## Scrolls

Each scroll block is 7 bytes plus the length of the scroll text long.

| pos | size | description |
|-----|------|-------------|
| 0   | w    | Number of lines
| 2   | w    | unused
| 4   | w    | Scroll text length in bytes
| 6   | b    | Used (1) or unused (0)
| 7   | s... | Scroll text

Notes:
* The scroll text must begin with a `01` hex byte, must contain at least one
  line break (`0A` hex), and must be null terminated (i.e. the smallest valid
  scroll is `01 0A 00` with number of lines=1 and length=3).
* The unused 2-byte field corresponded to the pointer of the scroll text
  allocation in memory in DOS versions of MZX.

## Sensors

Each sensor block is 32 bytes long. The sensor name and robot to message fields
must also be null terminated within their fixed-length buffer.

| pos | size | description |
|-----|------|-------------|
| 0   | s15  | Sensor name
| 15  | b    | Sensor char
| 16  | s15  | Robot to message
| 31  | b    | Used (1) or unused (0)

## MegaZeux Board (MZB) files

MZB header:

| pos | size | description |
|-----|------|-------------|
| 0   | b    | Always hex `FF`
| 0   | m3   | Magic (`MB2` for 2.00 through 2.51s1, same as world magic after)

The header is immediately followed by a single board (including robots,
scrolls, and sensors as-needed) as described above.
