
struct struct_layer
{
	int x;
	int y;
	uint w8_h8_z16;
	uint charOffset16_transparentColor10smzx6;
};

cbuffer layer_info : register(b0)
{
	struct_layer layer[512];
};

struct VSIn
{
	uint vertex_id : SV_VertexID;
};

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

VSOut main(VSIn In)
{
	VSOut Out;

	uint layer_id = In.vertex_id / 6;
	uint tri_i = In.vertex_id % 6;
	float2 vertex_id_pos = float2(tri_i == 1 || tri_i == 3 || tri_i == 4, tri_i == 2 || tri_i == 4 || tri_i == 5);

	float w = float((layer[layer_id].w8_h8_z16 >> 0) & 0xFF);
	float h = float((layer[layer_id].w8_h8_z16 >> 8) & 0xFF);
	float z = float((layer[layer_id].w8_h8_z16 >> 16) & 0xFFFF) / float(0xFFFF);

	Out.layer_w = w;

	float2 pos_in_layer = float2(8.0f, 14.0f) * float2(w, h) * vertex_id_pos;
	float2 pos_on_screen = float2(layer[layer_id].x, layer[layer_id].y);
	float2 pos_full_res = float2(pos_in_layer + pos_on_screen);

	Out.layer_id = layer_id;
	Out.tc = pos_in_layer;
	Out.transparent = (layer[layer_id].charOffset16_transparentColor10smzx6 >> 16) & 0x3FF;
	Out.charOffset = (layer[layer_id].charOffset16_transparentColor10smzx6 >> 0) & 0xFFFF;
	if (Out.charOffset > 0x7FFF)
	{
		Out.charOffset = 0x10000 - Out.charOffset;
	}
	Out.smzx = (layer[layer_id].charOffset16_transparentColor10smzx6 >> 26) & 0x3F;

	Out.pos = float4(
		(pos_full_res.x * 2.0f / 640.0f) - 1.0f,
		(pos_full_res.y * -2.0f / 350.0f) + 1.0f,
		z,
		1.0f);

	//Out.pos = float4(vertex_id_pos,
	//	0.0f,
	//	1.0f);

	return Out;
}