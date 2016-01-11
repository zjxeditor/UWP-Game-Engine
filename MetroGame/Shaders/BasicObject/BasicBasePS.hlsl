// Note the specific ps shader should be named in this pattern: 
// "BasicPS11111311.hlsl", the digit is according to texture, clip,
// normal, shadow, ssao, light count, reflect and fog features.

#ifndef TEX_ENABLE
#define TEX_ENABLE 0
#endif

#ifndef CLIP_ENABLE
#define CLIP_ENABLE 0
#endif

#ifndef NORMAL_ENABLE
#define NORMAL_ENABLE 0
#endif

#ifndef SHADOW_ENABLE
#define SHADOW_ENABLE 0
#endif

#ifndef SSAO_ENABLE
#define SSAO_ENABLE 0
#endif

#ifndef LIGHT_COUNT
#define LIGHT_COUNT 0
#endif

#ifndef REFLECT_ENABLE
#define REFLECT_ENABLE 0
#endif

#ifndef FOG_ENABLE
#define FOG_ENABLE 0
#endif


#include "../ShaderInclude.hlsl"

cbuffer cbPerObject : register(b1)
{
	float4x4 gWorld;
	float4x4 gWorldInvTranspose;
	float4x4 gTexTransform;
	Material gMaterial;
}; 

// Nonnumeric values cannot be added to a cbuffer.
#if TEX_ENABLE==1
Texture2D gDiffuseMap	: register(t0);
#endif
#if TEX_ENABLE==1 && NORMAL_ENABLE==1
Texture2D gNormalMap	: register(t1);
#endif
#if SHADOW_ENABLE==1
Texture2D gDepthMap	: register(t2);
#endif
#if SSAO_ENABLE==1
Texture2D gSsaoMap		: register(t3);
#endif
#if REFLECT_ENABLE==1
TextureCube gCubeMap	: register(t4);
#endif

#if TEX_ENABLE==1 || SSAO_ENABLE==1 || REFLECT_ENABLE==1
SamplerState sampleFilter				: register(s0);		// Often use linear sampler.
#endif
#if SHADOW_ENABLE==1
SamplerComparisonState compSampleFilter : register(s1);		// Often use less. Only for shadow mapping.
#endif

struct PixelIn
{
	float4 PosH       : SV_POSITION;
	float3 PosW       : POSITION;
	float3 NormalW    : NORMAL;
#if NORMAL_ENABLE==1
	float3 TangentW : TANGENT;
#endif
	float2 Tex        : TEXCOORD0;
#if SHADOW_ENABLE==1
	float4 ShadowPosH : TEXCOORD1;
#endif
#if SSAO_ENABLE==1
	float4 SsaoPosH   : TEXCOORD2;
#endif
};

float4 main(PixelIn pin) : SV_TARGET
{
	// Interpolating normal can unnormalize it, so normalize it.
    pin.NormalW = normalize(pin.NormalW);

	// The toEye vector is used in lighting.
	float3 toEye = gEyePosW - pin.PosW;

	// Cache the distance to the eye from this surface point.
	float distToEye = length(toEye);

	// Normalize.
	toEye /= distToEye;
	
    // Sample texture.
	float4 texColor = float4(1, 1, 1, 1);

#if TEX_ENABLE==1
	// Sample the texture
	texColor = gDiffuseMap.Sample( sampleFilter, pin.Tex );

#if CLIP_ENABLE==1    
	// Discard pixel if texture alpha < 0.1.  Note that we do this
	// test as soon as possible so that we can potentially exit the shader 
	// early, thereby skipping the rest of the shader code.
	clip(texColor.a - 0.1f);
#endif
#endif
		
	float3 normalW = pin.NormalW;
#if TEX_ENABLE==1 && NORMAL_ENABLE==1
	// Normal mapping.
	float3 normalMapSample = gNormalMap.Sample(sampleFilter, pin.Tex).rgb;
	normalW = NormalSampleToWorldSpace(normalMapSample, pin.NormalW, pin.TangentW);
#endif

	// Lighting.
	float4 litColor = texColor;
#if LIGHT_COUNT>0 
	// Start with a sum of zero. 
	float4 ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 spec    = float4(0.0f, 0.0f, 0.0f, 0.0f);

	// Only the first light casts a shadow.
	float3 shadow = float3(1.0f, 1.0f, 1.0f);
#if SHADOW_ENABLE==1
	shadow[0] = CalcShadowFactor(compSampleFilter, gDepthMap, pin.ShadowPosH);
#endif
	float ambientAccess = 1.0f;
#if SSAO_ENABLE==1
	// Finish texture projection and sample SSAO map.
	pin.SsaoPosH /= pin.SsaoPosH.w;
	ambientAccess = gSsaoMap.SampleLevel(sampleFilter, pin.SsaoPosH.xy, 0.0f).r;
#endif

	// Sum the light contribution from each light source.  
	[unroll]
	for(int i = 0; i < LIGHT_COUNT; ++i)
	{
		float4 A, D, S;
		ComputeDirectionalLight(gMaterial, gDirLights[i], normalW, toEye, 
			A, D, S);

		ambient += ambientAccess*A;
		diffuse += shadow[i]*D;
		spec += shadow[i]*S;
	}

	// Modulate with late add.
	litColor = texColor*(ambient + diffuse) + spec;
#endif

	// Reflection
#if REFLECT_ENABLE==1
	float3 incident = -toEye;
	float3 reflectionVector = reflect(incident, normalW);
	float4 reflectionColor = gCubeMap.Sample(sampleFilter, reflectionVector);

	litColor += gMaterial.Reflect*reflectionColor;
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
