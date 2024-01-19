Texture2D texture_maps[2] : register(t1);

SamplerState s1 : register(s0);

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	// return interpolated color
    return diffuse.Sample(s1, input.texCoord);
}