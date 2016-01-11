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
