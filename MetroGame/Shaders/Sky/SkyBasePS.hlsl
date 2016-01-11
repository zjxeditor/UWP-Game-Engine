TextureCube gCubeMap : register(t0);
SamplerState sampleFilter : register(s0);	// Oftn use linear

struct PixelIn
{
	float4 PosH : SV_POSITION;
	float3 PosL : POSITION;
};

float4 main(PixelIn pin) : SV_TARGET
{
	return gCubeMap.Sample(sampleFilter, pin.PosL);
}
