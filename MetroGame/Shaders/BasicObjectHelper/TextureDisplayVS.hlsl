cbuffer cbWorld : register(b0)
{
	float4x4 gWorld;
}; 

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 Tex     : TEXCOORD;
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float2 Tex  : TEXCOORD;
};

VertexOut main(VertexIn vin)
{
    VertexOut vout;
	
	vout.PosH = mul(float4(vin.PosL, 1.0f), gWorld);
	vout.Tex = vin.Tex;

	return vout;
}
