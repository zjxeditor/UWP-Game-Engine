// Only used for clip.

Texture2D gDiffuseMap : register(t0);
SamplerState sampleFilter : register(s0);		// Often use linear.

struct PixelIn
{
	float4 PosH : SV_POSITION;
	float2 Tex  : TEXCOORD;
};

// This is only used for alpha cut out geometry, so that shadows 
// show up correctly.  Geometry that does not need to sample a
// texture can use a NULL pixel shader for depth pass.
void main(PixelIn pin)
{
	float4 diffuse = gDiffuseMap.Sample(sampleFilter, pin.Tex);

	// Don't write transparent pixels to the shadow map.
	clip(diffuse.a - 0.15f);
}