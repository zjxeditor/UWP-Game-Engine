//=============================================================================
// SsaoBlur.fx by Frank Luna (C) 2011 All Rights Reserved.
//
// Performs a bilateral edge preserving blur of the ambient map.  We use 
// a pixel shader instead of compute shader to avoid the switch from 
// compute mode to rendering mode.  The texture cache makes up for some of the
// loss of not having shared memory.  The ambient map uses 16-bit texture
// format, which is small, so we should be able to fit a lot of texels
// in the cache.
//=============================================================================


#ifndef HORIZONTAL
#define HORIZONTAL 0
#endif

cbuffer cbTexelSize : register(b0)
{
	float gTexelWidth;
	float gTexelHeight;
};

static const float gWeights[11] =
{
	0.05f, 0.05f, 0.1f, 0.1f, 0.1f, 0.2f, 0.1f, 0.1f, 0.1f, 0.05f, 0.05f
};
static const int gBlurRadius = 5;

Texture2D gInputImage : register(t0);
Texture2D gNormalDepthMap : register(t1);
SamplerState sampleFilter : register(s0);		// Often use min-mag-linear-mip-point.

struct PixelIn
{
	float4 PosH : SV_POSITION;
	float2 Tex  : TEXCOORD;
};

float4 main(PixelIn pin) : SV_Target
{
	float2 texOffset;
#if HORIZONTAL==1		// HorizontalBlur
	texOffset = float2(gTexelWidth, 0.0f);
#else					// VerticalBlur
	texOffset = float2(0.0f, gTexelHeight);
#endif

	// The center value always contributes to the sum.
	float4 color = gWeights[5] * gInputImage.SampleLevel(sampleFilter, pin.Tex, 0.0);
	float totalWeight = gWeights[5];

	float4 centerNormalDepth = gNormalDepthMap.SampleLevel(sampleFilter, pin.Tex, 0.0f);

	for (float i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		// We already added in the center weight.
		if (i == 0)
			continue;

		float2 tex = pin.Tex + i*texOffset;

		float4 neighborNormalDepth = gNormalDepthMap.SampleLevel(
			sampleFilter, tex, 0.0f);

		//
		// If the center value and neighbor values differ too much (either in 
		// normal or depth), then we assume we are sampling across a discontinuity.
		// We discard such samples from the blur.
		//

		if (dot(neighborNormalDepth.xyz, centerNormalDepth.xyz) >= 0.8f &&
			abs(neighborNormalDepth.a - centerNormalDepth.a) <= 0.2f)
		{
			float weight = gWeights[i + gBlurRadius];

			// Add neighbor pixel to blur.
			color += weight*gInputImage.SampleLevel(
				sampleFilter, tex, 0.0);

			totalWeight += weight;
		}
	}

	// Compensate for discarded samples by making total weights sum to 1.
	return color / totalWeight;
}