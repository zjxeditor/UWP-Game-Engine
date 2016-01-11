cbuffer cbSettings : register(b0)
{
	// for when the emit position/direction is varying
	float3 gEmitPosW;
	float pad;
	float3 gEmitDirW;
	uint gTexNum;

	// Net constant acceleration used to accerlate the particles.
	float3 gAccelW;
};


struct VertexIn
{
	float3 InitialPosW	: POSITION;
	float3 InitialVelW	: VELOCITY;
	float2 SizeW		: SIZE;
	float Age			: AGE;
	uint Type			: TYPE;
};
