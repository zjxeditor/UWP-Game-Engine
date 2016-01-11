#include "../ShaderInclude.hlsl"

cbuffer cbPerObject : register(b1)
{
	float4x4 gWorld;
	float4x4 gWorldInvTranspose;
	float4x4 gTexTransform;
	Material gMaterial;
};

cbuffer cbRareChanged : register(b2)
{
	float  gGridSpatialStep;
	float2 gDisplacementMapTexelSize;
}

Texture2D gDisplacementMap : register(t0);
SamplerState sampleFilter : register(s0);		// Often use point.

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 Tex     : TEXCOORD;
};

struct VertexOut
{
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float2 Tex     : TEXCOORD;
	float4 PosH    : SV_POSITION;
};

VertexOut main( VertexIn vin )
{
	VertexOut vout;

	// Sample the displacement map using non-transformed [0,1]^2 tex-coords.
	vin.PosL.y = gDisplacementMap.SampleLevel(sampleFilter, vin.Tex, 0.0f).r;

	// Estimate normal using finite difference.
	float du = gDisplacementMapTexelSize.x;
	float dv = gDisplacementMapTexelSize.y;
	float l = gDisplacementMap.SampleLevel(sampleFilter, vin.Tex - float2(du, 0.0f), 0.0f).r;
	float r = gDisplacementMap.SampleLevel(sampleFilter, vin.Tex + float2(du, 0.0f), 0.0f).r;
	float t = gDisplacementMap.SampleLevel(sampleFilter, vin.Tex - float2(0.0f, dv), 0.0f).r;
	float b = gDisplacementMap.SampleLevel(sampleFilter, vin.Tex + float2(0.0f, dv), 0.0f).r;
	vin.NormalL = normalize(float3(-r + l, 2.0f*gGridSpatialStep, b - t));

	// Transform to world space space.
	vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
	vout.NormalW = mul(vin.NormalL, (float3x3)gWorldInvTranspose);

	// Transform to homogeneous clip space.
	vout.PosH = mul(float4(vout.PosW, 1.0f), gViewProj);

	// Output vertex attributes for interpolation across triangle.
	vout.Tex = mul(float4(vin.Tex, 0.0f, 1.0f), gTexTransform).xy;

	return vout;
}