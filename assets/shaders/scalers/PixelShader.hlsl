Texture2D<uint> charset_texture : register(t0);
Texture2D<uint2> layers_texture : register(t1);
Texture2D<float4> palette_texture : register(t2);

struct VSOut
{
	float4 pos : SV_POSITION;
	float2 tc : TEXCOORD0;
	int transparent : TEXCOORD1;
	int charOffset : TEXCOORD2;
	int layer_id : TEXCOORD3;
	int layer_w : TEXCOORD4;
	int smzx : TEXCOORD5;
};

float4 main(VSOut vs) : SV_TARGET
{
	uint2 tc_int = uint2(vs.tc);
	uint2 layer_tc = uint2(tc_int.x / 8, tc_int.y / 14);
	uint2 pos_in_char = uint2(tc_int.x % 8, tc_int.y % 14);
	uint2 tile_and_color = layers_texture.Load(int3(layer_tc.y * vs.layer_w + layer_tc.x, vs.layer_id, 0));
	int char = tile_and_color.x;
	if (char == 65535) {
		discard;
	}

	char += vs.charOffset;

	uint row_idx = char * 14 + pos_in_char.y;
	uint row = charset_texture.Load(int3(row_idx % (64 * 14), row_idx / (64 * 14), 0));

	uint color_fg;
	uint color_bg; 

	int color_idx;
	if (!vs.smzx) {
		color_fg = (tile_and_color.y >> 8) & 0x0F | (((tile_and_color.y >> 8) & 0x10) << 4);
		color_bg = (tile_and_color.y >> 0) & 0x0F | (((tile_and_color.y >> 0) & 0x10) << 4);

		uint is_foreground = (row >> (7 - pos_in_char.x)) & 1;
		color_idx = is_foreground ? color_fg : color_bg;
	}
	else {
		color_fg = (tile_and_color.y >> 8) & 0x0F;
		color_bg = (tile_and_color.y >> 0) & 0x0F;

		uint pix_idx = (row >> (6 - (pos_in_char.x & 0x6))) & 3;
		uint idx_idx = ((color_bg & 15) * 16) + (color_fg & 15);
		color_idx = int(palette_texture.Load(int3(idx_idx, 1, 0))[pix_idx] * 255.5f);
	}
	
	if (color_idx == vs.transparent) {
		discard;
	}

	float4 color = palette_texture.Load(int3(color_idx, 0, 0));
	return color;
}