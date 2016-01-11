#include "../BasicCommonDrawVS.hlsl"

struct VertexOut
{
	float3 PosW  : POSITION;
	uint   Type  : TYPE;
};

VertexOut main(VertexIn vin)
{
	VertexOut vout;

	float t = vin.Age;

	// constant acceleration equation
	vout.PosW = 0.5f*t*t*gAccelW + t*vin.InitialVelW + vin.InitialPosW;

	vout.Type = vin.Type;

	return vout;
}


