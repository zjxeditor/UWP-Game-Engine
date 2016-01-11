#ifndef ALPHA_CLIP
#define ALPHA_CLIP 0
#endif

#if ALPHA_CLIP==1
Texture2D gDiffuseMap : register(t0);
SamplerState sampleFilter : register(s0);		// Often use linear.
#endif

struct PixelIn
{
	float4 PosH       : SV_POSITION;
	float3 PosV       : POSITION;
	float3 NormalV    : NORMAL;
	float2 Tex        : TEXCOORD0;
};

float4 main(PixelIn pin) : SV_Target
{
	// Interpolating normal can unnormalize it, so normalize it.
	pin.NormalV = normalize(pin.NormalV);

#if ALPHA_CLIP==1
		float4 texColor = gDiffuseMap.Sample(sampleFilter, pin.Tex);
		clip(texColor.a - 0.1f);
#endif

	return float4(pin.NormalV, pin.PosV.z);
}