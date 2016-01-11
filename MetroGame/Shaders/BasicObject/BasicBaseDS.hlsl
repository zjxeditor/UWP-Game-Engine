// Only used for normal-displacement mapping.
// Note the specific ds shader should be named in this pattern: 
// "BasicDS11.hlsl", the digit is according to shadow and ssao features.

#ifndef SHADOW_ENABLE
#define SHADOW_ENABLE 0
#endif

#ifndef SSAO_ENABLE
#define SSAO_ENABLE 0
#endif

#include "../ShaderInclude.hlsl"

cbuffer cbTessSettings  : register(b1)
{
	float gHeightScale;
	float gMaxTessDistance;
	float gMinTessDistance;
	float gMinTessFactor;
	float gMaxTessFactor;
};

Texture2D gNormalMap : register(t0);
SamplerState sampleFilter : register(s0);	// Often use linear.

struct DomainIn
{
	float3 PosW     : POSITION;
	float3 NormalW  : NORMAL;
	float3 TangentW : TANGENT;
	float2 Tex      : TEXCOORD;
};

struct DomainOut
{
	float4 PosH     : SV_POSITION;
	float3 PosW     : POSITION;
	float3 NormalW  : NORMAL;
	float3 TangentW : TANGENT;
	float2 Tex      : TEXCOORD0;
#if SHADOW_ENABLE==1
	float4 ShadowPosH : TEXCOORD1;
#endif
#if SSAO_ENABLE==1
	float4 SsaoPosH   : TEXCOORD2;
#endif
};

// Output patch constant data.
struct PatchTess
{
	float EdgeTess[3] : SV_TessFactor;
	float InsideTess : SV_InsideTessFactor;
};

// The domain shader is called for every vertex created by the tessellator.  
// It is like the vertex shader after tessellation.
[domain("tri")]
DomainOut main(PatchTess patchTess,
	float3 bary : SV_DomainLocation,
	const OutputPatch<DomainIn, 3> tri)
{
	DomainOut dout;

	// Interpolate patch attributes to generated vertices.
	dout.PosW = bary.x*tri[0].PosW + bary.y*tri[1].PosW + bary.z*tri[2].PosW;
	dout.NormalW = bary.x*tri[0].NormalW + bary.y*tri[1].NormalW + bary.z*tri[2].NormalW;
	dout.TangentW = bary.x*tri[0].TangentW + bary.y*tri[1].TangentW + bary.z*tri[2].TangentW;
	dout.Tex = bary.x*tri[0].Tex + bary.y*tri[1].Tex + bary.z*tri[2].Tex;

	// Interpolating normal can unnormalize it, so normalize it.
	dout.NormalW = normalize(dout.NormalW);

	//
	// Displacement mapping.
	//

	// Choose the mipmap level based on distance to the eye; specifically, choose
	// the next miplevel every MipInterval units, and clamp the miplevel in [0,6].
	const float MipInterval = 20.0f;
	float mipLevel = clamp((distance(dout.PosW, gEyePosW) - MipInterval) / MipInterval, 0.0f, 6.0f);

	// Sample height map (stored in alpha channel).
	float h = gNormalMap.SampleLevel(sampleFilter, dout.Tex, mipLevel).a;

	// Offset vertex along normal.
	dout.PosW += (gHeightScale*(h - 1.0))*dout.NormalW;

	// Project to homogeneous clip space.
	dout.PosH = mul(float4(dout.PosW, 1.0f), gViewProj);

#if SHADOW_ENABLE==1
	// Generate projective tex-coords to project shadow map onto scene.
	dout.ShadowPosH = mul(float4(dout.PosW, 1.0f), mul(gLightProj, gTex));
#endif
#if SSAO_ENABLE==1
	// Generate projective tex-coords to project SSAO map onto scene.
	dout.SsaoPosH = mul(float4(dout.PosW, 1.0f), mul(gViewProj, gTex));
#endif

	return dout;
}