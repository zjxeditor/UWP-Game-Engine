#include "../ShaderInclude.hlsl"

cbuffer cbSettings : register(b1)
{
	// for when the emit position/direction is varying
	float3 gEmitPosW;
	float pad;
	float3 gEmitDirW;
	uint gTexNum;

	// Net constant acceleration used to accerlate the particles.
	float3 gAccelW;
};

// Array of textures for texturing the particles.
Texture1D gRandomTex : register(t0);
Texture2DArray gTexArray : register(t1);
SamplerState sampleFilter : register(s0);