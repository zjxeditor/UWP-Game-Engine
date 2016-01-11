#define PT_EMITTER 0
#define PT_FLARE 1

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

// Random texture used to generate random numbers in shaders.
Texture1D gRandomTex : register(t0);
SamplerState sampleFilter : register(s0);


//***********************************************
// HELPER FUNCTIONS                             *
//***********************************************
float3 RandUnitVec3(float offset)
{
	// Use game time plus offset to sample random texture.
	float u = (gGameTime + offset);

	// coordinates in [-1,1]
	float3 v = gRandomTex.SampleLevel(sampleFilter, u, 0).xyz;

	// project onto unit sphere
	return normalize(v);
}

float3 RandVec3(float offset)
{
	// Use game time plus offset to sample random texture.
	float u = (gGameTime + offset);

	// coordinates in [-1,1]
	float3 v = gRandomTex.SampleLevel(sampleFilter, u, 0).xyz;

	return v;
}

struct Particle
{
	float3 InitialPosW	: POSITION;
	float3 InitialVelW	: VELOCITY;
	float2 SizeW		: SIZE;
	float  Age			: AGE;
	uint   Type			: TYPE;
};
