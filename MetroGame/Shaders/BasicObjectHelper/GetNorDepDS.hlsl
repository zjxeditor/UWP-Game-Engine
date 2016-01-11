// Only used for tess.

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
	float3 PosW     : POSITIONW;
	float3 PosV     : POSITION;
	float3 NormalV  : NORMAL;
	float2 Tex      : TEXCOORD;
};

struct DomainOut
{
	float4 PosH       : SV_POSITION;
	float3 PosV       : POSITION;
	float3 NormalV    : NORMAL;
	float2 Tex        : TEXCOORD;
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
	float3 posW = bary.x*tri[0].PosW + bary.y*tri[1].PosW + bary.z*tri[2].PosW;
	dout.PosV = bary.x*tri[0].PosV + bary.y*tri[1].PosV + bary.z*tri[2].PosV;
	dout.NormalV = bary.x*tri[0].NormalV + bary.y*tri[1].NormalV + bary.z*tri[2].NormalV;
	dout.Tex = bary.x*tri[0].Tex + bary.y*tri[1].Tex + bary.z*tri[2].Tex;

	// Interpolating normal can unnormalize it, so normalize it.
	dout.NormalV = normalize(dout.NormalV);

	//
	// Displacement mapping.
	//

	// Choose the mipmap level based on distance to the eye; specifically, choose
	// the next miplevel every MipInterval units, and clamp the miplevel in [0,6].
	const float MipInterval = 20.0f;
	float mipLevel = clamp((distance(posW, gEyePosW) - MipInterval) / MipInterval, 0.0f, 6.0f);

	// Sample height map (stored in alpha channel).
	float h = gNormalMap.SampleLevel(sampleFilter, dout.Tex, mipLevel).a;

	// Offset vertex along normal.
	dout.PosV += (gHeightScale*(h - 1.0))*dout.NormalV;

	// Project to homogeneous clip space.
	dout.PosH = mul(float4(dout.PosV, 1.0f), gProj);

	return dout;
}