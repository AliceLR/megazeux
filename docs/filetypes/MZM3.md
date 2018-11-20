# MZM3 file format

This file format describes a rectangular portion of a MegaZeux board or
layer. This version of the MZM format was introduced in MegaZeux 2.84
and is the most current MZX image format as of MegaZeux 2.91h.

| data type | description  |
|-----------|--------------|
| b         | byte (8-bit)
| w         | word (16-bit, little endian)
| d         | dword (32-bit, little endian)
| sN        | string of N size (ie N bytes)


## Header

| pos | size | description                       | values |
|-----|------|-----------------------------------|--------|
| 0   | s4   | tag                               | "MZM3"
| 4   | w    | width
| 6   | w    | height
| 8   | d    | location in file of robot data    | 0 if not present
| 12  | b    | number of robots in data          | 0 if not present
| 13  | b    | storage mode                      | 0: board, 1: layer
| 14  | b    | MZM includes savegame data        | 0 if false, 1 if true
| 15  | w    | world version                     | see below.
| 17  | b    | reserved
| 18  | b    | reserved
| 19  | b    | reserved

The world version is stored as a 2-byte little endian value where the
upper byte is the major version number and the lower byte is the minor
version number. MZM3 files are forward compatible unless they contain
robot information, in which case the robots in the MZM are replaced
with customblocks.

The header is immediately followed by a board data block or a layer
data block depending on the storage mode value indicated in the header.


## Data (board)

The MZM data is stored in the following format if the "board" storage
mode is selected. The data is composed of (width * height) blocks, where
each block is 6 bytes and contains the following:

| pos | size | description |
|-----|------|-------------|
| 0   | b    | ID
| 1   | b    | param
| 2   | b    | color
| 3   | b    | under ID
| 4   | b    | under param
| 5   | b    | under color

Special notes: signs, scrolls, sensors, players, and IDs >=128 will be
replaced with spaces when an MZM is loaded. Robots in a board MZM require
extra storage information (see Robot Data).


## Data (layer)

The MZM data is stored in the following format if the "layer" storage
mode is selected. The data is composed of (width * height) blocks, where
each block is 2 bytes and contains the following:

| pos | size | description |
|-----|------|-------------|
| 0   | b    | char
| 1   | b    | color


## Robot data

MZMs using the "board" storage mode may additionally include a robot
data block if the MZM board data contains robots. This block will always
be located after the board data, and the format of this block is based
on the MZX world format corresponding to the world version indicated in
the header.

### MegaZeux 2.84X

The robots in this block are saved in the either the world robot format
or the save robot format from MegaZeux 2.84.

### MegaZeux 2.90+

The robot data block is a ZIP archive containing MegaZeux 2.90 robot
info files named in the format `r##`, where `##` is a hexadecimal number
between 1 and 255 (ex: `r01`, `r02`, ... `rFF`). The robot info file
with a given number corresponds to the robot in the board data with the
same number as its param.
