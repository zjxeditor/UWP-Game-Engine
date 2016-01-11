// Only used for tess.

struct HullIn
{
	float3 PosW       : POSITIONW;
	float3 PosV       : POSITION;
	float3 NormalV    : NORMAL;
	float2 Tex        : TEXCOORD;
	float  TessFactor : TESS;
};

struct HullOut
{
	float3 PosW     : POSITIONW;
	float3 PosV     : POSITION;
	float3 NormalV  : NORMAL;
	float2 Tex      : TEXCOORD;
};

struct PatchTess
{
	float EdgeTess[3] : SV_TessFactor;
	float InsideTess : SV_InsideTessFactor;
};

PatchTess PatchHS(InputPatch<HullIn, 3> patch,
	uint patchID : SV_PrimitiveID)
{
	PatchTess pt;

	// Average tess factors along edges, and pick an edge tess factor for 
	// the interior tessellation.  It is important to do the tess factor
	// calculation based on the edge properties so that edges shared by 
	// more than one triangle will have the same tessellation factor.  
	// Otherwise, gaps can appear.
	pt.EdgeTess[0] = 0.5f*(patch[1].TessFactor + patch[2].TessFactor);
	pt.EdgeTess[1] = 0.5f*(patch[2].TessFactor + patch[0].TessFactor);
	pt.EdgeTess[2] = 0.5f*(patch[0].TessFactor + patch[1].TessFactor);
	pt.InsideTess = pt.EdgeTess[0];

	return pt;
}

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("PatchHS")]
HullOut main(InputPatch<HullIn, 3> p,
	uint i : SV_OutputControlPointID,
	uint patchId : SV_PrimitiveID)
{
	HullOut hout;

	// Pass through shader.
	hout.PosW = p[i].PosW;
	hout.PosV = p[i].PosV;
	hout.NormalV = p[i].NormalV;
	hout.Tex = p[i].Tex;

	return hout;
}