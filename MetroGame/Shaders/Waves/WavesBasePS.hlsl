#include "../ShaderInclude.hlsl"

#ifndef LIGHT_COUNT
#define LIGHT_COUNT 0
#endif

#ifndef FOG_ENABLE
#define FOG_ENABLE 0
#endif

#ifndef TEX_ENABLE
#define TEX_ENABLE 0
#endif

cbuffer cbPerObject : register(b1)
{
	float4x4 gWorld;
	float4x4 gWorldInvTranspose;
	float4x4 gTexTransform;
	Material gMaterial;
};

Texture2D gDiffuseMap : register(t0);
SamplerState sampleFilter : register(s0);	// Often use Anisotropic.

struct PixelIn
{
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float2 Tex     : TEXCOORD;
	float4 PosH    : SV_POSITION;
};


float4 main(PixelIn pin) : SV_Target
{
	// Interpolating normal can unnormalize it, so normalize it.
	pin.NormalW = normalize(pin.NormalW);

	// The toEye vector is used in lighting.
	float3 toEye = gEyePosW - pin.PosW;

	// Cache the distance to the eye from this surface point.
	float distToEye = length(toEye);

	// Normalize.
	toEye /= distToEye;

	// Default to multiplicative identity.
	float4 texColor = float4(1, 1, 1, 1);
#if TEX_ENABLE == 1
	// Sample texture.
	texColor = gDiffuseMap.Sample(sampleFilter, pin.Tex);
#endif

	// Lighting.
	float4 litColor = texColor;
#if LIGHT_COUNT > 0
	// Start with a sum of zero.
	float4 ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 spec = float4(0.0f, 0.0f, 0.0f, 0.0f);
	// Sum the light contribution from each light source.  

	[unroll]
	for (int i = 0; i < LIGHT_COUNT; ++i)
	{
		float4 A, D, S;
		ComputeDirectionalLight(gMaterial, gDirLights[i], pin.NormalW, toEye,
			A, D, S);

		ambient += A;
		diffuse += D;
		spec += S;
	}

	// Modulate with late add.
	litColor = texColor*(ambient + diffuse) + spec;
#endif

	// Fogging
#if FOG_ENABLE == 1
		float fogLerp = saturate((distToEye - gFogStart) / gFogRange);

		// Blend the fog color and the lit color.
		litColor = lerp(litColor, gFogColor, fogLerp);
#endif

	// Common to take alpha from diffuse material and texture.
	litColor.a = gMaterial.Diffuse.a * texColor.a;

	return litColor;
}