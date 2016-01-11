#ifndef RED_CHANNEL
#define RED_CHANNEL 0
#endif

#ifndef GREEN_CHANNEL
#define GREEN_CHANNEL 0
#endif

#ifndef BLUE_CHANNEL
#define BLUE_CHANNEL 0
#endif

#ifndef ALPHA_CHANNEL
#define ALPHA_CHANNEL 0
#endif

Texture2D gDiffuseMap : register(t0);
SamplerState sampleFilter : register(s0);		// Often use linear.

struct PixelIn
{
	float4 PosH : SV_POSITION;
	float2 Tex  : TEXCOORD;
};

float4 main(PixelIn pin) : SV_Target
{
	float4 c = gDiffuseMap.Sample(sampleFilter, pin.Tex);

#if RED_CHANNEL==1
	return float4(c.rrr, 1.0);
#elif GREEN_CHANNEL==1
	return float4(c.ggg, 1.0);
#elif BLUE_CHANNEL==1
	return float4(c.bbb, 1.0);
#elif ALPHA_CHANNEL==1
	return float4(c.aaa, 1.0);
#else
	return c;
#endif
}