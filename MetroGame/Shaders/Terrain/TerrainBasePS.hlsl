#ifndef LIGHT_COUNT
#define LIGHT_COUNT 0
#endif

#ifndef FOG_ENABLE
#define FOG_ENABLE 0
#endif

#ifndef TEX_ENABLE
#define TEX_ENABLE 0
#endif

#include "../ShaderInclude.hlsl"

cbuffer cbPerObject : register(b1)
{
	float4x4 gWorld;
	float4x4 gWorldInvTranspose;
	float4x4 gTexTransform;
	Material gMaterial;
};

cbuffer cbTerrainSettings  : register(b2)
{
	float2 gTexScale;
	// When distance is minimum, the tessellation is maximum.
	// When distance is maximum, the tessellation is minimum.
	float gMinDist;
	float gMaxDist;

	// Exponents for power of 2 tessellation.  The tessellation
	// range is [2^(gMinTess), 2^(gMaxTess)].  Since the maximum
	// tessellation is 64, this means gMaxTess can be at most 6
	// since 2^6 = 64.
	float gMinTess;
	float gMaxTess;

	float gTexelCellSpaceU;
	float gTexelCellSpaceV;
	float gWorldCellSpace;
};

Texture2DArray gLayerMapArray : register(t0);
Texture2D gBlendMap : register(t1);
Texture2D gHeightMap : register(t2);

SamplerState sampleFilterHeight : register(s0);
SamplerState sampleFilter : register(s1);

struct PixelIn
{
	float4 PosH     : SV_POSITION;
	float3 PosW     : POSITION;
	float2 Tex      : TEXCOORD0;
	float2 TiledTex : TEXCOORD1;
};

float4 main(PixelIn pin) : SV_TARGET
{
	//
	// Estimate normal and tangent using central differences.
	//
	float2 leftTex = pin.Tex + float2(-gTexelCellSpaceU, 0.0f);
	float2 rightTex = pin.Tex + float2(gTexelCellSpaceU, 0.0f);
	float2 bottomTex = pin.Tex + float2(0.0f, gTexelCellSpaceV);
	float2 topTex = pin.Tex + float2(0.0f, -gTexelCellSpaceV);

	float leftY = gHeightMap.SampleLevel(sampleFilterHeight, leftTex, 0).r;
	float rightY = gHeightMap.SampleLevel(sampleFilterHeight, rightTex, 0).r;
	float bottomY = gHeightMap.SampleLevel(sampleFilterHeight, bottomTex, 0).r;
	float topY = gHeightMap.SampleLevel(sampleFilterHeight, topTex, 0).r;

	float3 tangent = normalize(float3(2.0f*gWorldCellSpace, rightY - leftY, 0.0f));
	float3 bitan = normalize(float3(0.0f, bottomY - topY, -2.0f*gWorldCellSpace));
	float3 normalW = cross(tangent, bitan);

	// The toEye vector is used in lighting.
	float3 toEye = gEyePosW - pin.PosW;

	// Cache the distance to the eye from this surface point.
	float distToEye = length(toEye);

	// Normalize.
	toEye /= distToEye;
	
    // Sample texture.
	float4 texColor = float4(1, 1, 1, 1);

	// Textureing
#if TEX_ENABLE==1
	// Sample layers in texture array.
	float4 c0 = gLayerMapArray.Sample(sampleFilter, float3(pin.TiledTex, 0.0f));
	float4 c1 = gLayerMapArray.Sample(sampleFilter, float3(pin.TiledTex, 1.0f));
	float4 c2 = gLayerMapArray.Sample(sampleFilter, float3(pin.TiledTex, 2.0f));
	float4 c3 = gLayerMapArray.Sample(sampleFilter, float3(pin.TiledTex, 3.0f));
	float4 c4 = gLayerMapArray.Sample(sampleFilter, float3(pin.TiledTex, 4.0f));

	// Sample the blend map.
	float4 t = gBlendMap.Sample(sampleFilter, pin.Tex);

	// Blend the layers on top of each other.
	texColor = c0;
	texColor = lerp(texColor, c1, t.r);
	texColor = lerp(texColor, c2, t.g);
	texColor = lerp(texColor, c3, t.b);
	texColor = lerp(texColor, c4, t.a);
#endif
		
	// Lighting.
	float4 litColor = texColor;
#if LIGHT_COUNT>0 
	// Start with a sum of zero. 
	float4 ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 spec    = float4(0.0f, 0.0f, 0.0f, 0.0f);

	// Sum the light contribution from each light source.  
	[unroll]
	for(int i = 0; i < LIGHT_COUNT; ++i)
	{
		float4 A, D, S;
		ComputeDirectionalLight(gMaterial, gDirLights[i], normalW, toEye,
			A, D, S);

		ambient += A;
		diffuse += D;
		spec    += S;
	}

	// Modulate with late add.
	litColor = texColor*(ambient + diffuse) + spec;
#endif

	// Fogging
#if FOG_ENABLE==1
	float fogLerp = saturate( (distToEye - gFogStart) / gFogRange ); 

	// Blend the fog color and the lit color.
	litColor = lerp(litColor, gFogColor, fogLerp);
#endif

	// Common to take alpha from diffuse material and texture.
	litColor.a = gMaterial.Diffuse.a * texColor.a;

    return litColor;
}
