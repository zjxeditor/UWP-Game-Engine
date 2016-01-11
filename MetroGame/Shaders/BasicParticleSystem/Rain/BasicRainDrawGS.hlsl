#include "../BasicCommonDrawGS.hlsl"

struct GeoIn
{
	float3 PosW  : POSITION;
	uint   Type  : TYPE;
};

struct GeoOut
{
	float4 PosH  : SV_Position;
	float2 Tex   : TEXCOORD;
};

// The draw GS just expands points into lines.
[maxvertexcount(2)]
void main(point GeoIn gin[1],
	inout LineStream<GeoOut> lineStream)
{
	// do not draw emitter particles.
	if (gin[0].Type != PT_EMITTER)
	{
		// Slant line in acceleration direction.
		float3 p0 = gin[0].PosW;
		float3 p1 = gin[0].PosW + 0.5f*gAccelW;

		GeoOut v0;
		v0.PosH = mul(float4(p0, 1.0f), gViewProj);
		v0.Tex = float2(0.0f, 0.0f);
		lineStream.Append(v0);

		GeoOut v1;
		v1.PosH = mul(float4(p1, 1.0f), gViewProj);
		v1.Tex = float2(1.0f, 1.0f);
		lineStream.Append(v1);
	}
}
