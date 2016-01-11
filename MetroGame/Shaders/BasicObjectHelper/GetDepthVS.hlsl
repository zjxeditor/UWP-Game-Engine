#ifndef TESS_ENABLE
#define TESS_ENABLE 0
#endif

#ifndef SKINNED_ENABLE
#define SKINNED_ENABLE 0
#endif

#include "../ShaderInclude.hlsl"

cbuffer cbPerObject : register(b1)
{
	float4x4 gWorld;
	float4x4 gWorldInvTranspose;
	float4x4 gTexTransform;
	Material gMaterial;
}; 

#if TESS_ENABLE==1
cbuffer cbTessSettings  : register(b2)
{
	float gHeightScale;
	float gMaxTessDistance;
	float gMinTessDistance;
	float gMinTessFactor;
	float gMaxTessFactor;
};
#endif

#if SKINNED_ENABLE==1
cbuffer cbSkinned : register(b3)
{
	float4x4 gBoneTransforms[96];
};
#endif

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 Tex     : TEXCOORD;
#if SKINNED_ENABLE==1
	float3 TangentL   : TANGENT;
	float3 Weights    : WEIGHTS;
	uint4 BoneIndices : BONEINDICES;
#endif
};

#if TESS_ENABLE==1
struct VertexOut
{
	float3 PosW       : POSITION;
	float3 NormalW    : NORMAL;
	float2 Tex        : TEXCOORD;
	float  TessFactor : TESS;
};
#else
struct VertexOut
{
	float4 PosH : SV_POSITION;
	float2 Tex  : TEXCOORD;
};
#endif

#if TESS_ENABLE==1
VertexOut main(VertexIn vin)
{
	VertexOut vout;

	vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
	vout.NormalW = mul(vin.NormalL, (float3x3)gWorldInvTranspose);
	vout.Tex = mul(float4(vin.Tex, 0.0f, 1.0f), gTexTransform).xy;

	float d = distance(vout.PosW, gEyePosW);

	// Normalized tessellation factor. 
	// The tessellation is 
	//   0 if d >= gMinTessDistance and
	//   1 if d <= gMaxTessDistance.  
	float tess = saturate((gMinTessDistance - d) / (gMinTessDistance - gMaxTessDistance));

	// Rescale [0,1] --> [gMinTessFactor, gMaxTessFactor].
	vout.TessFactor = gMinTessFactor + tess*(gMaxTessFactor - gMinTessFactor);

	return vout;
}
#elif SKINNED_ENABLE==1
VertexOut main(VertexIn vin)
{
	VertexOut vout;
	// Init array or else we get strange warnings about SV_POSITION.
	float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	weights[0] = vin.Weights.x;
	weights[1] = vin.Weights.y;
	weights[2] = vin.Weights.z;
	weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

	float3 posL = float3(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < 4; ++i)
	{
		// Assume no nonuniform scaling when transforming normals, so 
		// that we do not have to use the inverse-transpose.
		posL += weights[i] * mul(float4(vin.PosL, 1.0f), gBoneTransforms[vin.BoneIndices[i]]).xyz;
	}

	// Transform to world space space.
	float3 posW = mul(float4(posL, 1.0f), gWorld).xyz;
	// Transform to homogeneous clip space.
	vout.PosH = mul(float4(posW, 1.0f), gViewProj);
	// Output vertex attributes for interpolation across triangle.
	vout.Tex = mul(float4(vin.Tex, 0.0f, 1.0f), gTexTransform).xy;

	return vout;
}
#else
VertexOut main(VertexIn vin)
{
	VertexOut vout;

	// Transform to world space space.
	float3 posW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;

	// Transform to homogeneous clip space.
	vout.PosH = mul(float4(posW, 1.0f), gViewProj);

	// Output vertex attributes for interpolation across triangle.
	vout.Tex = mul(float4(vin.Tex, 0.0f, 1.0f), gTexTransform).xy;

	return vout;
}
#endif


