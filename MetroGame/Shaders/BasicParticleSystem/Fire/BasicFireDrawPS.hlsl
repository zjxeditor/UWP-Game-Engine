#include "../BasicCommonDrawPS.hlsl"

struct PixelIn
{
	float4 PosH		: SV_Position;
	float4 Color	: COLOR;
	float2 Tex		: TEXCOORD;
};

float4 main(PixelIn pin) : SV_TARGET
{
	float v = gRandomTex.SampleLevel(sampleFilter, gGameTime, 0).x;
	uint index = (uint)(abs(v)*gTexNum);
	[flatten]
	if (index == gTexNum) --index;

	return gTexArray.Sample(sampleFilter, float3(pin.Tex, index))*pin.Color;
}