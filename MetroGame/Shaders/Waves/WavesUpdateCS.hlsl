// UpdateWavesCS(): Solves 2D wave equation using the compute shader.

// For updating the simulation.
cbuffer cbSimSettings : register(b0)
{
	float3 gWaveConstants;
};

Texture2D gPrevSolInput : register(t0);
Texture2D gCurrSolInput : register(t1);
RWTexture2D<float> gNextSolOutput : register(u0);

[numthreads(16, 16, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
	// We do not need to do bounds checking because:
	//	 *out-of-bounds reads return 0, which works for us--it just means the boundary of 
	//    our water simulation is clamped to 0 in local space.
	//   *out-of-bounds writes are a no-op.

	int x = dispatchThreadID.x;
	int y = dispatchThreadID.y;

	gNextSolOutput[int2(x, y)] =
		gWaveConstants[0] * gPrevSolInput[int2(x, y)].r +
		gWaveConstants[1] * gCurrSolInput[int2(x, y)].r +
		gWaveConstants[2] * (
			gCurrSolInput[int2(x, y + 1)].r +
			gCurrSolInput[int2(x, y - 1)].r +
			gCurrSolInput[int2(x + 1, y)].r +
			gCurrSolInput[int2(x - 1, y)].r);

	/*
	// Equivalently using SampleLevel() instead of operator [].
	int x = dispatchThreadID.x;
	int y = dispatchThreadID.y;

	float2 c = float2(x,y)/512.0f;
	float2 t = float2(x,y-1)/512.0;
	float2 b = float2(x,y+1)/512.0;
	float2 l = float2(x-1,y)/512.0;
	float2 r = float2(x+1,y)/512.0;

	gNextSolOutput[int2(x,y)] =
	gWaveConstants[0]*gPrevSolInput.SampleLevel(samPoint, c, 0.0f).r +
	gWaveConstants[1]*gCurrSolInput.SampleLevel(samPoint, c, 0.0f).r +
	gWaveConstants[2]*(
	gCurrSolInput.SampleLevel(samPoint, b, 0.0f).r +
	gCurrSolInput.SampleLevel(samPoint, t, 0.0f).r +
	gCurrSolInput.SampleLevel(samPoint, r, 0.0f).r +
	gCurrSolInput.SampleLevel(samPoint, l, 0.0f).r);*/
}




