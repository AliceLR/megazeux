#!/usr/bin/env python3

# MegaZeux
#
# Copyright (C) 2020 Adrian Siekierka <asiekierka@gmail.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

from PIL import Image
import struct, sys

COLORS = [
	(0, 0, 0),
	(0, 0, 170),
	(0, 170, 0),
	(0, 170, 170),
	(170, 0, 0),
	(170, 0, 170),
	(170, 85, 0),
	(170, 170, 170),
	(85, 85, 85),
	(85, 85, 255),
	(85, 255, 85),
	(85, 255, 255),
	(255, 85, 85),
	(255, 85, 255),
	(255, 255, 85),
	(255, 255, 255)
]

# overlay size (16) + solid palette size (16) + blends size ((16+1)*8)
BLENDING_LENGTH = int((15+1)*15/2)
PALETTE_OFFSET = 16 + 16 + BLENDING_LENGTH
PALETTE_LEN = 256 - PALETTE_OFFSET
PALETTE_SOLID_LEN = 16
PALETTE_BLEND_LEN = PALETTE_LEN - 16
IDX_TABLE = [PALETTE_OFFSET] * 256

def xy2idx(x, y):
	return (y * 16) + x

def blend(c1, c2):
	return (
		(c1[0] + c2[0]) >> 1,
		(c1[1] + c2[1]) >> 1,
		(c1[2] + c2[2]) >> 1
	)

def col2nds(c):
	cr = (c[0] >> 3)
	cg = (c[1] >> 3)
	cb = (c[2] >> 3)
	return (1 << 15) | (cb << 10) | (cg << 5) | cr

# populate IDX_TABLE with solid colors
for a in range(0, 16):
	IDX_TABLE[xy2idx(a, a)] = PALETTE_OFFSET + a

# generate texture with all blending colors
i = 0
blend_tex = Image.new('RGB', (BLENDING_LENGTH, 1))

for a in range(0, 16):
	for b in range(a + 1, 16):
		blend_tex.putpixel((i, 0), blend(COLORS[a], COLORS[b]))
		i += 1
print("%d -> %d colors (expected %d)" % (i, PALETTE_BLEND_LEN, BLENDING_LENGTH))
assert i == BLENDING_LENGTH

# palettize texture
#blend_tex = blend_tex.convert('P', dither=Image.NONE, palette=Image.ADAPTIVE, colors=PALETTE_BLEND_LEN)
blend_tex = blend_tex.quantize(colors=PALETTE_BLEND_LEN, method=Image.MAXCOVERAGE, dither=Image.NONE)

# populate IDX_TABLE
i = 0
for a in range(0, 16):
	for b in range(a + 1, 16):
		pal_idx = PALETTE_OFFSET + 16 + blend_tex.getpixel((i, 0))
		IDX_TABLE[xy2idx(a, b)] = pal_idx
		IDX_TABLE[xy2idx(b, a)] = pal_idx
		i += 1

# generate binary data
with open(sys.argv[1], "wb") as fp:
	# write IDX_TABLE
	for i in range(0, 256):
		fp.write(struct.pack("<B", IDX_TABLE[i]))
	# write palette data
	for i in range(0, 16):
		fp.write(struct.pack("<H", col2nds(COLORS[i])))
	blend_pal = blend_tex.getpalette()
	for i in range(0, PALETTE_BLEND_LEN):
		fp.write(struct.pack("<H", col2nds(blend_pal[i*3:(i+1)*3])))
